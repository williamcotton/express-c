#include "model.h"

model_store_t *createModelStore(memory_manager_t *memoryManager) {
  model_store_t *store = memoryManager->malloc(sizeof(model_store_t));
  store->count = 0;
  store->add = memoryManager->blockCopy(^(model_t *model) {
    store->models[store->count++] = model;
  });
  store->lookup = memoryManager->blockCopy(^(char *tableName) {
    for (int i = 0; i < store->count; i++) {
      if (strcmp(store->models[i]->tableName, tableName) == 0) {
        return store->models[i];
      }
    }
    return (model_t *)NULL;
  });
  return store;
}

model_t *CreateModel(char *tableName, memory_manager_t *memoryManager, pg_t *pg,
                     model_store_t *modelStore) {

  model_t *model = memoryManager->malloc(sizeof(model_t));

  modelStore->add(model);

  model->tableName = tableName;
  model->memoryManager = memoryManager;
  model->pg = pg;
  model->attributesCount = 0;
  model->validationsCount = 0;
  model->hasManyCount = 0;
  model->hasOneCount = 0;
  model->belongsToCount = 0;
  model->validatesCallbacksCount = 0;
  model->beforeSaveCallbacksCount = 0;
  model->afterSaveCallbacksCount = 0;
  model->beforeDestroyCallbacksCount = 0;
  model->afterDestroyCallbacksCount = 0;
  model->beforeUpdateCallbacksCount = 0;
  model->afterUpdateCallbacksCount = 0;
  model->beforeCreateCallbacksCount = 0;
  model->afterCreateCallbacksCount = 0;

  model->lookup = modelStore->lookup;

  model->query = memoryManager->blockCopy(^() {
    query_t * (^baseQuery)(const char *) =
        getPostgresQuery(model->memoryManager, model->pg);
    __block query_t *modelQuery = baseQuery(model->tableName);
    void * (^originalAll)(void) = modelQuery->all;
    void * (^originalFind)(char *) = modelQuery->find;

    modelQuery->all = model->memoryManager->blockCopy(^() {
      PGresult *result = originalAll();
      model_instance_collection_t *collection =
          createModelInstanceCollection(model);
      int recordCount = PQntuples(result);
      collection->arr = model->memoryManager->malloc(
          sizeof(model_instance_t *) * recordCount);
      collection->size = recordCount;
      for (int i = 0; i < recordCount; i++) {
        char *idValue = PQgetvalue(result, i, 0);
        char *id = model->memoryManager->malloc(strlen(idValue) + 1);
        strncpy(id, idValue, strlen(idValue) + 1);
        collection->arr[i] = createModelInstance(model);
        collection->arr[i]->id = id;
        int fieldsCount = PQnfields(result);
        for (int j = 0; j < fieldsCount; j++) {
          char *name = PQfname(result, j);
          if (model->getAttribute(name)) {
            char *pgValue = PQgetvalue(result, i, j);
            size_t valueLen = strlen(pgValue) + 1;
            char *value = model->memoryManager->malloc(valueLen);
            strlcpy(value, pgValue, valueLen);
            collection->arr[i]->initAttr(name, value, 0);
          }
        }
      }
      PQclear(result);
      return collection;
    });

    modelQuery->find = memoryManager->blockCopy(^(char *id) {
      if (!id) {
        log_err("id is required");
        return (model_instance_t *)NULL;
      }
      PGresult *result = originalFind(id);
      int recordCount = PQntuples(result);
      if (recordCount == 0) {
        PQclear(result);
        return (model_instance_t *)NULL;
      }
      model_instance_t *instance = createModelInstance(model);
      instance->id = id;
      int fieldsCount = PQnfields(result);
      for (int i = 0; i < fieldsCount; i++) {
        char *name = PQfname(result, i);
        if (model->getAttribute(name)) {
          char *pgValue = PQgetvalue(result, 0, i);
          size_t valueLen = strlen(pgValue) + 1;
          char *value = model->memoryManager->malloc(valueLen);
          strlcpy(value, pgValue, valueLen);
          instance->initAttr(name, value, 0);
        }
      }
      PQclear(result);
      return instance;
    });

    return modelQuery;
  });

  model->find = memoryManager->blockCopy(^(char *id) {
    return model->query()->find(id);
  });

  model->all = memoryManager->blockCopy(^() {
    return model->query()->all();
  });

  model->attribute =
      memoryManager->blockCopy(^(char *attributeName, char *type) {
        class_attribute_t *newAttribute =
            memoryManager->malloc(sizeof(class_attribute_t));
        newAttribute->name = attributeName;
        newAttribute->type = type;
        model->attributes[model->attributesCount] = newAttribute;
        model->attributesCount++;
      });

  model->getAttribute = memoryManager->blockCopy(^(char *attributeName) {
    for (int i = 0; i < model->attributesCount; i++) {
      if (strcmp(model->attributes[i]->name, attributeName) == 0) {
        return model->attributes[i];
      }
    }
    return (class_attribute_t *)NULL;
  });

  model->validatesAttribute =
      memoryManager->blockCopy(^(char *attributeName, char *validation) {
        model->validations[model->validationsCount] =
            memoryManager->malloc(sizeof(validation_t));
        model->validations[model->validationsCount]->attributeName =
            attributeName;
        model->validations[model->validationsCount]->validation = validation;
        model->validationsCount++;
      });

  model->new = memoryManager->blockCopy(^() {
    model_instance_t *instance = createModelInstance(model);
    return instance;
  });

  model->belongsTo =
      memoryManager->blockCopy(^(char *relatedTableName, char *foreignKey) {
        model->attribute(foreignKey, "integer", NULL);
        belongs_to_t *newBelongsTo =
            memoryManager->malloc(sizeof(belongs_to_t));
        newBelongsTo->tableName = relatedTableName;
        newBelongsTo->foreignKey = foreignKey;
        model->belongsToRelationships[model->belongsToCount] = newBelongsTo;
        model->belongsToCount++;
      });

  model->hasMany =
      memoryManager->blockCopy(^(char *relatedTableName, char *foreignKey) {
        has_many_t *newHasMany = memoryManager->malloc(sizeof(has_many_t));
        newHasMany->tableName = relatedTableName;
        newHasMany->foreignKey = foreignKey;
        model->hasManyRelationships[model->hasManyCount] = newHasMany;
        model->hasManyCount++;
      });

  model->hasOne =
      memoryManager->blockCopy(^(char *relatedTableName, char *foreignKey) {
        has_one_t *newHasOne = memoryManager->malloc(sizeof(has_one_t));
        newHasOne->tableName = relatedTableName;
        newHasOne->foreignKey = foreignKey;
        model->hasOneRelationships[model->hasOneCount] = newHasOne;
        model->hasOneCount++;
      });

  model->getForeignKey = memoryManager->blockCopy(^(const char *relationName) {
    model_t *relatedModel = model->lookup(relationName);
    if (relatedModel == NULL) {
      log_err("Could not find model '%s'", relationName);
      return (char *)NULL;
    }
    for (int i = 0; i < model->hasManyCount; i++) {
      if (strcmp(model->hasManyRelationships[i]->tableName,
                 relatedModel->tableName) == 0) {
        return model->hasManyRelationships[i]->foreignKey;
      }
    }
    for (int i = 0; i < model->hasOneCount; i++) {
      if (strcmp(model->hasOneRelationships[i]->tableName,
                 relatedModel->tableName) == 0) {
        return model->hasOneRelationships[i]->foreignKey;
      }
    }
    for (int i = 0; i < model->belongsToCount; i++) {
      if (strcmp(model->belongsToRelationships[i]->tableName,
                 relatedModel->tableName) == 0) {
        return "id";
      }
    }
    log_err("Could not find foreign key for '%s'", relationName);
    return (char *)NULL;
  });

  model->getBelongsToKey =
      memoryManager->blockCopy(^(const char *relationName) {
        model_t *relatedModel = model->lookup(relationName);
        if (relatedModel == NULL) {
          log_err("Could not find model '%s'", relationName);
          return (char *)NULL;
        }
        for (int i = 0; i < model->belongsToCount; i++) {
          if (strcmp(model->belongsToRelationships[i]->tableName,
                     relatedModel->tableName) == 0) {
            return model->belongsToRelationships[i]->foreignKey;
          }
        }
        log_err("Could not find belongs to key for '%s'", relationName);
        return (char *)NULL;
      });

  model->validates = memoryManager->blockCopy(^(instanceCallback callback) {
    model->validatesCallbacks[model->validatesCallbacksCount] = callback;
    model->validatesCallbacksCount++;
  });

  model->beforeSave = memoryManager->blockCopy(^(beforeCallback callback) {
    model->beforeSaveCallbacks[model->beforeSaveCallbacksCount] = callback;
    model->beforeSaveCallbacksCount++;
  });

  model->afterSave = memoryManager->blockCopy(^(instanceCallback callback) {
    model->afterSaveCallbacks[model->afterSaveCallbacksCount] = callback;
    model->afterSaveCallbacksCount++;
  });

  model->beforeDestroy = memoryManager->blockCopy(^(beforeCallback callback) {
    model->beforeDestroyCallbacks[model->beforeDestroyCallbacksCount] =
        callback;
    model->beforeDestroyCallbacksCount++;
  });

  model->afterDestroy = memoryManager->blockCopy(^(instanceCallback callback) {
    model->afterDestroyCallbacks[model->afterDestroyCallbacksCount] = callback;
    model->afterDestroyCallbacksCount++;
  });

  model->beforeUpdate = memoryManager->blockCopy(^(beforeCallback callback) {
    model->beforeUpdateCallbacks[model->beforeUpdateCallbacksCount] = callback;
    model->beforeUpdateCallbacksCount++;
  });

  model->afterUpdate = memoryManager->blockCopy(^(instanceCallback callback) {
    model->afterUpdateCallbacks[model->afterUpdateCallbacksCount] = callback;
    model->afterUpdateCallbacksCount++;
  });

  model->beforeCreate = memoryManager->blockCopy(^(beforeCallback callback) {
    model->beforeCreateCallbacks[model->beforeCreateCallbacksCount] = callback;
    model->beforeCreateCallbacksCount++;
  });

  model->afterCreate = memoryManager->blockCopy(^(instanceCallback callback) {
    model->afterCreateCallbacks[model->afterCreateCallbacksCount] = callback;
    model->afterCreateCallbacksCount++;
  });

  return model;
}
