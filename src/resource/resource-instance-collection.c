#include "resource.h"

void nestedIncludes(json_t *includedJSONAPI,
                    resource_instance_collection_t *collection) {
  // debug("collection->type: %p %s %d", collection, collection->type,
  //       collection->includedResourceInstancesCount);
  for (int i = 0; i < collection->includedResourceInstancesCount; i++) {
    // debug("nested include collection %s found in collection %s",
    //       collection->includedResourceInstances[i]->type, collection->type);
    collection->includedResourceInstances[i]->each(
        ^(resource_instance_t *relatedInstance) {
          json_t *relatedJSONAPI = relatedInstance->dataJSONAPI();
          json_array_append_new(includedJSONAPI, relatedJSONAPI);
        });
    resource_instance_collection_t *nestedCollection =
        collection->includedResourceInstances[i];
    nestedIncludes(includedJSONAPI, nestedCollection);
  }
};

resource_instance_collection_t *
createResourceInstanceCollection(resource_t *resource,
                                 model_instance_collection_t *modelCollection,
                                 jsonapi_params_t *params) {
  // debug("createResourceInstanceCollection %s", resource->type);
  /* Create a collection of resource instances from a collection of model
   * instances. */
  memory_manager_t *memoryManager = resource->model->instanceMemoryManager;

  resource_instance_collection_t *collection =
      (resource_instance_collection_t *)memoryManager->malloc(
          sizeof(resource_instance_collection_t));

  collection->type = resource->type;
  collection->arr = memoryManager->malloc(sizeof(resource_instance_t *) *
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
        // debug("filteredCollection->type: %p %s %d", filteredCollection,
        //       filteredCollection->type,
        //       filteredCollection->includedResourceInstancesCount);
        filteredCollection->size = 0;
        filteredCollection->includedResourceInstancesCount =
            collection->includedResourceInstancesCount;
        for (int i = 0; i < filteredCollection->includedResourceInstancesCount;
             i++) {
          filteredCollection->includedResourceInstances[i] =
              collection->includedResourceInstances[i];
        }
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

    nestedIncludes(includedJSONAPI, collection);

    if (json_array_size(includedJSONAPI) == 0) {
      json_decref(includedJSONAPI);
      return (json_t *)NULL;
    }

    return includedJSONAPI;
  });

  collection->toJSONAPI = memoryManager->blockCopy(^json_t *() {
    json_t *data = json_array();
    collection->each(^(resource_instance_t *instance) {
      // instance->includedToJSONAPI();
      json_array_append_new(data, instance->dataJSONAPI());
    });

    // TODO: add links
    // TODO: add included
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

    __block json_t *response =
        json_pack("{s:o, s:o}", "data", data, "meta", meta);

    json_t *included = collection->includedToJSONAPI();

    if (included) {
      json_object_set_new(response, "included", included);
    }

    memoryManager->cleanup(memoryManager->blockCopy(^{
      json_decref(response);
    }));

    // debug("\n%s", json_dumps(response, JSON_INDENT(2)));

    return response;
  });

  return collection;
};
