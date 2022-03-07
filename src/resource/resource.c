#include "resource.h"

static query_t *applyFiltersToScope(UNUSED json_t *filters, query_t *baseScope,
                                    UNUSED resource_t *resource) {
  return baseScope;
}

static query_t *applySortersToScope(UNUSED json_t *sorters, query_t *baseScope,
                                    UNUSED resource_t *resource) {
  return baseScope;
}

static query_t *applyPaginatorToScope(UNUSED json_t *paginator,
                                      query_t *baseScope,
                                      UNUSED resource_t *resource) {
  return baseScope;
}

static query_t *applyFieldsToScope(UNUSED json_t *fields, query_t *baseScope,
                                   UNUSED resource_t *resource) {
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

  json_t *fields = json_object_get(query, "fields");
  if (fields) {
    baseScope = applyFieldsToScope(fields, baseScope, resource);
  }

  return baseScope;
}

resource_t *CreateResource(char *type, model_t *model, void *context,
                           memory_manager_t *memoryManager) {
  resource_t *resource = memoryManager->malloc(sizeof(resource_t));

  /* Global resource store */
  static int resourceCount = 0;
  static resource_t *resources[100];
  resources[resourceCount] = resource;
  resourceCount++;

  resource->memoryManager = memoryManager;
  resource->type = type;
  resource->model = model;
  resource->context = context;

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

  resource->lookup = memoryManager->blockCopy(^(char *lookupType) {
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

    // TODO: add default filters
    // https://github.com/graphiti-api/graphiti/blob/master/lib/graphiti/adapters/active_record.rb

    if (strcmp(attributeType, "integer") == 0 ||
        strcmp(attributeType, "integer_id") == 0 ||
        strcmp(attributeType, "big_decimal") == 0 ||
        strcmp(attributeType, "float") == 0 ||
        strcmp(attributeType, "date") == 0 ||
        strcmp(attributeType, "datetime") == 0) {
      // TODO: gt, gte, lt, lte
    }

    if (strcmp(attributeType, "boolean") == 0) {
      // TODO: eq
    }

    if (strcmp(attributeType, "hash") == 0) {
      // TODO: eq
    }

    if (strcmp(attributeType, "array") == 0) {
      // TODO: eq
    }

    if (strcmp(attributeType, "enum") == 0 ||
        strcmp(attributeType, "uuid") == 0 ||
        strcmp(attributeType, "integer") == 0 ||
        strcmp(attributeType, "integer_id") == 0 ||
        strcmp(attributeType, "big_decimal") == 0 ||
        strcmp(attributeType, "float") == 0 ||
        strcmp(attributeType, "date") == 0 ||
        strcmp(attributeType, "datetime") == 0) {

      resource->filter(attributeName, "eq", ^(query_t *scope, char *value) {
        char *whereCondition =
            memoryManager->malloc(strlen(attributeName) + strlen(" = $") + 1);
        sprintf(whereCondition, "%s = $", attributeName);
        return scope->where(whereCondition, value);
      });

      resource->filter(attributeName, "not_eq", ^(query_t *scope, char *value) {
        char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                     strlen(" NOT = $") + 1);
        sprintf(whereCondition, "NOT %s = $", attributeName);
        return scope->where(whereCondition, value);
      });
    }

    if (strcmp(attributeType, "string") == 0) {
      // TODO: eq, not_eq (downcase)

      resource->filter(attributeName, "eql", ^(query_t *scope, char *value) {
        char *whereCondition =
            memoryManager->malloc(strlen(attributeName) + strlen(" = $") + 1);
        sprintf(whereCondition, "%s = $", attributeName);
        return scope->where(whereCondition, value);
      });

      resource->filter(attributeName, "not_eql",
                       ^(query_t *scope, char *value) {
                         char *whereCondition = memoryManager->malloc(
                             strlen(attributeName) + strlen(" NOT = $") + 1);
                         sprintf(whereCondition, "NOT %s = $", attributeName);
                         return scope->where(whereCondition, value);
                       });

      resource->filter(attributeName, "match", ^(query_t *scope, char *value) {
        char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                     strlen(" LIKE '%$%'") + 1);
        sprintf(whereCondition, "%s LIKE '%%$%%'", attributeName);
        return scope->where(whereCondition, value);
      });

      resource->filter(
          attributeName, "not_match", ^(query_t *scope, char *value) {
            char *whereCondition = memoryManager->malloc(
                strlen(attributeName) + strlen(" NOT LIKE '%$%'") + 1);
            sprintf(whereCondition, "%s NOT LIKE '%%$%%'", attributeName);
            return scope->where(whereCondition, value);
          });

      resource->filter(attributeName, "prefix", ^(query_t *scope, char *value) {
        char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                     strlen(" LIKE '$%'") + 1);
        sprintf(whereCondition, "%s LIKE '$%%'", attributeName);
        return scope->where(whereCondition, value);
      });

      resource->filter(
          attributeName, "not_prefix", ^(query_t *scope, char *value) {
            char *whereCondition = memoryManager->malloc(
                strlen(attributeName) + strlen(" NOT LIKE '$%'") + 1);
            sprintf(whereCondition, "%s NOT LIKE '$%%'", attributeName);
            return scope->where(whereCondition, value);
          });

      resource->filter(attributeName, "suffix", ^(query_t *scope, char *value) {
        char *whereCondition = memoryManager->malloc(strlen(attributeName) +
                                                     strlen(" LIKE '%$'") + 1);
        sprintf(whereCondition, "%s LIKE '%%$'", attributeName);
        return scope->where(whereCondition, value);
      });

      resource->filter(
          attributeName, "not_suffix", ^(query_t *scope, char *value) {
            char *whereCondition = memoryManager->malloc(
                strlen(attributeName) + strlen(" NOT LIKE '%$'") + 1);
            sprintf(whereCondition, "%s NOT LIKE '%%$'", attributeName);
            return scope->where(whereCondition, value);
          });
    }

    resource->sort(attributeName, ^(query_t *scope, char *direction) {
      return scope->order(attributeName, direction);
    });
  });

  resource->baseScope(^(model_t *_model) {
    return _model->query();
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

    model_instance_collection_t *allModelInstances =
        resource->resolver->callback(queriedScope);

    debug("allModelInstances->size: %zu", allModelInstances->size);
    return allModelInstances;
  });

  return resource;
}
