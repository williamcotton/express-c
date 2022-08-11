#include "resource.h"

void addRelatedToResource(resource_instance_t *resourceInstance,
                          char *foreignKey,
                          resource_instance_collection_t *includedCollection) {
  resourceInstance->includedResourceInstances
      [resourceInstance->includedResourceInstancesCount++] =
      resourceInstanceCollectionFilter(
          includedCollection, ^(resource_instance_t *relatedInstance) {
            char *relatedInstanceId =
                modelInstanceGet(relatedInstance->modelInstance, foreignKey);
            char *resourceInstanceId = resourceInstance->id;
            if (relatedInstanceId == NULL) {
              relatedInstanceId = relatedInstance->id;
              char *belongsToKey =
                  (char *)resourceInstance->modelInstance->model
                      ->getBelongsToKey(relatedInstance->type);
              resourceInstanceId = modelInstanceGet(
                  resourceInstance->modelInstance, belongsToKey);
            }
            if (relatedInstanceId == NULL || resourceInstanceId == NULL)
              return 0;

            return strcmp(relatedInstanceId, resourceInstanceId) == 0;
          });
}

/* Check for nested resources to include */

included_params_builder_t *
buildIncludedParams(char *originalIncludedResourceName, resource_t *resource,
                    jsonapi_params_t *params) {

  memory_manager_t *memoryManager = resource->model->memoryManager;
  included_params_builder_t *result = (included_params_builder_t *)mmMalloc(
      memoryManager, sizeof(included_params_builder_t));

  check(originalIncludedResourceName != NULL,
        "originalIncludedResourceName is NULL");
  char *nestedName = NULL;
  char *includedName = originalIncludedResourceName;

  char *splitPoint = strstr(originalIncludedResourceName, ".");
  if (splitPoint != NULL) {
    nestedName = splitPoint + 1;
    includedName[splitPoint - includedName] = '\0';
  }

  resource_t *includedResource = resource->lookup(includedName);
  check(includedResource != NULL, "Could not find included resource %s",
        includedName);

  jsonapi_params_t *includedParams =
      mmMalloc(memoryManager, sizeof(jsonapi_params_t));
  includedParams->query = json_deep_copy(params->query);

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
  }

  /* Delete all sorts that are related to the base resource and not relared to
   * the included resource */
  json_t *sorts = json_object_get(includedParams->query, "sort");
  if (sorts != NULL) {
    json_t *newSorts = json_array();
    size_t index;
    json_array_foreach(sorts, index, value) {
      const char *sortKey = json_string_value(value);
      char *sortSplit = strstr(sortKey, ".");
      if (sortSplit != NULL) {
        json_array_append(newSorts, value);
      }
    }
    json_object_set_new(includedParams->query, "sort", newSorts);
  }

  char *foreignKey = (char *)resource->model->getForeignKey(
      includedResource->model->tableName);

  check(foreignKey != NULL, "Could not find foreign key for included resource");

  char *originalForeignKey = foreignKey;

  /* Only add nested resources to the includes filter */
  if (nestedName != NULL) {
    json_t *newIncludes = json_array();
    json_array_append_new(newIncludes, json_string(nestedName));
    json_object_set_new(includedParams->query, "include", newIncludes);
  } else {
    json_object_del(includedParams->query, "include");
  }

  json_t *ids = json_array();

  jsonapi_params_t * (^getIncludedParams)(void) = mmBlockCopy(memoryManager, ^{
    json_object_set_new(filters, originalForeignKey, ids);
    json_object_set(includedParams->query, "filter", filters);
    return includedParams;
  });

  result->nestedName = nestedName;
  result->includedName = includedName;
  result->includedParams = includedParams;
  result->filters = filters;
  result->includedResource = includedResource;
  result->foreignKey = foreignKey;
  result->originalForeignKey = originalForeignKey;
  result->ids = ids;
  result->getIncludedParams = getIncludedParams;
  return result;
error:
  result->includedResource = NULL;
  return result;
}

void addIncludedResourcesToCollection(
    char *originalIncludedResourceName, resource_t *resource,
    resource_instance_collection_t *collection, jsonapi_params_t *params) {
  included_params_builder_t *includedParamsBuild =
      buildIncludedParams(originalIncludedResourceName, resource, params);

  if (includedParamsBuild->includedResource == NULL)
    return;

  /* Add ids to the included resource */
  if (strcmp(includedParamsBuild->foreignKey, "id") == 0) {
    includedParamsBuild->foreignKey = (char *)resource->model->getBelongsToKey(
        includedParamsBuild->includedResource->model->tableName);
    resourceInstanceCollectionEach(
        collection, ^(resource_instance_t *resourceInstance) {
          json_array_append_new(
              includedParamsBuild->ids,
              json_string(modelInstanceGet(resourceInstance->modelInstance,
                                           includedParamsBuild->foreignKey)));
        });
  } else {
    resourceInstanceCollectionEach(
        collection, ^(resource_instance_t *resourceInstance) {
          json_array_append_new(includedParamsBuild->ids,
                                json_string(resourceInstance->id));
        });
  }

  resource_instance_collection_t *includedCollection =
      includedParamsBuild->includedResource->all(
          includedParamsBuild->getIncludedParams());

  for (int i = 0; i < includedCollection->statsArrayCount; i++) {
    collection->statsArray[collection->statsArrayCount++] =
        includedCollection->statsArray[i];
  }

  collection->includedResourceInstances
      [collection->includedResourceInstancesCount++] = includedCollection;

  for (size_t i = 0; i < collection->size; i++) {
    resource_instance_t *resourceInstance = collection->arr[i];
    addRelatedToResource(resourceInstance, includedParamsBuild->foreignKey,
                         includedCollection);
  }
}

void addIncludedResourcesToInstance(char *originalIncludedResourceName,
                                    resource_t *resource,
                                    resource_instance_t *resourceInstance,
                                    jsonapi_params_t *params) {
  included_params_builder_t *includedParamsBuild =
      buildIncludedParams(originalIncludedResourceName, resource, params);

  if (includedParamsBuild->includedResource == NULL)
    return;

  /* Add id to the included resource */
  json_array_append_new(includedParamsBuild->ids,
                        json_string(resourceInstance->id));

  resource_instance_collection_t *includedCollection =
      includedParamsBuild->includedResource->all(
          includedParamsBuild->getIncludedParams());

  addRelatedToResource(resourceInstance, includedParamsBuild->foreignKey,
                       includedCollection);
}