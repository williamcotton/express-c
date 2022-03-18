#include "resource.h"

static query_t *applyAttributeFilterOperatorToScope(const char *attribute,
                                                    const char *oper,
                                                    json_t *valueArray,
                                                    query_t *scope,
                                                    resource_t *resource) {
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
      break;
    }
  }
  return scope;
}

query_t *applyFiltersToScope(json_t *filters, query_t *scope,
                             resource_t *resource) {
  const char *attributesToDelete[100];
  int attributesToDeleteCount = 0;
  const char *checkAttribute = NULL;
  json_t *checkValues;
  json_object_foreach(filters, checkAttribute, checkValues) {
    if (strcmp(checkAttribute, resource->type) == 0) {
      json_t *value;
      const char *key = NULL;
      json_object_foreach(checkValues, key, value) {
        json_object_set(filters, key, value);
      }
      // TODO: json_object_del causes problems
      attributesToDelete[attributesToDeleteCount++] = checkAttribute;
    }
  }

  for (int i = 0; i < attributesToDeleteCount; i++) {
    json_object_del(filters, attributesToDelete[i]);
  }

  const char *attribute = NULL;
  json_t *operatorValues;
  json_object_foreach(filters, attribute, operatorValues) {
    check(operatorValues != NULL, "operatorValues is NULL");
    if (strcmp(attribute, resource->type) == 0) {
    }
    const char *oper = NULL;
    json_t *valueArray;
    /* check if it is an array */
    size_t shorthandCount = json_array_size(operatorValues);
    if (shorthandCount > 0) {
      oper = "eq";
      applyAttributeFilterOperatorToScope(attribute, oper, operatorValues,
                                          scope, resource);
    } else {
      json_object_foreach(operatorValues, oper, valueArray) {
        check(valueArray != NULL, "valueArray is NULL");
        applyAttributeFilterOperatorToScope(attribute, oper, valueArray, scope,
                                            resource);
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
    char *value = (char *)json_string_value(jsonValue);
    char *attribute = NULL;

    const char *direction = NULL;
    if (value[0] == '-') {
      attribute = value + 1;
      direction = "DESC";
    } else {
      attribute = value;
      direction = "ASC";
    }

    char *attributeName = NULL;
    size_t len = strlen(attribute) + 1;
    char *resourceName = resource->model->instanceMemoryManager->malloc(len);
    strncpy(resourceName, attribute, len);

    char *splitPoint = strstr(resourceName, ".");
    if (splitPoint != NULL) {
      attributeName = splitPoint + 1;
      resourceName[splitPoint - resourceName] = '\0';
    }

    if (attributeName && strcmp(resourceName, resource->type) == 0) {
      attribute = attributeName;
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

query_t *applyBelongsToFieldsToScope(query_t *scope, resource_t *resource) {
  for (int i = 0; i < resource->belongsToModelCount; i++) {
    belongs_to_t *belongsToModel = resource->belongsToModelRelationships[i];
    applyResourceAttributeToScope(scope, resource, belongsToModel->foreignKey);
  }
  return scope;
}

query_t *applyAllFieldsToScope(query_t *scope, resource_t *resource) {
  for (int i = 0; i < resource->attributesCount; i++) {
    class_attribute_t *attribute = resource->attributes[i];
    applyResourceAttributeToScope(scope, resource, attribute->name);
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

query_t *applyStatsToScope(UNUSED json_t *stats, UNUSED query_t *scope,
                           UNUSED resource_t *resource,
                           resource_stat_value_t **statsArray,
                           int *statsArrayCount) {

  resource_stat_value_t *statValue =
      resource->model->instanceMemoryManager->malloc(
          sizeof(resource_stat_value_t));

  statValue->attribute = NULL;
  statValue->stat = NULL;
  statValue->value = NULL;
  statValue->type = NULL;

  const char *key;
  json_t *value;
  json_object_foreach(stats, key, value) {
    char *attribute = (char *)key;
    json_t *statArray = value;
    char *stat = (char *)json_string_value(json_array_get(statArray, 0));
    if (stat == NULL) {
      if (strcmp(attribute, resource->type) != 0) {
        continue;
      }
      statValue->type = resource->type;
      const char *attrKey;
      json_t *attrStatArray;
      json_object_foreach(statArray, attrKey, attrStatArray) {
        stat = (char *)json_string_value(json_array_get(attrStatArray, 0));
        attribute = (char *)attrKey;
      }
    }
    if (strcmp(attribute, "total") == 0 & strcmp(stat, "count") == 0) {
      statValue->attribute = "total";
      statValue->stat = "count";
      int count = scope->count();
      statValue->value =
          resource->model->instanceMemoryManager->malloc(sizeof(int) * 2);
      sprintf(statValue->value, "%d", count);
    } else if (resource->hasAttribute(attribute)) {
      statValue->attribute = attribute;
      statValue->stat = stat;
      query_stat_result_t *result = scope->stat(attribute, stat);
      statValue->value = resource->model->instanceMemoryManager->malloc(
          strlen(result->value) + 1);
      sprintf(statValue->value, "%s", result->value);
    }
  }

  statsArray[*statsArrayCount] = statValue;

  (*statsArrayCount)++;

  return scope;
}

query_t *applyQueryToScope(json_t *query, query_t *scope, resource_t *resource,
                           resource_stat_value_t **statsArray,
                           int *statsArrayCount) {

  json_t *filters = json_object_get(query, "filter");
  if (filters) {
    scope = applyFiltersToScope(filters, scope, resource);
  }

  json_t *stats = json_object_get(query, "stats");
  if (stats) {
    scope =
        applyStatsToScope(stats, scope, resource, statsArray, statsArrayCount);
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
    applyResourceAttributeToScope(scope, resource, "id");
    if (json_object_get(fields, resource->type) == NULL) {
      scope = applyAllFieldsToScope(scope, resource);
    }
    scope = applyFieldsToScope(fields, scope, resource);
    scope = applyBelongsToFieldsToScope(scope, resource);
  }

  // json_t *include = json_object_get(query, "include");
  // if (include) {
  //   scope = applyIncludeToScope(include, scope, resource);
  // }

  return scope;
}
