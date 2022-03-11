#include "resource.h"

resource_instance_collection_t *
createResourceInstanceCollection(resource_t *resource,
                                 model_instance_collection_t *modelCollection,
                                 jsonapi_params_t *params) {
  memory_manager_t *memoryManager = resource->model->instanceMemoryManager;

  resource_instance_collection_t *collection =
      (resource_instance_collection_t *)memoryManager->malloc(
          sizeof(resource_instance_collection_t));

  collection->arr = memoryManager->malloc(sizeof(resource_instance_t *) *
                                          modelCollection->size);
  collection->size = modelCollection->size;
  collection->modelCollection = modelCollection;

  for (int i = 0; i < modelCollection->includesCount; i++) {
    model_t *includedModel = modelCollection->includesArray[i];
    resource_t *includedResource =
        resource->lookupByModel(includedModel->tableName);
    if (includedResource == NULL)
      continue;
    model_instance_collection_t *relatedModelInstances =
        modelCollection->includedModelInstanceCollections[i];
    collection->includedResourceInstances[i] = createResourceInstanceCollection(
        includedResource, relatedModelInstances, params);
  }

  for (size_t i = 0; i < collection->size; i++) {
    model_instance_t *modelInstance = modelCollection->arr[i];
    resource_instance_t *resourceInstance =
        createResourceInstance(resource, modelInstance, params);
    collection->arr[i] = resourceInstance;
  }

  collection->at = memoryManager->blockCopy(^(size_t index) {
    if (index >= collection->size) {
      return (resource_instance_t *)NULL;
    }
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

  collection->includedToJSONAPI = memoryManager->blockCopy(^json_t *() {
    json_t *includedJSONAPI = json_array();

    for (int i = 0; i < modelCollection->includesCount; i++) {
      collection->includedResourceInstances[i]->each(
          ^(resource_instance_t *relatedInstance) {
            json_t *relatedJSONAPI = relatedInstance->dataJSONAPI();
            json_array_append_new(includedJSONAPI, relatedJSONAPI);
          });
    }

    if (json_array_size(includedJSONAPI) == 0) {
      json_decref(includedJSONAPI);
      return (json_t *)NULL;
    }

    return includedJSONAPI;
  });

  collection->toJSONAPI = memoryManager->blockCopy(^json_t *() {
    json_t *data = json_array();
    collection->each(^(resource_instance_t *instance) {
      instance->includedToJSONAPI();
      json_array_append_new(data, instance->dataJSONAPI());
    });

    // TODO: add links
    // TODO: add included
    // TODO: add errors

    json_t *meta = json_object();

    __block json_t *response =
        json_pack("{s:o, s:o}", "data", data, "meta", meta);

    json_t *included = collection->includedToJSONAPI();

    if (included) {
      json_object_set_new(response, "included", included);
    }

    memoryManager->cleanup(memoryManager->blockCopy(^{
      json_decref(response);
    }));

    return response;
  });

  return collection;
};
