#include "resource.h"

query_t *applyFiltersToScope(json_t *filters, query_t *baseScope,
                             resource_t *resource) {
  const char *attribute = NULL;
  json_t *operatorValues;
  json_object_foreach(filters, attribute, operatorValues) {
    check(operatorValues != NULL, "operatorValues is NULL");
    const char *oper = NULL;
    json_t *valueArray;
    json_object_foreach(operatorValues, oper, valueArray) {
      check(valueArray != NULL, "valueArray is NULL");
      int count = (int)json_array_size(valueArray);
      const char **values =
          (const char **)resource->model->instanceMemoryManager->malloc(
              sizeof(char *) * count);
      size_t index;
      json_t *jsonValue;
      json_array_foreach(valueArray, index, jsonValue) {
        values[index] = json_string_value(jsonValue);
      }
      for (int i = 0; i < resource->filtersCount; i++) {
        resource_filter_t *filter = resource->filters[i];
        if (strcmp(filter->attribute, attribute) == 0 &&
            strcmp(filter->operator, oper) == 0) {
          baseScope = filter->callback(baseScope, values, count);
        }
      }
    }
  }
error:
  return baseScope;
}

query_t *applySortersToScope(json_t *sorters, query_t *baseScope,
                             resource_t *resource) {
  size_t index;
  json_t *jsonValue;
  json_array_foreach(sorters, index, jsonValue) {
    check(jsonValue != NULL, "Invalid sorter");
    const char *value = json_string_value(jsonValue);
    const char *attribute = NULL;
    const char *direction = NULL;
    if (value[0] == '-') {
      attribute = value + 1;
      direction = "DESC";
    } else {
      attribute = value;
      direction = "ASC";
    }
    for (int i = 0; i < resource->sortersCount; i++) {
      resource_sort_t *sorter = resource->sorters[i];
      if (strcmp(sorter->attribute, attribute) == 0) {
        baseScope = sorter->callback(baseScope, direction);
      }
    }
  }
error:
  return baseScope;
}

query_t *applyPaginatorToScope(json_t *paginator, query_t *baseScope,
                               resource_t *resource) {
  json_t *numberObject = json_object_get(paginator, "number");
  check(numberObject != NULL, "Invalid paginator: number is required");
  json_t *sizeObject = json_object_get(paginator, "size");
  check(sizeObject != NULL, "Invalid paginator: size is required");

  json_t *numberArray = json_array_get(numberObject, 0);
  check(numberArray != NULL, "Invalid paginator: number must be an array");
  json_t *sizeArray = json_array_get(sizeObject, 0);
  check(sizeArray != NULL, "Invalid paginator: size must be an array");

  const char *page = json_string_value(numberArray);
  check(page != NULL, "Invalid paginator: number must be a string");
  const char *perPage = json_string_value(sizeArray);
  check(perPage != NULL, "Invalid paginator: size must be a string");

  baseScope =
      resource->paginator->callback(baseScope, atoi(page), atoi(perPage));
error:
  return baseScope;
}

static query_t *applyFieldsToScope(json_t *fields, query_t *baseScope,
                                   resource_t *resource) {
  char *selectConditions = NULL;
  const char *resourceType = NULL;
  json_t *fieldValues;
  check(json_is_object(fields), "Invalid fields: fields must be an object");
  json_object_foreach(fields, resourceType, fieldValues) {
    check(resourceType != NULL, "Invalid fields: resource type is required");
    // check if resource type is valid
    check(resource->lookup(resourceType) != NULL,
          "Invalid fields: resource type is invalid");
    size_t index;
    json_t *jsonValue;
    json_array_foreach(fieldValues, index, jsonValue) {
      const char *value = json_string_value(jsonValue);
      check(value != NULL,
            "Invalid fields: fields must be an array of strings");
      // TODO: check if value is a valid field on resource
      char *selectCondition =
          malloc(strlen(resourceType) + strlen(".") + strlen(value) + 1);
      sprintf(selectCondition, "%s.%s", resourceType, value);
      if (selectConditions == NULL) {
        selectConditions = malloc(strlen(selectCondition) + 1);
        size_t len = strlen(selectCondition);
        memcpy(selectConditions, selectCondition, len);
        selectConditions[len] = '\0';
      } else {
        size_t newLen = strlen(selectConditions) + strlen(selectCondition) + 2;
        selectConditions = realloc(selectConditions, newLen);
        strncat(selectConditions, ",", 1);
        strncat(selectConditions, selectCondition, newLen);
      }
      free(selectCondition);
    }
  }
  baseScope = baseScope->select(selectConditions);
error:
  resource->model->instanceMemoryManager->cleanup(
      resource->model->instanceMemoryManager->blockCopy(^{
        free(selectConditions);
      }));
  return baseScope;
}

query_t *applyIncludeToScope(json_t *include, query_t *baseScope,
                             resource_t *resource) {

  int count = (int)json_array_size(include);
  const char **includedResources =
      resource->model->instanceMemoryManager->malloc(sizeof(char *) * count);

  size_t index;
  json_t *includedResource;
  json_array_foreach(include, index, includedResource) {
    includedResources[index] = json_string_value(includedResource);
  }

  return baseScope = baseScope->includes(includedResources, count);
};

query_t *applyQueryToScope(json_t *query, query_t *baseScope,
                           resource_t *resource) {
  json_t *filters = json_object_get(query, "filter");
  if (filters) {
    baseScope = applyFiltersToScope(filters, baseScope, resource);
  }

  json_t *sorters = json_object_get(query, "sort");
  if (sorters) {
    baseScope = applySortersToScope(sorters, baseScope, resource);
  }

  json_t *paginator = json_object_get(query, "page");
  if (paginator) {
    baseScope = applyPaginatorToScope(paginator, baseScope, resource);
  }

  // TODO: only select fields that are in the resource
  json_t *fields = json_object_get(query, "fields");
  if (fields) {
    baseScope = applyFieldsToScope(fields, baseScope, resource);
  }

  json_t *include = json_object_get(query, "include");
  if (include) {
    baseScope = applyIncludeToScope(include, baseScope, resource);
  }

  return baseScope;
}
