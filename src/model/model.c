#include "model.h"

model_store_t *createModelStore(memory_manager_t *memoryManager) {
  model_store_t *store = mmMalloc(memoryManager, sizeof(model_store_t));
  store->count = 0;
  store->add = mmBlockCopy(memoryManager, ^(model_t *model) {
    store->models[store->count++] = model;
  });
  store->lookup = mmBlockCopy(memoryManager, ^(char *tableName) {
    for (int i = 0; i < store->count; i++) {
      if (strcmp(store->models[i]->tableName, tableName) == 0) {
        return store->models[i];
      }
    }
    return (model_t *)NULL;
  });
  return store;
}

model_t *CreateModel(char *tableName, memory_manager_t *memoryManager,
                     database_pool_t *db, model_store_t *modelStore) {

  model_t *model = mmMalloc(memoryManager, sizeof(model_t));

  modelStore->add(model);

  model->tableName = tableName;
  model->memoryManager = memoryManager;
  model->db = db;
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

  model->query = mmBlockCopy(memoryManager, ^() {
    query_t * (^baseQuery)(const char *) =
        getPostgresDBQuery(model->memoryManager, model->db);
    __block query_t *modelQuery = baseQuery(model->tableName);
    void * (^originalAll)(void) = modelQuery->all;
    void * (^originalFind)(char *) = modelQuery->find;

    modelQuery->all = mmBlockCopy(model->memoryManager, ^() {
      PGresult *result = originalAll();
      model_instance_collection_t *collection =
          createModelInstanceCollection(model);
      int recordCount = PQntuples(result);
      collection->arr = mmMalloc(model->memoryManager,
                                 sizeof(model_instance_t *) * recordCount);
      collection->size = recordCount;
      for (int i = 0; i < recordCount; i++) {
        char *idValue = PQgetvalue(result, i, 0);
        char *id = mmMalloc(model->memoryManager, strlen(idValue) + 1);
        strncpy(id, idValue, strlen(idValue) + 1);
        collection->arr[i] = createModelInstance(model);
        collection->arr[i]->id = id;
        int fieldsCount = PQnfields(result);
        for (int j = 0; j < fieldsCount; j++) {
          char *name = PQfname(result, j);
          if (model->getAttribute(name)) {
            char *pgValue = PQgetvalue(result, i, j);
            size_t valueLen = strlen(pgValue) + 1;
            char *value = mmMalloc(model->memoryManager, valueLen);
            strlcpy(value, pgValue, valueLen);
            modelInstanceInitAttr(collection->arr[i], name, value, 0);
          }
        }
      }
      PQclear(result);
      return collection;
    });

    modelQuery->find = mmBlockCopy(memoryManager, ^(char *id) {
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
          char *value = mmMalloc(model->memoryManager, valueLen);
          strlcpy(value, pgValue, valueLen);
          modelInstanceInitAttr(instance, name, value, 0);
        }
      }
      PQclear(result);
      return instance;
    });

    return modelQuery;
  });

  model->find = mmBlockCopy(memoryManager, ^(char *id) {
    return model->query()->find(id);
  });

  model->all = mmBlockCopy(memoryManager, ^() {
    return model->query()->all();
  });

  model->attribute =
      mmBlockCopy(memoryManager, ^(char *attributeName, char *type) {
        class_attribute_t *newAttribute =
            mmMalloc(memoryManager, sizeof(class_attribute_t));
        newAttribute->name = attributeName;
        newAttribute->type = type;
        model->attributes[model->attributesCount] = newAttribute;
        model->attributesCount++;
      });

  model->getAttribute = mmBlockCopy(memoryManager, ^(char *attributeName) {
    for (int i = 0; i < model->attributesCount; i++) {
      if (strcmp(model->attributes[i]->name, attributeName) == 0) {
        return model->attributes[i];
      }
    }
    return (class_attribute_t *)NULL;
  });

  model->validatesAttribute =
      mmBlockCopy(memoryManager, ^(char *attributeName, char *validation) {
        model->validations[model->validationsCount] =
            mmMalloc(memoryManager, sizeof(validation_t));
        model->validations[model->validationsCount]->attributeName =
            attributeName;
        model->validations[model->validationsCount]->validation = validation;
        model->validationsCount++;
      });

  model->new = mmBlockCopy(memoryManager, ^() {
    model_instance_t *instance = createModelInstance(model);
    return instance;
  });

  model->belongsTo =
      mmBlockCopy(memoryManager, ^(char *relatedTableName, char *foreignKey) {
        model->attribute(foreignKey, "integer", NULL);
        belongs_to_t *newBelongsTo =
            mmMalloc(memoryManager, sizeof(belongs_to_t));
        newBelongsTo->tableName = relatedTableName;
        newBelongsTo->foreignKey = foreignKey;
        model->belongsToRelationships[model->belongsToCount] = newBelongsTo;
        model->belongsToCount++;
      });

  model->hasMany =
      mmBlockCopy(memoryManager, ^(char *relatedTableName, char *foreignKey) {
        has_many_t *newHasMany = mmMalloc(memoryManager, sizeof(has_many_t));
        newHasMany->tableName = relatedTableName;
        newHasMany->foreignKey = foreignKey;
        model->hasManyRelationships[model->hasManyCount] = newHasMany;
        model->hasManyCount++;
      });

  model->hasOne =
      mmBlockCopy(memoryManager, ^(char *relatedTableName, char *foreignKey) {
        has_one_t *newHasOne = mmMalloc(memoryManager, sizeof(has_one_t));
        newHasOne->tableName = relatedTableName;
        newHasOne->foreignKey = foreignKey;
        model->hasOneRelationships[model->hasOneCount] = newHasOne;
        model->hasOneCount++;
      });

  model->getForeignKey =
      mmBlockCopy(memoryManager, ^(const char *relationName) {
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
      mmBlockCopy(memoryManager, ^(const char *relationName) {
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

  model->validates = mmBlockCopy(memoryManager, ^(instanceCallback callback) {
    model->validatesCallbacks[model->validatesCallbacksCount] = callback;
    model->validatesCallbacksCount++;
  });

  model->beforeSave = mmBlockCopy(memoryManager, ^(beforeCallback callback) {
    model->beforeSaveCallbacks[model->beforeSaveCallbacksCount] = callback;
    model->beforeSaveCallbacksCount++;
  });

  model->afterSave = mmBlockCopy(memoryManager, ^(instanceCallback callback) {
    model->afterSaveCallbacks[model->afterSaveCallbacksCount] = callback;
    model->afterSaveCallbacksCount++;
  });

  model->beforeDestroy = mmBlockCopy(memoryManager, ^(beforeCallback callback) {
    model->beforeDestroyCallbacks[model->beforeDestroyCallbacksCount] =
        callback;
    model->beforeDestroyCallbacksCount++;
  });

  model->afterDestroy =
      mmBlockCopy(memoryManager, ^(instanceCallback callback) {
        model->afterDestroyCallbacks[model->afterDestroyCallbacksCount] =
            callback;
        model->afterDestroyCallbacksCount++;
      });

  model->beforeUpdate = mmBlockCopy(memoryManager, ^(beforeCallback callback) {
    model->beforeUpdateCallbacks[model->beforeUpdateCallbacksCount] = callback;
    model->beforeUpdateCallbacksCount++;
  });

  model->afterUpdate = mmBlockCopy(memoryManager, ^(instanceCallback callback) {
    model->afterUpdateCallbacks[model->afterUpdateCallbacksCount] = callback;
    model->afterUpdateCallbacksCount++;
  });

  model->beforeCreate = mmBlockCopy(memoryManager, ^(beforeCallback callback) {
    model->beforeCreateCallbacks[model->beforeCreateCallbacksCount] = callback;
    model->beforeCreateCallbacksCount++;
  });

  model->afterCreate = mmBlockCopy(memoryManager, ^(instanceCallback callback) {
    model->afterCreateCallbacks[model->afterCreateCallbacksCount] = callback;
    model->afterCreateCallbacksCount++;
  });

  return model;
}
