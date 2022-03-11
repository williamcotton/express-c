#include "resource.h"

resource_t *CreateResource(char *type, model_t *model) {
  memory_manager_t *appMemoryManager = model->appMemoryManager;
  resource_t *resource = appMemoryManager->malloc(sizeof(resource_t));

  /* Global resource store */
  static int resourceCount = 0;
  static resource_t *resources[100];
  resources[resourceCount] = resource;
  resourceCount++;

  resource->type = type;
  resource->model = model;

  resource->attributesCount = 0;
  resource->belongsToCount = 0;
  resource->hasManyCount = 0;
  resource->relationshipsCount = 0;
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

  resource->lookup = appMemoryManager->blockCopy(^(const char *lookupType) {
    for (int i = 0; i < resourceCount; i++) {
      if (strcmp(resources[i]->type, lookupType) == 0) {
        return resources[i];
      }
    }
    return (resource_t *)NULL;
  });

  resource->lookupByModel =
      appMemoryManager->blockCopy(^(const char *lookupTableName) {
        for (int i = 0; i < resourceCount; i++) {
          if (strcmp(resources[i]->model->tableName, lookupTableName) == 0) {
            return resources[i];
          }
        }
        return (resource_t *)NULL;
      });

  resource->belongsTo = appMemoryManager->blockCopy(^(char *relatedResourceName,
                                                      char *foreignKey) {
    resource->relationshipsCount++;
    belongs_to_resource_t *newBelongsTo =
        appMemoryManager->malloc(sizeof(belongs_to_resource_t));
    newBelongsTo->resourceName = relatedResourceName;
    newBelongsTo->foreignKey = foreignKey;
    resource->belongsToRelationships[resource->belongsToCount] = newBelongsTo;
    resource->belongsToCount++;
  });

  resource->hasMany = appMemoryManager->blockCopy(
      ^(char *relatedResourceName, char *foreignKey) {
        resource->relationshipsCount++;
        has_many_resource_t *newHasMany =
            appMemoryManager->malloc(sizeof(has_many_resource_t));
        newHasMany->resourceName = relatedResourceName;
        newHasMany->foreignKey = foreignKey;
        resource->hasManyRelationships[resource->hasManyCount] = newHasMany;
        resource->hasManyCount++;
      });

  resource->relationshipNames = appMemoryManager->blockCopy(^() {
    char **relationshipNames = appMemoryManager->malloc(
        sizeof(char *) * (resource->belongsToCount + resource->hasManyCount));
    for (int i = 0; i < resource->belongsToCount; i++) {
      relationshipNames[i] = resource->belongsToRelationships[i]->resourceName;
    }
    for (int i = 0; i < resource->hasManyCount; i++) {
      relationshipNames[i + resource->belongsToCount] =
          resource->hasManyRelationships[i]->resourceName;
    }
    return relationshipNames;
  });

  resource->filter = appMemoryManager->blockCopy(
      ^(char *attribute, char *operator, filterCallback callback) {
        resource_filter_t *newFilter =
            appMemoryManager->malloc(sizeof(resource_filter_t));
        newFilter->attribute = attribute;
        newFilter->operator= operator;
        newFilter->callback = callback;
        resource->filters[resource->filtersCount] = newFilter;
        resource->filtersCount++;
      });

  resource->sort =
      appMemoryManager->blockCopy(^(char *attribute, sortCallback callback) {
        resource_sort_t *newSort =
            appMemoryManager->malloc(sizeof(resource_sort_t));
        newSort->attribute = attribute;
        newSort->callback = callback;
        resource->sorters[resource->sortersCount] = newSort;
        resource->sortersCount++;
      });

  resource->resolveAll =
      appMemoryManager->blockCopy(^(resolveAllCallback callback) {
        resource_resolve_all_t *newResolveAll =
            appMemoryManager->malloc(sizeof(resource_resolve_all_t));
        newResolveAll->callback = callback;
        resource->allResolver = newResolveAll;
      });

  resource->resolveFind =
      appMemoryManager->blockCopy(^(resolveFindCallback callback) {
        resource_resolve_find_t *newResolveFind =
            appMemoryManager->malloc(sizeof(resource_resolve_find_t));
        newResolveFind->callback = callback;
        resource->findResolver = newResolveFind;
      });

  resource->paginate =
      appMemoryManager->blockCopy(^(paginateCallback callback) {
        resource_paginate_t *newPaginate =
            appMemoryManager->malloc(sizeof(resource_paginate_t));
        newPaginate->callback = callback;
        resource->paginator = newPaginate;
      });

  resource->baseScope =
      appMemoryManager->blockCopy(^(baseScopeCallback callback) {
        resource_base_scope_t *newBaseScope =
            appMemoryManager->malloc(sizeof(resource_base_scope_t));
        newBaseScope->callback = callback;
        resource->baseScoper = newBaseScope;
      });

  resource->attribute = appMemoryManager->blockCopy(^(char *attributeName,
                                                      char *attributeType) {
    class_attribute_t *newAttribute =
        appMemoryManager->malloc(sizeof(class_attribute_t));
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
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" > $") + 1);
                sprintf(whereCondition, "%s > $", attributeName);
                return scope->where(whereCondition, value);
              }));

      resource->filter(
          attributeName, "gte",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" >= $") + 1);
                sprintf(whereCondition, "%s >= $", attributeName);
                return scope->where(whereCondition, value);
              }));

      resource->filter(
          attributeName, "lt",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" < $") + 1);
                sprintf(whereCondition, "%s < $", attributeName);
                return scope->where(whereCondition, value);
              }));

      resource->filter(
          attributeName, "lte",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" <= $") + 1);
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

      resource->filter(
          attributeName, "eq",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, int count) {
                if (count == 1) {
                  char *whereCondition = model->instanceMemoryManager->malloc(
                      strlen(attributeName) + strlen(" = $") + 1);
                  sprintf(whereCondition, "%s = $", attributeName);
                  return scope->where(whereCondition, values[0]);
                }
                return scope->whereIn(attributeName, true, values, count);
              }));

      resource->filter(
          attributeName, "not_eq",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, int count) {
                if (count == 1) {
                  char *whereCondition = model->instanceMemoryManager->malloc(
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
          appMemoryManager->blockCopy(^(query_t *scope, const char **values,
                                        int count) {
            if (count == 1) {
              char *whereCondition = model->instanceMemoryManager->malloc(
                  strlen(attributeName) + strlen("LOWER() = LOWER($)") + 1);
              sprintf(whereCondition, "LOWER(%s) = LOWER($)", attributeName);
              return scope->where(whereCondition, values[0]);
            }
            // TODO: support LOWER
            return scope->whereIn(attributeName, true, values, count);
          }));

      resource->filter(
          attributeName, "not_eq",
          appMemoryManager->blockCopy(^(query_t *scope, const char **values,
                                        int count) {
            if (count == 1) {
              char *whereCondition = model->instanceMemoryManager->malloc(
                  strlen(attributeName) + strlen("NOT LOWER() = LOWER($)") + 1);
              sprintf(whereCondition, "NOT LOWER(%s) = LOWER($)",
                      attributeName);
              return scope->where(whereCondition, values[0]);
            }
            // TODO: support LOWER
            return scope->whereIn(attributeName, false, values, count);
          }));

      resource->filter(
          attributeName, "eql",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, int count) {
                if (count == 1) {
                  char *whereCondition = model->instanceMemoryManager->malloc(
                      strlen(attributeName) + strlen(" = $") + 1);
                  sprintf(whereCondition, "%s = $", attributeName);
                  return scope->where(whereCondition, values[0]);
                }
                return scope->whereIn(attributeName, true, values, count);
              }));

      resource->filter(
          attributeName, "not_eql",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, int count) {
                if (count == 1) {
                  char *whereCondition = model->instanceMemoryManager->malloc(
                      strlen(attributeName) + strlen(" NOT = $") + 1);
                  sprintf(whereCondition, "NOT %s = $", attributeName);
                  return scope->where(whereCondition, values[0]);
                }
                return scope->whereIn(attributeName, false, values, count);
              }));

      resource->filter(
          attributeName, "match",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" LIKE $") + 1);
                sprintf(whereCondition, "%s LIKE $", attributeName);
                size_t matchValueLen = strlen(value) + strlen("%%") + 1;
                char *matchValue =
                    model->instanceMemoryManager->malloc(matchValueLen);
                snprintf(matchValue, matchValueLen, "%%%s%%", value);
                return scope->where(whereCondition, matchValue);
              }));

      resource->filter(
          attributeName, "not_match",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" NOT LIKE $") + 1);
                sprintf(whereCondition, "%s NOT LIKE $", attributeName);
                size_t matchValueLen = strlen(value) + strlen("%%") + 1;
                char *matchValue =
                    model->instanceMemoryManager->malloc(matchValueLen);
                snprintf(matchValue, matchValueLen, "%%%s%%", value);
                return scope->where(whereCondition, matchValue);
              }));

      resource->filter(
          attributeName, "prefix",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" LIKE $") + 1);
                sprintf(whereCondition, "%s LIKE $", attributeName);
                size_t matchValueLen = strlen(value) + strlen("%") + 1;
                char *matchValue =
                    model->instanceMemoryManager->malloc(matchValueLen);
                snprintf(matchValue, matchValueLen, "%s%%", value);
                return scope->where(whereCondition, matchValue);
              }));

      resource->filter(
          attributeName, "not_prefix",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" NOT LIKE $") + 1);
                sprintf(whereCondition, "%s NOT LIKE $", attributeName);
                size_t matchValueLen = strlen(value) + strlen("%") + 1;
                char *matchValue =
                    model->instanceMemoryManager->malloc(matchValueLen);
                snprintf(matchValue, matchValueLen, "%s%%", value);
                return scope->where(whereCondition, matchValue);
              }));

      resource->filter(
          attributeName, "suffix",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" LIKE $") + 1);
                sprintf(whereCondition, "%s LIKE $", attributeName);
                size_t matchValueLen = strlen(value) + strlen("%") + 1;
                char *matchValue =
                    model->instanceMemoryManager->malloc(matchValueLen);
                snprintf(matchValue, matchValueLen, "%%%s", value);
                return scope->where(whereCondition, matchValue);
              }));

      resource->filter(
          attributeName, "not_suffix",
          appMemoryManager->blockCopy(
              ^(query_t *scope, const char **values, UNUSED int count) {
                const char *value = values[0];
                char *whereCondition = model->instanceMemoryManager->malloc(
                    strlen(attributeName) + strlen(" NOT LIKE $") + 1);
                sprintf(whereCondition, "%s NOT LIKE $", attributeName);
                size_t matchValueLen = strlen(value) + strlen("%") + 1;
                char *matchValue =
                    model->instanceMemoryManager->malloc(matchValueLen);
                snprintf(matchValue, matchValueLen, "%%%s", value);
                return scope->where(whereCondition, matchValue);
              }));
    }

    resource->sort(attributeName, appMemoryManager->blockCopy(^(
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

  resource->resolveAll(^(query_t *scope) {
    return (model_instance_collection_t *)scope->all();
  });

  resource->resolveFind(^(query_t *scope, char *id) {
    return (model_instance_t *)scope->find(id);
  });

  resource->all = appMemoryManager->blockCopy(^(jsonapi_params_t *params) {
    query_t *baseScope = resource->baseScoper->callback(resource->model);

    query_t *queriedScope =
        applyQueryToScope(params->query, baseScope, resource);

    model_instance_collection_t *modelCollection =
        resource->allResolver->callback(queriedScope);

    // debug("%s: %zu", resource->model->tableName, modelCollection->size);
    resource_instance_collection_t *collection =
        createResourceInstanceCollection(resource, modelCollection, params);

    return collection;
  });

  resource->find =
      appMemoryManager->blockCopy(^(jsonapi_params_t *params, char *id) {
        query_t *baseScope = resource->baseScoper->callback(resource->model);

        query_t *queriedScope =
            applyQueryToScope(params->query, baseScope, resource);

        model_instance_t *modelInstance =
            resource->findResolver->callback(queriedScope, id);

        if (modelInstance == NULL) {
          return (resource_instance_t *)NULL;
        }

        resource_instance_t *instance =
            createResourceInstance(resource, modelInstance, params);

        return instance;
      });

  return resource;
}
