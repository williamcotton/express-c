#include "resource.h"

static resource_instance_t *
createResourceInstance(resource_t *resource, model_instance_t *modelInstance,
                       jsonapi_params_t *params) {
  memory_manager_t *memoryManager = resource->model->memoryManager;

  resource_instance_t *instance =
      memoryManager->malloc(sizeof(resource_instance_t));

  instance->modelInstance = modelInstance;
  instance->id = modelInstance->id;
  instance->type = resource->type;

  instance->toJSONAPI = memoryManager->blockCopy(^json_t *() {
    UNUSED json_t *fields = json_object_get(params->query, "fields");
    json_t *attributes = json_object();

    for (int i = 0; i < resource->attributesCount; i++) {
      class_attribute_t *attribute = resource->attributes[i];
      json_t *value = NULL;
      char *attributeType = attribute->type;

      if (modelInstance->get(attribute->name) == NULL) {
        continue;
      }

      if (strcmp(attributeType, "integer") == 0 ||
          strcmp(attributeType, "integer_id") == 0) {
        value = json_integer(atoll(modelInstance->get(attribute->name)));
      } else if (strcmp(attributeType, "big_decimal") == 0 ||
                 strcmp(attributeType, "float") == 0) {
        char *eptr;
        value = json_real(strtod(modelInstance->get(attribute->name), &eptr));
      } else if (strcmp(attributeType, "boolean") == 0) {
        value = json_boolean(
            strcmp(modelInstance->get(attribute->name), "t") == 0 ||
            strcmp(modelInstance->get(attribute->name), "true") == 0);
      } else {
        value = json_string(modelInstance->get(attribute->name));
      }

      json_object_set(attributes, attribute->name, value);
      memoryManager->cleanup(memoryManager->blockCopy(^{
        json_decref(value);
      }));
    }

    // TODO: add relationships
    // TODO: add links

    json_t *data = json_pack("{s:s, s:s:, s:o}", "type", instance->type, "id",
                             instance->id, "attributes", attributes);

    return data;
  });

  return instance;
};

static resource_instance_collection_t *
createResourceInstanceCollection(resource_t *resource,
                                 model_instance_collection_t *modelCollection,
                                 jsonapi_params_t *params) {
  memory_manager_t *memoryManager = resource->model->memoryManager;

  resource_instance_collection_t *collection =
      (resource_instance_collection_t *)memoryManager->malloc(
          sizeof(resource_instance_collection_t));

  collection->arr = NULL;
  collection->size = 0;

  collection->arr = memoryManager->malloc(sizeof(resource_instance_t *) *
                                          modelCollection->size);
  collection->size = modelCollection->size;
  collection->data = modelCollection;

  for (size_t i = 0; i < collection->size; i++) {
    model_instance_t *modelInstance = modelCollection->arr[i];
    resource_instance_t *resourceInstance =
        createResourceInstance(resource, modelInstance, params);
    collection->arr[i] = resourceInstance;
  }

  collection->at = memoryManager->blockCopy(^(size_t index) {
    return collection->arr[index];
  });

  collection->each =
      memoryManager->blockCopy(^(eachResourceInstanceCallback callback) {
        for (size_t i = 0; i < collection->size; i++) {
          callback(collection->arr[i]);
        }
      });

  collection->filter =
      memoryManager->blockCopy(^(filterResourceInstanceCallback callback) {
        resource_instance_collection_t *filteredCollection =
            createResourceInstanceCollection(resource, modelCollection, params);
        for (size_t i = 0; i < collection->size; i++) {
          if (callback(collection->arr[i])) {
            filteredCollection->arr[filteredCollection->size++] =
                collection->arr[i];
          }
        }
        return filteredCollection;
      });

  collection->find =
      memoryManager->blockCopy(^(findResourceInstanceCallback callback) {
        for (size_t i = 0; i < collection->size; i++) {
          if (callback(collection->arr[i])) {
            return collection->arr[i];
          }
        }
        return (resource_instance_t *)NULL;
      });

  collection->eachWithIndex = memoryManager->blockCopy(
      ^(eachResourceInstanceWithIndexCallback callback) {
        for (size_t i = 0; i < collection->size; i++) {
          callback(collection->arr[i], i);
        }
      });

  collection->reduce = memoryManager->blockCopy(
      ^(void *accumulator, reducerResourceInstanceCallback callback) {
        for (size_t i = 0; i < collection->size; i++) {
          accumulator = callback(accumulator, collection->arr[i]);
        }
        return accumulator;
      });

  collection->map =
      memoryManager->blockCopy(^(mapResourceInstanceCallback callback) {
        void **map =
            (void **)memoryManager->malloc(sizeof(void *) * collection->size);
        for (size_t i = 0; i < collection->size; i++) {
          map[i] = callback(collection->arr[i]);
        }
        return map;
      });

  collection->toJSONAPI = memoryManager->blockCopy(^json_t *() {
    json_t *data = json_array();
    collection->each(^(resource_instance_t *instance) {
      json_array_append_new(data, instance->toJSONAPI());
    });

    // TODO: add meta
    // TODO: add links
    // TODO: add included
    // TODO: add errors

    __block json_t *response = json_pack("{s:o}", "data", data);

    memoryManager->cleanup(memoryManager->blockCopy(^{
      json_decref(response);
    }));

    return response;
  });

  return collection;
};

static query_t *applyFiltersToScope(UNUSED json_t *filters, query_t *baseScope,
                                    UNUSED resource_t *resource) {
  const char *attribute = NULL;
  json_t *operatorValues;
  json_object_foreach(filters, attribute, operatorValues) {
    check(operatorValues != NULL, "operatorValues is NULL");
    const char *oper = NULL;
    json_t *valueArray;
    json_object_foreach(operatorValues, oper, valueArray) {
      check(valueArray != NULL, "valueArray is NULL");
      int count = (int)json_array_size(valueArray);
      const char **values = (const char **)malloc(sizeof(char *) * count);
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

static query_t *applySortersToScope(json_t *sorters, query_t *baseScope,
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

static query_t *applyPaginatorToScope(json_t *paginator, query_t *baseScope,
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

static query_t *applyFieldsToScope(UNUSED json_t *fields, query_t *baseScope,
                                   UNUSED resource_t *resource) {

  char *selectConditions = malloc(1);
  selectConditions[0] = '\0';
  const char *resourceType = NULL;
  json_t *fieldValues;
  json_object_foreach(fields, resourceType, fieldValues) {
    check(resourceType != NULL, "Invalid fields: resource type is required");
    size_t index;
    json_t *jsonValue;
    json_array_foreach(fieldValues, index, jsonValue) {
      const char *value = json_string_value(jsonValue);
      check(value != NULL,
            "Invalid fields: fields must be an array of strings");
      char *selectCondition =
          malloc(strlen(resourceType) + strlen(".") + strlen(value) + 1);
      sprintf(selectCondition, "%s.%s", resourceType, value);
      if (strlen(selectConditions) > 0) {
        size_t newLen = strlen(selectConditions) + strlen(selectCondition) + 2;
        selectConditions = realloc(selectConditions, newLen);
        strncat(selectConditions, ",", 1);
        strncat(selectConditions, selectCondition, newLen);
      } else {
        selectConditions =
            realloc(selectConditions, strlen(selectCondition) + 1);
        selectConditions = selectCondition;
      }
    }
  }
  baseScope = baseScope->select(selectConditions);
error:
  return baseScope;
}

static query_t *applyQueryToScope(json_t *query, query_t *baseScope,
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

  return baseScope;
}

resource_t *CreateResource(char *type, model_t *model) {
  memory_manager_t *memoryManager = model->memoryManager;
  resource_t *resource = memoryManager->malloc(sizeof(resource_t));

  /* Global resource store */
  static int resourceCount = 0;
  static resource_t *resources[1000]; // TODO: fix this
  resources[resourceCount] = resource;
  resourceCount++;

  resource->memoryManager = model->memoryManager;
  resource->type = type;
  resource->model = model;

  resource->attributesCount = 0;
  resource->belongsToCount = 0;
  resource->hasManyCount = 0;
  resource->filtersCount = 0;
  resource->sortersCount = 0;
  resource->statsCount = 0;
  resource->beforeSaveCallbacksCount = 0;
  resource->afterSaveCallbacksCount = 0;
  resource->beforeDestroyCallbacksCount = 0;
  resource->afterDestroyCallbacksCount = 0;
  resource->beforeUpdateCallbacksCount = 0;
  resource->afterUpdateCallbacksCount = 0;
  resource->beforeCreateCallbacksCount = 0;
  resource->afterCreateCallbacksCount = 0;

  resource->lookup = resource->memoryManager->blockCopy(^(char *lookupType) {
    for (int i = 0; i < resourceCount; i++) {
      if (strcmp(resources[i]->type, lookupType) == 0) {
        return resources[i];
      }
    }
    return (resource_t *)NULL;
  });

  resource->belongsTo =
      memoryManager->blockCopy(^(char *relatedResourceName, char *foreignKey) {
        belongs_to_resource_t *newBelongsTo =
            memoryManager->malloc(sizeof(belongs_to_resource_t));
        newBelongsTo->resourceName = relatedResourceName;
        newBelongsTo->foreignKey = foreignKey;
        resource->belongsToRelationships[resource->belongsToCount] =
            newBelongsTo;
        resource->belongsToCount++;
      });

  resource->hasMany =
      memoryManager->blockCopy(^(char *relatedResourceName, char *foreignKey) {
        has_many_resource_t *newHasMany =
            memoryManager->malloc(sizeof(has_many_resource_t));
        newHasMany->resourceName = relatedResourceName;
        newHasMany->foreignKey = foreignKey;
        resource->hasManyRelationships[resource->hasManyCount] = newHasMany;
        resource->hasManyCount++;
      });

  resource->filter = memoryManager->blockCopy(
      ^(char *attribute, char *operator, filterCallback callback) {
        resource_filter_t *newFilter =
            memoryManager->malloc(sizeof(resource_filter_t));
        newFilter->attribute = attribute;
        newFilter->operator= operator;
        newFilter->callback = callback;
        resource->filters[resource->filtersCount] = newFilter;
        resource->filtersCount++;
      });

  resource->sort =
      memoryManager->blockCopy(^(char *attribute, sortCallback callback) {
        resource_sort_t *newSort =
            memoryManager->malloc(sizeof(resource_sort_t));
        newSort->attribute = attribute;
        newSort->callback = callback;
        resource->sorters[resource->sortersCount] = newSort;
        resource->sortersCount++;
      });

  resource->resolve = memoryManager->blockCopy(^(resolveCallback callback) {
    resource_resolve_t *newResolve =
        memoryManager->malloc(sizeof(resource_resolve_t));
    newResolve->callback = callback;
    resource->resolver = newResolve;
  });

  resource->paginate = memoryManager->blockCopy(^(paginateCallback callback) {
    resource_paginate_t *newPaginate =
        memoryManager->malloc(sizeof(resource_paginate_t));
    newPaginate->callback = callback;
    resource->paginator = newPaginate;
  });

  resource->baseScope = memoryManager->blockCopy(^(baseScopeCallback callback) {
    resource_base_scope_t *newBaseScope =
        memoryManager->malloc(sizeof(resource_base_scope_t));
    newBaseScope->callback = callback;
    resource->baseScoper = newBaseScope;
  });

  resource->attribute = memoryManager->blockCopy(^(char *attributeName,
                                                   char *attributeType) {
    class_attribute_t *newAttribute =
        memoryManager->malloc(sizeof(class_attribute_t));
    newAttribute->name = attributeName;
    newAttribute->type = attributeType;
    resource->attributes[resource->attributesCount] = newAttribute;
    resource->attributesCount++;

    if (strcmp(attributeType, "integer") == 0 ||
        strcmp(attributeType, "integer_id") == 0 ||
        strcmp(attributeType, "big_decimal") == 0 ||
        strcmp(attributeType, "float") == 0 ||
        strcmp(attributeType, "date") == 0 ||
        strcmp(attributeType, "datetime") == 0) {

      resource->filter(
          attributeName, "gt",
          memoryManager->blockCopy(^(query_t *scope, const char **values,
                                     UNUSED int count) {
            const char *value = values[0];
            char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                         strlen(" > $") + 1);
            sprintf(whereCondition, "%s > $", attributeName);
            return scope->where(whereCondition, value);
          }));

      resource->filter(
          attributeName, "gte",
          memoryManager->blockCopy(^(query_t *scope, const char **values,
                                     UNUSED int count) {
            const char *value = values[0];
            char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                         strlen(" >= $") + 1);
            sprintf(whereCondition, "%s >= $", attributeName);
            return scope->where(whereCondition, value);
          }));

      resource->filter(
          attributeName, "lt",
          memoryManager->blockCopy(^(query_t *scope, const char **values,
                                     UNUSED int count) {
            const char *value = values[0];
            char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                         strlen(" < $") + 1);
            sprintf(whereCondition, "%s < $", attributeName);
            return scope->where(whereCondition, value);
          }));

      resource->filter(
          attributeName, "lte",
          memoryManager->blockCopy(^(query_t *scope, const char **values,
                                     UNUSED int count) {
            const char *value = values[0];
            char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                         strlen(" <= $") + 1);
            sprintf(whereCondition, "%s <= $", attributeName);
            return scope->where(whereCondition, value);
          }));
    }

    if (strcmp(attributeType, "enum") == 0 ||
        strcmp(attributeType, "uuid") == 0 ||
        strcmp(attributeType, "integer") == 0 ||
        strcmp(attributeType, "integer_id") == 0 ||
        strcmp(attributeType, "big_decimal") == 0 ||
        strcmp(attributeType, "float") == 0 ||
        strcmp(attributeType, "date") == 0 ||
        strcmp(attributeType, "boolean") == 0 ||
        strcmp(attributeType, "datetime") == 0) {

      resource->filter(attributeName, "eq",
                       memoryManager->blockCopy(
                           ^(query_t *scope, const char **values, int count) {
                             if (count == 1) {
                               char *whereCondition = memoryManager->malloc(
                                   strlen(attributeName) + strlen(" = $") + 1);
                               sprintf(whereCondition, "%s = $", attributeName);
                               return scope->where(whereCondition, values[0]);
                             }
                             return scope->whereIn(attributeName, true, values,
                                                   count);
                           }));

      resource->filter(
          attributeName, "not_eq",
          memoryManager->blockCopy(
              ^(query_t *scope, const char **values, int count) {
                if (count == 1) {
                  char *whereCondition = memoryManager->malloc(
                      strlen(attributeName) + strlen(" NOT = $") + 1);
                  sprintf(whereCondition, "NOT %s = $", attributeName);
                  return scope->where(whereCondition, values[0]);
                }
                return scope->whereIn(attributeName, false, values, count);
              }));
    }

    if (strcmp(attributeType, "string") == 0) {
      resource->filter(
          attributeName, "eq",
          memoryManager->blockCopy(^(query_t *scope, const char **values,
                                     int count) {
            if (count == 1) {
              char *whereCondition = memoryManager->malloc(
                  strlen(attributeName) + strlen("LOWER() = LOWER($)") + 1);
              sprintf(whereCondition, "LOWER(%s) = LOWER($)", attributeName);
              return scope->where(whereCondition, values[0]);
            }
            // TODO: support LOWER
            return scope->whereIn(attributeName, true, values, count);
          }));

      resource->filter(
          attributeName, "not_eq",
          memoryManager->blockCopy(^(query_t *scope, const char **values,
                                     int count) {
            if (count == 1) {
              char *whereCondition = memoryManager->malloc(
                  strlen(attributeName) + strlen("NOT LOWER() = LOWER($)") + 1);
              sprintf(whereCondition, "NOT LOWER(%s) = LOWER($)",
                      attributeName);
              return scope->where(whereCondition, values[0]);
            }
            // TODO: support LOWER
            return scope->whereIn(attributeName, false, values, count);
          }));

      resource->filter(attributeName, "eql",
                       memoryManager->blockCopy(
                           ^(query_t *scope, const char **values, int count) {
                             if (count == 1) {
                               char *whereCondition = memoryManager->malloc(
                                   strlen(attributeName) + strlen(" = $") + 1);
                               sprintf(whereCondition, "%s = $", attributeName);
                               return scope->where(whereCondition, values[0]);
                             }
                             return scope->whereIn(attributeName, true, values,
                                                   count);
                           }));

      resource->filter(
          attributeName, "not_eql",
          memoryManager->blockCopy(
              ^(query_t *scope, const char **values, int count) {
                if (count == 1) {
                  char *whereCondition = memoryManager->malloc(
                      strlen(attributeName) + strlen(" NOT = $") + 1);
                  sprintf(whereCondition, "NOT %s = $", attributeName);
                  return scope->where(whereCondition, values[0]);
                }
                return scope->whereIn(attributeName, false, values, count);
              }));

      resource->filter(
          attributeName, "match",
          memoryManager->blockCopy(^(query_t *scope, const char **values,
                                     UNUSED int count) {
            const char *value = values[0];
            char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                         strlen(" LIKE $") + 1);
            sprintf(whereCondition, "%s LIKE $", attributeName);
            size_t matchValueLen = strlen(value) + strlen("%%") + 1;
            char *matchValue = memoryManager->malloc(matchValueLen);
            snprintf(matchValue, matchValueLen, "%%%s%%", value);
            return scope->where(whereCondition, matchValue);
          }));

      resource->filter(
          attributeName, "not_match",
          memoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = memoryManager->malloc(
                    strlen(attributeName) + strlen(" NOT LIKE $") + 1);
                sprintf(whereCondition, "%s NOT LIKE $", attributeName);
                size_t matchValueLen = strlen(value) + strlen("%%") + 1;
                char *matchValue = memoryManager->malloc(matchValueLen);
                snprintf(matchValue, matchValueLen, "%%%s%%", value);
                return scope->where(whereCondition, matchValue);
              }));

      resource->filter(
          attributeName, "prefix",
          memoryManager->blockCopy(^(query_t *scope, const char **values,
                                     UNUSED int count) {
            const char *value = values[0];
            char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                         strlen(" LIKE $") + 1);
            sprintf(whereCondition, "%s LIKE $", attributeName);
            size_t matchValueLen = strlen(value) + strlen("%") + 1;
            char *matchValue = memoryManager->malloc(matchValueLen);
            snprintf(matchValue, matchValueLen, "%s%%", value);
            return scope->where(whereCondition, matchValue);
          }));

      resource->filter(
          attributeName, "not_prefix",
          memoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = memoryManager->malloc(
                    strlen(attributeName) + strlen(" NOT LIKE $") + 1);
                sprintf(whereCondition, "%s NOT LIKE $", attributeName);
                size_t matchValueLen = strlen(value) + strlen("%") + 1;
                char *matchValue = memoryManager->malloc(matchValueLen);
                snprintf(matchValue, matchValueLen, "%s%%", value);
                return scope->where(whereCondition, matchValue);
              }));

      resource->filter(
          attributeName, "suffix",
          memoryManager->blockCopy(^(query_t *scope, const char **values,
                                     UNUSED int count) {
            const char *value = values[0];
            char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                         strlen(" LIKE $") + 1);
            sprintf(whereCondition, "%s LIKE $", attributeName);
            size_t matchValueLen = strlen(value) + strlen("%") + 1;
            char *matchValue = memoryManager->malloc(matchValueLen);
            snprintf(matchValue, matchValueLen, "%%%s", value);
            return scope->where(whereCondition, matchValue);
          }));

      resource->filter(
          attributeName, "not_suffix",
          memoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = memoryManager->malloc(
                    strlen(attributeName) + strlen(" NOT LIKE $") + 1);
                sprintf(whereCondition, "%s NOT LIKE $", attributeName);
                size_t matchValueLen = strlen(value) + strlen("%") + 1;
                char *matchValue = memoryManager->malloc(matchValueLen);
                snprintf(matchValue, matchValueLen, "%%%s", value);
                return scope->where(whereCondition, matchValue);
              }));
    }

    resource->sort(attributeName, memoryManager->blockCopy(^(
                                      query_t *scope, const char *direction) {
      return scope->order(attributeName, (char *)direction);
    }));
  });

  resource->sort("id", ^(query_t *scope, const char *direction) {
    return scope->order("id", (char *)direction);
  });

  resource->baseScope(^(model_t *baseModel) {
    return baseModel->query();
  });

  resource->paginate(^(query_t *scope, int page, int perPage) {
    return scope->limit(perPage)->offset((page - 1) * perPage);
  });

  resource->resolve(^(query_t *scope) {
    return (model_instance_collection_t *)scope->all();
  });

  resource->all = memoryManager->blockCopy(^(jsonapi_params_t *params) {
    query_t *baseScope = resource->baseScoper->callback(resource->model);

    query_t *queriedScope =
        applyQueryToScope(params->query, baseScope, resource);

    model_instance_collection_t *modelCollection =
        resource->resolver->callback(queriedScope);

    resource_instance_collection_t *collection =
        createResourceInstanceCollection(resource, modelCollection, params);

    return collection;
  });

  resource->find =
      memoryManager->blockCopy(^(jsonapi_params_t *params, char *id) {
        query_t *baseScope = resource->baseScoper->callback(resource->model);

        baseScope = baseScope->where("id = $", id);

        query_t *queriedScope =
            applyQueryToScope(params->query, baseScope, resource);

        model_instance_collection_t *modelCollection =
            resource->resolver->callback(queriedScope);

        resource_instance_collection_t *collection =
            createResourceInstanceCollection(resource, modelCollection, params);

        resource_instance_t *instance = collection->at(0);

        json_t *data = instance->toJSONAPI();

        instance->toJSONAPI = memoryManager->blockCopy(^json_t *() {
          // TODO: add included

          __block json_t *response = json_pack("{s:o}", "data", data);

          memoryManager->cleanup(memoryManager->blockCopy(^{
            json_decref(response);
          }));

          return response;
        });

        return instance;
      });

  return resource;
}
