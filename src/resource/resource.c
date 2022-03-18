#include "resource.h"

static void
addRelatedToResource(resource_instance_t *resourceInstance, char *foreignKey,
                     resource_instance_collection_t *includedCollection) {
  resourceInstance->includedResourceInstances
      [resourceInstance->includedResourceInstancesCount++] =
      includedCollection->filter(^(resource_instance_t *relatedInstance) {
        char *relatedInstanceId =
            relatedInstance->modelInstance->get(foreignKey);
        char *resourceInstanceId = resourceInstance->id;
        if (relatedInstanceId == NULL) {
          relatedInstanceId = relatedInstance->id;
          char *belongsToKey =
              (char *)resourceInstance->modelInstance->model->getBelongsToKey(
                  relatedInstance->type);
          resourceInstanceId =
              resourceInstance->modelInstance->get(belongsToKey);
        }
        if (relatedInstanceId == NULL || resourceInstanceId == NULL)
          return 0;

        return strcmp(relatedInstanceId, resourceInstanceId) == 0;
      });
}

struct nested_and_included_names_t {
  char *nestedName;
  char *includedName;
};

/* Check for nested resources to include */

struct nested_and_included_names_t
getNestedAndIncludedNames(char *originalIncludedResourceName) {
  struct nested_and_included_names_t result;
  char *nestedResourceName = NULL;
  char *includedResourceName = originalIncludedResourceName;

  char *splitPoint = strstr(originalIncludedResourceName, ".");
  if (splitPoint != NULL) {
    nestedResourceName = splitPoint + 1;
    *splitPoint = '\0';
    includedResourceName[splitPoint - includedResourceName] = '\0';
  }
  result = (struct nested_and_included_names_t){
      .nestedName = nestedResourceName,
      .includedName = includedResourceName,
  };
  return result;
}

static void addIncludedResourcesToCollection(
    char *originalIncludedResourceName, resource_t *resource,
    resource_instance_collection_t *collection, jsonapi_params_t *params) {

  memory_manager_t *memoryManager = resource->model->instanceMemoryManager;

  /* Check for nested resources to include

    Nested resources are in the form of:

    "includedResourceName.nestedResourceName"

   */
  struct nested_and_included_names_t nestedAndIncludedNames =
      getNestedAndIncludedNames(originalIncludedResourceName);
  char *includedResourceName = nestedAndIncludedNames.includedName;

  resource_t *includedResource = resource->lookup(includedResourceName);
  if (includedResource == NULL)
    return;

  char *foreignKey = (char *)resource->model->getForeignKey(
      includedResource->model->tableName);

  char *originalForeignKey = foreignKey;

  json_t *ids = json_array();
  if (strcmp(foreignKey, "id") == 0) {
    foreignKey = (char *)resource->model->getBelongsToKey(
        includedResource->model->tableName);
    collection->each(^(resource_instance_t *resourceInstance) {
      json_array_append_new(
          ids, json_string(resourceInstance->modelInstance->get(foreignKey)));
    });
  } else {
    collection->each(^(resource_instance_t *resourceInstance) {
      json_array_append_new(ids, json_string(resourceInstance->id));
    });
  }

  /* Create a new jsonAPI query for the included resources

    We need to do this because there are query parameters that only apply to the
    base resource

  */
  jsonapi_params_t *includedParams =
      memoryManager->malloc(sizeof(jsonapi_params_t));
  includedParams->query = json_deep_copy(params->query);

  memoryManager->cleanup(memoryManager->blockCopy(^{
    json_decref(includedParams->query);
  }));

  json_t *filters = json_object_get(includedParams->query, "filter");

  /* Delete all filters that are related to the base resource and not related to
   * the included resource */
  char *keysToDelete[100];
  size_t keysToDeleteCount = 0;
  const char *key;
  json_t *value;
  json_object_foreach(filters, key, value) {
    resource_t *includedResourceFilter = resource->lookup(key);
    if (includedResourceFilter == NULL)
      keysToDelete[keysToDeleteCount++] = (char *)key;
  }
  for (size_t i = 0; i < keysToDeleteCount; i++)
    json_object_del(filters, keysToDelete[i]);

  if (filters == NULL) {
    filters = json_object();
    memoryManager->cleanup(memoryManager->blockCopy(^{
      json_decref(filters);
    }));
  }

  /* Filter the included resources by the ids of the base resources */
  json_object_set_new(filters, originalForeignKey, ids);
  json_object_set(includedParams->query, "filter", filters);

  /* Only include the nested resource in the new query */
  char *nestedResourceName = nestedAndIncludedNames.nestedName;
  if (nestedResourceName != NULL) {
    json_t *newIncludes = json_array();
    json_array_append_new(newIncludes, json_string(nestedResourceName));
    json_object_set_new(includedParams->query, "include", newIncludes);
  } else {
    json_object_del(includedParams->query, "include");
  }

  /* Get the included resources */
  resource_instance_collection_t *includedCollection =
      includedResource->all(includedParams);

  /* Add the included resources to the collection */
  collection->includedResourceInstances
      [collection->includedResourceInstancesCount++] = includedCollection;

  /* Add the collection of included resources to each resource in the collection
   */
  for (size_t i = 0; i < collection->size; i++) {
    resource_instance_t *resourceInstance = collection->arr[i];
    addRelatedToResource(resourceInstance, foreignKey, includedCollection);
  }
}

static void addIncludedResourcesToInstance(
    char *originalIncludedResourceName, resource_t *resource,
    resource_instance_t *resourceInstance, jsonapi_params_t *params) {

  memory_manager_t *memoryManager = resource->model->instanceMemoryManager;

  /* Check for nested resources to include

    Nested resources are in the form of:

    "includedResourceName.nestedResourceName"

   */
  struct nested_and_included_names_t nestedAndIncludedNames =
      getNestedAndIncludedNames(originalIncludedResourceName);
  char *includedResourceName = nestedAndIncludedNames.includedName;

  resource_t *includedResource = resource->lookup(includedResourceName);
  if (includedResource == NULL)
    return;

  char *foreignKey = (char *)resource->model->getForeignKey(
      includedResource->model->tableName);

  char *originalForeignKey = foreignKey;

  json_t *ids = json_array();
  memoryManager->cleanup(memoryManager->blockCopy(^{
    json_decref(ids);
  }));
  json_array_append_new(ids, json_string(resourceInstance->id));

  /* Create a new jsonAPI query for the included resources

    We need to do this because there are query parameters that only apply to the
    base resource

  */
  jsonapi_params_t *includedParams =
      memoryManager->malloc(sizeof(jsonapi_params_t));
  includedParams->query = json_deep_copy(params->query);

  memoryManager->cleanup(memoryManager->blockCopy(^{
    json_decref(includedParams->query);
  }));

  json_t *filters = json_object_get(includedParams->query, "filter");

  /* Delete all filters that are related to the base resource and not related to
   * the included resource */
  char *keysToDelete[100];
  size_t keysToDeleteCount = 0;
  const char *key;
  json_t *value;
  json_object_foreach(filters, key, value) {
    resource_t *includedResourceFilter = resource->lookup(key);
    if (includedResourceFilter == NULL)
      keysToDelete[keysToDeleteCount++] = (char *)key;
  }
  for (size_t i = 0; i < keysToDeleteCount; i++)
    json_object_del(filters, keysToDelete[i]);

  if (filters == NULL) {
    filters = json_object();
    memoryManager->cleanup(memoryManager->blockCopy(^{
      json_decref(filters);
    }));
  }

  /* Filter the included resources by the ids of the base resources */
  json_object_set(filters, originalForeignKey, ids);
  json_object_set(includedParams->query, "filter", filters);

  /* Only include the nested resource in the new query */
  char *nestedResourceName = nestedAndIncludedNames.nestedName;
  if (nestedResourceName != NULL) {
    json_t *newIncludes = json_array();
    json_array_append_new(newIncludes, json_string(nestedResourceName));
    json_object_set_new(includedParams->query, "include", newIncludes);
  } else {
    json_object_del(includedParams->query, "include");
  }

  /* Get the included resources */
  resource_instance_collection_t *includedCollection =
      includedResource->all(includedParams);

  /* Add the collection of included resources to the resource */
  addRelatedToResource(resourceInstance, foreignKey, includedCollection);
}

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

  resource->attribute =
      appMemoryManager->blockCopy(^(char *attributeName, char *attributeType) {
        class_attribute_t *newAttribute =
            appMemoryManager->malloc(sizeof(class_attribute_t));
        newAttribute->name = attributeName;
        newAttribute->type = attributeType;
        resource->attributes[resource->attributesCount] = newAttribute;
        resource->attributesCount++;

        addDefaultFiltersToAttribute(resource, model, appMemoryManager,
                                     attributeName, attributeType);

        resource->sort(attributeName,
                       appMemoryManager->blockCopy(^(query_t *scope,
                                                     const char *direction) {
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
    // debug("%s->all() %s", resource->model->tableName,
    //       json_dumps(params->query, JSON_INDENT(2)));
    /* Get the base scope, a query_t object */
    query_t *baseScope = resource->baseScoper->callback(resource->model);

    /* Apply the query string params to the base scope */
    query_t *queriedScope =
        applyQueryToScope(params->query, baseScope, resource);

    /* Query the database and return a collection of model instances */
    model_instance_collection_t *modelCollection =
        resource->allResolver->callback(queriedScope);

    /* Create a collection of resource instances from the model instances and
     * the query parameters */
    resource_instance_collection_t *collection =
        createResourceInstanceCollection(resource, modelCollection, params);

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
      appMemoryManager->blockCopy(^(jsonapi_params_t *params, char *id) {
        /* Get the base scope, a query_t object */
        query_t *baseScope = resource->baseScoper->callback(resource->model);

        /* Apply the query string params to the base scope */
        query_t *queriedScope =
            applyQueryToScope(params->query, baseScope, resource);

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

  addDefaultFiltersToAttribute(resource, model, appMemoryManager, "id",
                               "integer_id");

  for (int i = 0; i < model->belongsToCount; i++) {
    resource->belongsToModelRelationships[i] = model->belongsToRelationships[i];
    addDefaultFiltersToAttribute(
        resource, model, appMemoryManager,
        resource->belongsToModelRelationships[i]->foreignKey, "integer_id");
  }
  resource->belongsToModelCount = model->belongsToCount;

  return resource;
}
