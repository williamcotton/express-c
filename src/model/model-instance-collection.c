#include "model.h"

model_instance_collection_t *createModelInstanceCollection(model_t *model) {
  memory_manager_t *memoryManager = model->instanceMemoryManager;

  model_instance_collection_t *collection =
      memoryManager->malloc(sizeof(model_instance_collection_t));

  collection->arr = NULL;
  collection->size = 0;

  collection->at = memoryManager->blockCopy(^(size_t index) {
    if (collection->arr == NULL) {
      log_err("'%s' collection->arr is NULL", model->tableName);
      return (model_instance_t *)NULL;
    }
    if (index >= collection->size) {
      log_err("'%s' collection index out of bounds: %zu >= %zu",
              model->tableName, index, collection->size);
      return (model_instance_t *)NULL;
    }
    return collection->arr[index];
  });

  collection->each = memoryManager->blockCopy(^(eachInstanceCallback callback) {
    for (size_t i = 0; i < collection->size; i++) {
      callback(collection->arr[i]);
    }
  });

  collection->eachWithIndex =
      memoryManager->blockCopy(^(eachInstanceWithIndexCallback callback) {
        for (size_t i = 0; i < collection->size; i++) {
          callback(collection->arr[i], i);
        }
      });

  collection->reduce = memoryManager->blockCopy(
      ^(void *accumulator, reducerInstanceCallback reducer) {
        for (size_t i = 0; i < collection->size; i++) {
          accumulator = reducer(accumulator, collection->arr[i]);
        }
        return accumulator;
      });

  collection->map = memoryManager->blockCopy(^(mapInstanceCallback callback) {
    void **arr = memoryManager->malloc(sizeof(void *) * collection->size);
    for (size_t i = 0; i < collection->size; i++) {
      arr[i] = callback(collection->arr[i]);
    }
    return arr;
  });

  collection->filter = memoryManager->blockCopy(^(
      filterInstanceCallback callback) {
    model_instance_collection_t *filteredCollection =
        createModelInstanceCollection(model);
    filteredCollection->arr =
        memoryManager->malloc(sizeof(model_instance_t *) * collection->size);
    filteredCollection->size = 0;
    for (size_t i = 0; i < collection->size; i++) {
      if (callback(collection->arr[i])) {
        filteredCollection->arr[filteredCollection->size] = collection->arr[i];
        filteredCollection->size++;
      }
    }
    return filteredCollection;
  });

  collection->find = memoryManager->blockCopy(^(findInstanceCallback callback) {
    for (size_t i = 0; i < collection->size; i++) {
      if (callback(collection->arr[i])) {
        return collection->arr[i];
      }
    }
    return (model_instance_t *)NULL;
  });

  collection->r = memoryManager->blockCopy(^(const char *relationName) {
    // debug("'%s' collection r: '%s'", model->tableName, relationName);
    model_t *relatedModel = model->lookup(relationName);
    if (relatedModel == NULL) {
      log_err("'%s' related model '%s' not found", model->tableName,
              relationName);
      return (query_t *)NULL;
    }
    const char **relatedModelIds =
        memoryManager->malloc(sizeof(const char *) * collection->size);
    collection->eachWithIndex(^(model_instance_t *instance, int i) {
      // debug("instance->id: %s", instance->id);
      relatedModelIds[i] = instance->id;
    });
    const char *foreignKey = model->getForeignKey(relationName);
    check(foreignKey != NULL, "foreign key not found");
    // debug("foreignKey: %s", foreignKey);
    // debug("relatedModel->tableName: %s", relatedModel->tableName);
    return relatedModel->query()->whereIn(foreignKey, true, relatedModelIds,
                                          collection->size);
  error:
    return (query_t *)NULL;
  });

  return collection;
}
