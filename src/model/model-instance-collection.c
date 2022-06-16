#include "model.h"

void modelInstanceCollectionEachWithIndex(
    model_instance_collection_t *collection,
    eachInstanceWithIndexCallback callback) {
  for (size_t i = 0; i < collection->size; i++) {
    callback(collection->arr[i], i);
  }
};

model_instance_t *
modelInstanceCollectionAt(model_instance_collection_t *collection,
                          size_t index) {
  if (collection->arr == NULL) {
    log_err("'%s' collection->arr is NULL", collection->model->tableName);
    return (model_instance_t *)NULL;
  }
  if (index >= collection->size) {
    log_err("'%s' collection index out of bounds: %zu >= %zu",
            collection->model->tableName, index, collection->size);
    return (model_instance_t *)NULL;
  }
  return collection->arr[index];
}

void modelInstanceCollectionHelpers(model_instance_collection_t *collection) {
  collection->at =
      mmBlockCopy(collection->model->memoryManager, ^(size_t index) {
        if (collection->arr == NULL) {
          log_err("'%s' collection->arr is NULL", collection->model->tableName);
          return (model_instance_t *)NULL;
        }
        if (index >= collection->size) {
          log_err("'%s' collection index out of bounds: %zu >= %zu",
                  collection->model->tableName, index, collection->size);
          return (model_instance_t *)NULL;
        }
        return collection->arr[index];
      });

  collection->each = mmBlockCopy(
      collection->model->memoryManager, ^(eachInstanceCallback callback) {
        for (size_t i = 0; i < collection->size; i++) {
          callback(collection->arr[i]);
        }
      });

  collection->eachWithIndex =
      mmBlockCopy(collection->model->memoryManager,
                  ^(eachInstanceWithIndexCallback callback) {
                    for (size_t i = 0; i < collection->size; i++) {
                      callback(collection->arr[i], i);
                    }
                  });

  collection->reduce =
      mmBlockCopy(collection->model->memoryManager,
                  ^(void *accumulator, reducerInstanceCallback reducer) {
                    for (size_t i = 0; i < collection->size; i++) {
                      accumulator = reducer(accumulator, collection->arr[i]);
                    }
                    return accumulator;
                  });

  collection->map = mmBlockCopy(
      collection->model->memoryManager, ^(mapInstanceCallback callback) {
        void **arr = mmMalloc(collection->model->memoryManager,
                              sizeof(void *) * collection->size);
        for (size_t i = 0; i < collection->size; i++) {
          arr[i] = callback(collection->arr[i]);
        }
        return arr;
      });

  collection->filter = mmBlockCopy(collection->model->memoryManager, ^(
                                       filterInstanceCallback callback) {
    model_instance_collection_t *filteredCollection =
        createModelInstanceCollection(collection->model);
    filteredCollection->arr =
        mmMalloc(collection->model->memoryManager,
                 sizeof(model_instance_t *) * collection->size);
    filteredCollection->size = 0;
    for (size_t i = 0; i < collection->size; i++) {
      if (callback(collection->arr[i])) {
        filteredCollection->arr[filteredCollection->size] = collection->arr[i];
        filteredCollection->size++;
      }
    }
    return filteredCollection;
  });

  collection->find = mmBlockCopy(
      collection->model->memoryManager, ^(findInstanceCallback callback) {
        for (size_t i = 0; i < collection->size; i++) {
          if (callback(collection->arr[i])) {
            return collection->arr[i];
          }
        }
        return (model_instance_t *)NULL;
      });

  collection->r = mmBlockCopy(
      collection->model->memoryManager, ^(const char *relationName) {
        /* Get a collection of related model instances */
        // debug("'%s' collection r: '%s'", model->tableName, relationName);
        model_t *relatedModel = collection->model->lookup(relationName);
        if (relatedModel == NULL) {
          log_err("'%s' related model '%s' not found",
                  collection->model->tableName, relationName);
          return (query_t *)NULL;
        }

        /* For each model instance in the collection, get the related model
         * instance id */
        const char **relatedModelIds =
            mmMalloc(collection->model->memoryManager,
                     sizeof(const char *) * collection->size);
        modelInstanceCollectionEachWithIndex(
            collection, ^(model_instance_t *instance, int i) {
              // debug("instance->id: %s",
              // instance->id);
              relatedModelIds[i] = instance->id;
            });

        /* Get the foreign key for the related model */
        const char *foreignKey = collection->model->getForeignKey(relationName);
        check(foreignKey != NULL, "foreign key not found");
        // debug("foreignKey: %s", foreignKey);
        // debug("relatedModel->tableName: %s", relatedModel->tableName);

        /* Get the related model instance collection with an array of related
        model
         * ids using a WHERE IN sql query */
        return relatedModel->query()->whereIn(foreignKey, true, relatedModelIds,
                                              collection->size);
      error:
        return (query_t *)NULL;
      });
}

model_instance_collection_t *createModelInstanceCollection(model_t *model) {
  memory_manager_t *memoryManager = model->memoryManager;

  model_instance_collection_t *collection =
      mmMalloc(memoryManager, sizeof(model_instance_collection_t));

  collection->arr = NULL;
  collection->size = 0;
  collection->model = model;

  return collection;
}
