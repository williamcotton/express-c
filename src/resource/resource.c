#include "resource.h"

resource_store_t *createResourceStore(memory_manager_t *memoryManager) {
  resource_store_t *store = memoryManager->malloc(sizeof(resource_store_t));
  store->count = 0;
  store->add = memoryManager->blockCopy(^(resource_t *resource) {
    store->resources[store->count++] = resource;
  });
  store->lookup = memoryManager->blockCopy(^(char *type) {
    for (int i = 0; i < store->count; i++) {
      if (strcmp(store->resources[i]->type, type) == 0) {
        return store->resources[i];
      }
    }
    return (resource_t *)NULL;
  });
  return store;
}

resource_t *CreateResource(char *type, model_t *model,
                           resource_store_t *resourceStore) {
  memory_manager_t *memoryManager = model->memoryManager;
  resource_t *resource = memoryManager->malloc(sizeof(resource_t));

  resourceStore->add(resource);

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

  resource->lookup = resourceStore->lookup;

  resource->belongsTo =
      memoryManager->blockCopy(^(char *relatedResourceName, char *foreignKey) {
        resource->relationshipsCount++;
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
        resource->relationshipsCount++;
        has_many_resource_t *newHasMany =
            memoryManager->malloc(sizeof(has_many_resource_t));
        newHasMany->resourceName = relatedResourceName;
        newHasMany->foreignKey = foreignKey;
        resource->hasManyRelationships[resource->hasManyCount] = newHasMany;
        resource->hasManyCount++;
      });

  resource->relationshipNames = memoryManager->blockCopy(^() {
    char **relationshipNames = memoryManager->malloc(
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

  resource->resolveAll =
      memoryManager->blockCopy(^(resolveAllCallback callback) {
        resource_resolve_all_t *newResolveAll =
            memoryManager->malloc(sizeof(resource_resolve_all_t));
        newResolveAll->callback = callback;
        resource->allResolver = newResolveAll;
      });

  resource->resolveFind =
      memoryManager->blockCopy(^(resolveFindCallback callback) {
        resource_resolve_find_t *newResolveFind =
            memoryManager->malloc(sizeof(resource_resolve_find_t));
        newResolveFind->callback = callback;
        resource->findResolver = newResolveFind;
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

  resource->attribute =
      memoryManager->blockCopy(^(char *attributeName, char *attributeType) {
        class_attribute_t *newAttribute =
            memoryManager->malloc(sizeof(class_attribute_t));
        newAttribute->name = attributeName;
        newAttribute->type = attributeType;
        resource->attributes[resource->attributesCount] = newAttribute;
        resource->attributesCount++;

        addDefaultFiltersToAttribute(resource, model, memoryManager,
                                     attributeName, attributeType);

        resource->sort(
            attributeName,
            memoryManager->blockCopy(^(query_t *scope, const char *direction) {
              return scope->order(attributeName, (char *)direction);
            }));
      });

  resource->hasAttribute = memoryManager->blockCopy(^(char *attributeName) {
    for (int i = 0; i < resource->attributesCount; i++) {
      if (strcmp(resource->attributes[i]->name, attributeName) == 0) {
        return true;
      }
    }
    return false;
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

  resource->all = memoryManager->blockCopy(^(jsonapi_params_t *params) {
    // debug("%s->all() %s", resource->model->tableName,
    //       json_dumps(params->query, JSON_INDENT(2)));
    /* Get the base scope, a query_t object */
    query_t *baseScope = resource->baseScoper->callback(resource->model);

    resource_stat_value_t *statsArray[100];
    int statsArrayCount = 0;

    /* Apply the query string params to the base scope */
    query_t *queriedScope = applyQueryToScope(
        params->query, baseScope, resource, statsArray, &statsArrayCount);

    /* Query the database and return a collection of model instances

     */
    model_instance_collection_t *modelCollection =
        resource->allResolver->callback(queriedScope);

    /* Create a collection of resource instances from the model instances and
     * the query parameters */
    resource_instance_collection_t *collection =
        createResourceInstanceCollection(resource, modelCollection, params);

    // Add the stats to the collection
    for (int i = 0; i < statsArrayCount; i++) {
      collection->statsArray[i] = statsArray[i];
    }
    collection->statsArrayCount = statsArrayCount;

    /* If there are any included resources, add them to the base resource
     * collection */
    json_t *include = json_object_get(params->query, "include");
    if (include) {
      size_t index;
      json_t *includedResource;
      json_array_foreach(include, index, includedResource) {
        char *includedResourceName =
            (char *)json_string_value(includedResource);
        addIncludedResourcesToCollection(includedResourceName, resource,
                                         collection, params);
      }
    }

    return collection;
  });

  resource->find =
      memoryManager->blockCopy(^(jsonapi_params_t *params, char *id) {
        /* Get the base scope, a query_t object */
        query_t *baseScope = resource->baseScoper->callback(resource->model);

        resource_stat_value_t *statsArray[100];
        int statsArrayCount = 0;

        /* Apply the query string params to the base scope */
        query_t *queriedScope = applyQueryToScope(
            params->query, baseScope, resource, statsArray, &statsArrayCount);

        /* Query the database and return a model instance */
        model_instance_t *modelInstance =
            resource->findResolver->callback(queriedScope, id);

        if (modelInstance == NULL) {
          return (resource_instance_t *)NULL;
        }

        /* Create a resource instance from the model instance and the query
         * parameters */
        resource_instance_t *instance =
            createResourceInstance(resource, modelInstance, params);

        for (int i = 0; i < statsArrayCount; i++) {
          instance->statsArray[i] = statsArray[i];
        }
        instance->statsArrayCount = statsArrayCount;

        /* If there are any included resources, add them to the base resource
         * instance */
        json_t *include = json_object_get(params->query, "include");
        if (include) {
          size_t index;
          json_t *includedResource;
          json_array_foreach(include, index, includedResource) {
            char *includedResourceName =
                (char *)json_string_value(includedResource);
            addIncludedResourcesToInstance(includedResourceName, resource,
                                           instance, params);
          }
        }

        return instance;
      });

  addDefaultFiltersToAttribute(resource, model, memoryManager, "id",
                               "integer_id");

  for (int i = 0; i < model->belongsToCount; i++) {
    resource->belongsToModelRelationships[i] = model->belongsToRelationships[i];
    addDefaultFiltersToAttribute(
        resource, model, memoryManager,
        resource->belongsToModelRelationships[i]->foreignKey, "integer_id");
  }
  resource->belongsToModelCount = model->belongsToCount;

  return resource;
}
