#include "resource.h"

void nestedIncludes(json_t *includedJSONAPI,
                    resource_instance_collection_t *collection) {
  for (int i = 0; i < collection->includedResourceInstancesCount; i++) {
    resourceInstanceCollectionEach(collection->includedResourceInstances[i], ^(
                                       resource_instance_t *relatedInstance) {
      json_t *relatedJSONAPI = resourceInstanceDataJSONAPI(relatedInstance);
      json_array_append_new(includedJSONAPI, relatedJSONAPI);
    });
    resource_instance_collection_t *nestedCollection =
        collection->includedResourceInstances[i];
    nestedIncludes(includedJSONAPI, nestedCollection);
  }
};

json_t *resourceInstanceCollectionIncludedToJSONAPI(
    resource_instance_collection_t *collection) {
  json_t *includedJSONAPI = json_array();

  nestedIncludes(includedJSONAPI, collection);

  if (json_array_size(includedJSONAPI) == 0) {
    json_decref(includedJSONAPI);
    return (json_t *)NULL;
  }

  return includedJSONAPI;
}

json_t *resourceInstanceCollectionToJSONAPI(
    resource_instance_collection_t *collection) {
  json_t *data = json_array();
  resourceInstanceCollectionEach(collection, ^(resource_instance_t *instance) {
    json_array_append_new(data, resourceInstanceDataJSONAPI(instance));
  });

  // TODO: add links
  // TODO: add errors

  json_t *meta = json_object();

  /* Add stats to the meta object */
  for (int i = 0; i < collection->statsArrayCount; i++) {
    json_t *stat = json_object();
    if (collection->statsArray[i] == NULL) {
      continue;
    }
    if (collection->statsArray[i]->type == NULL) {
      json_object_set_new(stat, collection->statsArray[i]->stat,
                          json_string(collection->statsArray[i]->value));
      json_object_set_new(meta, collection->statsArray[i]->attribute, stat);
    } else {
      json_t *type = json_object();
      json_object_set_new(stat, collection->statsArray[i]->stat,
                          json_string(collection->statsArray[i]->value));
      json_object_set_new(type, collection->statsArray[i]->attribute, stat);
      json_object_set_new(meta, collection->statsArray[i]->type, type);
    }
  }

  json_t *response = json_pack("{s:o, s:o}", "data", data, "meta", meta);

  json_t *included = resourceInstanceCollectionIncludedToJSONAPI(collection);

  if (included) {
    json_object_set_new(response, "included", included);
  }

  return response;
}

void resourceInstanceCollectionEach(resource_instance_collection_t *collection,
                                    eachResourceInstanceCallback callback) {
  for (size_t i = 0; i < collection->size; i++) {
    callback(collection->arr[i]);
  }
};

resource_instance_collection_t *
resourceInstanceCollectionFilter(resource_instance_collection_t *collection,
                                 filterResourceInstanceCallback callback) {
  resource_instance_collection_t *filteredCollection =
      createResourceInstanceCollection(collection->resource,
                                       collection->modelCollection,
                                       collection->params);
  filteredCollection->size = 0;
  filteredCollection->includedResourceInstancesCount =
      collection->includedResourceInstancesCount;
  for (int i = 0; i < filteredCollection->includedResourceInstancesCount; i++) {
    filteredCollection->includedResourceInstances[i] =
        collection->includedResourceInstances[i];
  }
  for (size_t i = 0; i < collection->size; i++) {
    if (callback(collection->arr[i])) {
      filteredCollection->arr[filteredCollection->size++] = collection->arr[i];
    }
  }
  return filteredCollection;
};

void collectionHelpers(resource_instance_collection_t *collection) {
  collection->toJSONAPI =
      mmBlockCopy(collection->resource->model->memoryManager, ^json_t *() {
        return resourceInstanceCollectionToJSONAPI(collection);
      });
}

resource_instance_collection_t *
createResourceInstanceCollection(resource_t *resource,
                                 model_instance_collection_t *modelCollection,
                                 jsonapi_params_t *params) {
  /* Create a collection of resource instances from a collection of model
   * instances. */
  memory_manager_t *memoryManager = resource->model->memoryManager;

  resource_instance_collection_t *collection =
      (resource_instance_collection_t *)mmMalloc(
          memoryManager, sizeof(resource_instance_collection_t));

  collection->resource = resource;
  collection->modelCollection = modelCollection;
  collection->params = params;

  collection->type = resource->type;
  collection->arr = mmMalloc(memoryManager, sizeof(resource_instance_t *) *
                                                modelCollection->size);
  collection->size = modelCollection->size;
  collection->modelCollection = modelCollection;
  collection->includedResourceInstancesCount = 0;

  /* Create the collection's array of resource instances from the collection
  of
   * model instances. */
  for (size_t i = 0; i < collection->size; i++) {
    model_instance_t *modelInstance = modelCollection->arr[i];
    resource_instance_t *resourceInstance =
        createResourceInstance(resource, modelInstance, params);
    collection->arr[i] = resourceInstance;
  }

  return collection;
};
