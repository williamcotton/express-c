#include "resource.h"

query_t *applyFiltersToScope(json_t *filters, query_t *scope,
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
          scope = filter->callback(scope, values, count);
        }
      }
    }
  }
error:
  return scope;
}

query_t *applySortersToScope(json_t *sorters, query_t *scope,
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
        scope = sorter->callback(scope, direction);
      }
    }
  }
error:
  return scope;
}

query_t *applyPaginatorToScope(json_t *paginator, query_t *scope,
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

  scope = resource->paginator->callback(scope, atoi(page), atoi(perPage));
error:
  return scope;
}

query_t *applyResourceAttributeToScope(query_t *scope, resource_t *resource,
                                       char *attributeName) {
  char *selectCondition = resource->model->instanceMemoryManager->malloc(
      strlen(resource->type) + strlen(".") + strlen(attributeName) + 1);
  sprintf(selectCondition, "%s.%s", resource->type, attributeName);
  scope = scope->select(selectCondition);
  return scope;
}

query_t *applyAllFieldsToScope(query_t *scope, resource_t *resource) {
  applyResourceAttributeToScope(scope, resource, "id");
  for (int i = 0; i < resource->attributesCount; i++) {
    class_attribute_t *attribute = resource->attributes[i];
    applyResourceAttributeToScope(scope, resource, attribute->name);
  }
  for (int i = 0; i < resource->belongsToModelCount; i++) {
    belongs_to_t *belongsToModel = resource->belongsToModelRelationships[i];
    applyResourceAttributeToScope(scope, resource, belongsToModel->foreignKey);
  }
  return scope;
}

query_t *applyFieldsToScope(json_t *fields, query_t *scope,
                            resource_t *resource) {
  const char *resourceType = NULL;
  json_t *fieldValues;
  check(json_is_object(fields), "Invalid fields: fields must be an object");
  json_object_foreach(fields, resourceType, fieldValues) {
    check(resourceType != NULL, "Invalid fields: resource type is required");
    resource_t *fieldsResource = resource->lookup(resourceType);
    check(fieldsResource != NULL, "Invalid fields: resource type is invalid");
    if (fieldsResource != resource) {
      goto error;
    }
    size_t index;
    json_t *jsonValue;
    json_array_foreach(fieldValues, index, jsonValue) {
      const char *value = json_string_value(jsonValue);
      check(value != NULL,
            "Invalid fields: fields must be an array of strings");
      applyResourceAttributeToScope(scope, fieldsResource, (char *)value);
    }
  }
error:
  return scope;
}

query_t *applyIncludeToScope(json_t *include, query_t *scope,
                             resource_t *resource) {

  /* Loop through the array of included resources and build up an array of
   * resource types */
  int count = (int)json_array_size(include);
  const char **includedResources =
      resource->model->instanceMemoryManager->malloc(sizeof(char *) * count);
  size_t index;
  json_t *includedResource;
  json_array_foreach(include, index, includedResource) {
    includedResources[index] = json_string_value(includedResource);
  }

  /* Apply the included resources to the scope */
  scope = scope->includes(includedResources, count);
  return scope;
};

query_t *applyQueryToScope(json_t *query, query_t *scope,
                           resource_t *resource) {
  json_t *filters = json_object_get(query, "filter");
  if (filters) {
    scope = applyFiltersToScope(filters, scope, resource);
  }

  json_t *sorters = json_object_get(query, "sort");
  if (sorters) {
    scope = applySortersToScope(sorters, scope, resource);
  }

  json_t *paginator = json_object_get(query, "page");
  if (paginator) {
    scope = applyPaginatorToScope(paginator, scope, resource);
  }

  json_t *fields = json_object_get(query, "fields");
  if (fields) {
    if (json_object_get(fields, resource->type) == NULL) {
      scope = applyAllFieldsToScope(scope, resource);
    }
    scope = applyFieldsToScope(fields, scope, resource);
  }

  json_t *include = json_object_get(query, "include");
  if (include) {
    scope = applyIncludeToScope(include, scope, resource);
  }

  return scope;
}
