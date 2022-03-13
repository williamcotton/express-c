#include "model.h"

static void addIncludesToCollection(char **includesArray, int includesCount,
                                    model_instance_collection_t *collection) {
  /* The final includes count depends on valid models */
  int finalIncludesCount = 0;

  /* Iterate through the includes array */
  for (int i = 0; i < includesCount; i++) {
    char *relatedModelTableName = includesArray[i];

    char *nestedModelTableName = NULL;

    /* Check if the related model is a nested model */
    char *t = strstr(relatedModelTableName, ".");
    if (t != NULL) {
      /* Get the nested model table name */
      nestedModelTableName = t + 1;
      /* Get the related model table name */
      relatedModelTableName[t - relatedModelTableName] = '\0';
    }

    /* Get the related models query */
    query_t *relatedQuery = collection->r(relatedModelTableName);
    if (relatedQuery == NULL)
      continue;

    // how to apply filters to nested query?

    /* Get the related model instance collection and add them to the collection
     */
    collection->includesArray[i] = includesArray[i];
    collection->includedModelInstanceCollections[i] = relatedQuery->all();

    /* If the related model is a nested model, add the nested model instance
     * collection to the collection */
    if (nestedModelTableName != NULL) {
      addIncludesToCollection(&nestedModelTableName, 1,
                              collection->includedModelInstanceCollections[i]);
    }

    finalIncludesCount++;
  }

  collection->includesCount = finalIncludesCount;
}

static void addIncludesToInstance(char **includesArray, int includesCount,
                                  model_instance_t *instance) {
  int finalIncludesCount = 0;

  for (int i = 0; i < includesCount; i++) {
    char *relatedModelTableName = includesArray[i];

    char *nestedModelTableName = NULL;
    char *t = strstr(relatedModelTableName, ".");
    if (t != NULL) {
      nestedModelTableName = t + 1;
      relatedModelTableName[t - relatedModelTableName] = '\0';
    }

    query_t *relatedQuery = instance->r(relatedModelTableName);
    if (relatedQuery == NULL)
      continue;

    instance->includesArray[i] = includesArray[i];
    instance->includedModelInstanceCollections[i] = relatedQuery->all();

    if (nestedModelTableName != NULL) {
      addIncludesToCollection(&nestedModelTableName, 1,
                              instance->includedModelInstanceCollections[i]);
    }

    finalIncludesCount++;
  }

  instance->includesCount = finalIncludesCount;
}

model_t *CreateModel(char *tableName, memory_manager_t *appMemoryManager) {
  model_t *model = appMemoryManager->malloc(sizeof(model_t));

  /* Global model store */
  static int modelCount = 0;
  static model_t *models[100];
  models[modelCount] = model;
  modelCount++;

  model->tableName = tableName;
  model->instanceMemoryManager = NULL;
  model->appMemoryManager = appMemoryManager;
  model->pg = NULL;
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

  model->setPg = appMemoryManager->blockCopy(^(pg_t *pg) {
    model->pg = pg;
  });

  model->setInstanceMemoryManager =
      appMemoryManager->blockCopy(^(memory_manager_t *instanceMemoryManager) {
        model->instanceMemoryManager = instanceMemoryManager;
      });

  model->lookup = appMemoryManager->blockCopy(^(const char *lookupTableName) {
    for (int i = 0; i < modelCount; i++) {
      if (strcmp(models[i]->tableName, lookupTableName) == 0) {
        return models[i];
      }
    }
    return (model_t *)NULL;
  });

  model->query = appMemoryManager->blockCopy(^() {
    query_t * (^baseQuery)(const char *) =
        getPostgresQuery(model->instanceMemoryManager, model->pg);
    __block query_t *modelQuery = baseQuery(model->tableName);
    void * (^originalAll)(void) = modelQuery->all;
    void * (^originalFind)(char *) = modelQuery->find;
    // query_t * (^originalWhere)(const char *, ...) = modelQuery->where;
    // query_t * (^originalSort)(const char *, ...) = modelQuery->sort;

    modelQuery->includes = model->instanceMemoryManager->blockCopy(
        ^(char **includesResources, int count) {
          /* Add included resources to the current query */
          for (int i = 0; i < count; i++) {
            modelQuery->includesArray[modelQuery->includesCount++] =
                includesResources[i];
          }
          return modelQuery;
        });

    // modelQuery->where =
    //     model->instanceMemoryManager->blockCopy(^(char *columnName, ...) {
    //       // where do we store the included where?
    //       int nParams = pgParamCount(columnName);
    //       debug("nParams: %d", nParams);
    //       va_list args;
    //       va_start(args, columnName);
    //       char *value = va_arg(args, char *);
    //       va_end(args);
    //       debug("[where] %s = %s", columnName, value);
    //       modelQuery = originalWhere(columnName, value);
    //       return modelQuery;
    //     });

    modelQuery->all = model->instanceMemoryManager->blockCopy(^() {
      PGresult *result = originalAll();
      model_instance_collection_t *collection =
          createModelInstanceCollection(model);
      int recordCount = PQntuples(result);
      collection->arr = model->instanceMemoryManager->malloc(
          sizeof(model_instance_t *) * recordCount);
      collection->size = recordCount;
      for (int i = 0; i < recordCount; i++) {
        char *idValue = PQgetvalue(result, i, 0);
        char *id = model->instanceMemoryManager->malloc(strlen(idValue) + 1);
        strncpy(id, idValue, strlen(idValue) + 1);
        collection->arr[i] = createModelInstance(model);
        collection->arr[i]->id = id;
        int fieldsCount = PQnfields(result);
        for (int j = 0; j < fieldsCount; j++) {
          char *name = PQfname(result, j);
          if (model->getAttribute(name)) {
            char *pgValue = PQgetvalue(result, i, j);
            size_t valueLen = strlen(pgValue) + 1;
            char *value = model->instanceMemoryManager->malloc(valueLen);
            strlcpy(value, pgValue, valueLen);
            collection->arr[i]->initAttr(name, value, 0);
          }
        }
      }
      /* Add included resources to the collection of model instances */
      addIncludesToCollection((char **)modelQuery->includesArray,
                              modelQuery->includesCount, collection);
      // debug("Model: %s", model->tableName);
      PQclear(result);
      return collection;
    });

    modelQuery->find = appMemoryManager->blockCopy(^(char *id) {
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
          char *value = model->instanceMemoryManager->malloc(valueLen);
          strlcpy(value, pgValue, valueLen);
          instance->initAttr(name, value, 0);
        }
      }
      addIncludesToInstance((char **)modelQuery->includesArray,
                            modelQuery->includesCount, instance);
      PQclear(result);
      return instance;
    });

    return modelQuery;
  });

  model->find = appMemoryManager->blockCopy(^(char *id) {
    return model->query()->find(id);
  });

  model->all = appMemoryManager->blockCopy(^() {
    return model->query()->all();
  });

  model->attribute =
      appMemoryManager->blockCopy(^(char *attributeName, char *type) {
        class_attribute_t *newAttribute =
            appMemoryManager->malloc(sizeof(class_attribute_t));
        newAttribute->name = attributeName;
        newAttribute->type = type;
        model->attributes[model->attributesCount] = newAttribute;
        model->attributesCount++;
      });

  model->getAttribute = appMemoryManager->blockCopy(^(char *attributeName) {
    for (int i = 0; i < model->attributesCount; i++) {
      if (strcmp(model->attributes[i]->name, attributeName) == 0) {
        return model->attributes[i];
      }
    }
    return (class_attribute_t *)NULL;
  });

  model->validatesAttribute =
      appMemoryManager->blockCopy(^(char *attributeName, char *validation) {
        model->validations[model->validationsCount] =
            appMemoryManager->malloc(sizeof(validation_t));
        model->validations[model->validationsCount]->attributeName =
            attributeName;
        model->validations[model->validationsCount]->validation = validation;
        model->validationsCount++;
      });

  model->new = appMemoryManager->blockCopy(^() {
    model_instance_t *instance = createModelInstance(model);
    return instance;
  });

  model->belongsTo =
      appMemoryManager->blockCopy(^(char *relatedTableName, char *foreignKey) {
        model->attribute(foreignKey, "integer", NULL);
        belongs_to_t *newBelongsTo =
            appMemoryManager->malloc(sizeof(belongs_to_t));
        newBelongsTo->tableName = relatedTableName;
        newBelongsTo->foreignKey = foreignKey;
        model->belongsToRelationships[model->belongsToCount] = newBelongsTo;
        model->belongsToCount++;
      });

  model->hasMany =
      appMemoryManager->blockCopy(^(char *relatedTableName, char *foreignKey) {
        has_many_t *newHasMany = appMemoryManager->malloc(sizeof(has_many_t));
        newHasMany->tableName = relatedTableName;
        newHasMany->foreignKey = foreignKey;
        model->hasManyRelationships[model->hasManyCount] = newHasMany;
        model->hasManyCount++;
      });

  model->hasOne =
      appMemoryManager->blockCopy(^(char *relatedTableName, char *foreignKey) {
        has_one_t *newHasOne = appMemoryManager->malloc(sizeof(has_one_t));
        newHasOne->tableName = relatedTableName;
        newHasOne->foreignKey = foreignKey;
        model->hasOneRelationships[model->hasOneCount] = newHasOne;
        model->hasOneCount++;
      });

  model->getForeignKey =
      appMemoryManager->blockCopy(^(const char *relationName) {
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
      appMemoryManager->blockCopy(^(const char *relationName) {
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

  model->validates = appMemoryManager->blockCopy(^(instanceCallback callback) {
    model->validatesCallbacks[model->validatesCallbacksCount] = callback;
    model->validatesCallbacksCount++;
  });

  model->beforeSave = appMemoryManager->blockCopy(^(beforeCallback callback) {
    model->beforeSaveCallbacks[model->beforeSaveCallbacksCount] = callback;
    model->beforeSaveCallbacksCount++;
  });

  model->afterSave = appMemoryManager->blockCopy(^(instanceCallback callback) {
    model->afterSaveCallbacks[model->afterSaveCallbacksCount] = callback;
    model->afterSaveCallbacksCount++;
  });

  model->beforeDestroy =
      appMemoryManager->blockCopy(^(beforeCallback callback) {
        model->beforeDestroyCallbacks[model->beforeDestroyCallbacksCount] =
            callback;
        model->beforeDestroyCallbacksCount++;
      });

  model->afterDestroy =
      appMemoryManager->blockCopy(^(instanceCallback callback) {
        model->afterDestroyCallbacks[model->afterDestroyCallbacksCount] =
            callback;
        model->afterDestroyCallbacksCount++;
      });

  model->beforeUpdate = appMemoryManager->blockCopy(^(beforeCallback callback) {
    model->beforeUpdateCallbacks[model->beforeUpdateCallbacksCount] = callback;
    model->beforeUpdateCallbacksCount++;
  });

  model->afterUpdate =
      appMemoryManager->blockCopy(^(instanceCallback callback) {
        model->afterUpdateCallbacks[model->afterUpdateCallbacksCount] =
            callback;
        model->afterUpdateCallbacksCount++;
      });

  model->beforeCreate = appMemoryManager->blockCopy(^(beforeCallback callback) {
    model->beforeCreateCallbacks[model->beforeCreateCallbacksCount] = callback;
    model->beforeCreateCallbacksCount++;
  });

  model->afterCreate =
      appMemoryManager->blockCopy(^(instanceCallback callback) {
        model->afterCreateCallbacks[model->afterCreateCallbacksCount] =
            callback;
        model->afterCreateCallbacksCount++;
      });

  return model;
}
