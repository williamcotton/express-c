#include "model.h"

static model_instance_collection_t *
createModelInstanceCollection(model_t *model) {
  request_t *req = model->req;

  model_instance_collection_t *collection =
      req->malloc(sizeof(model_instance_collection_t));

  collection->arr = NULL;
  collection->size = 0;

  collection->at = req->blockCopy(^(size_t index) {
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

  return collection;
}

static model_instance_t *createModelInstance(model_t *model) {
  request_t *req = model->req;
  model_instance_t *instance = req->malloc(sizeof(model_instance_t));
  instance->attributesCount = 0;
  instance->errors = req->malloc(sizeof(instance_errors_t));
  instance->errors->count = 0;
  instance->id = NULL;

  instance->addError = req->blockCopy(^(char *attribute, char *message) {
    instance->errors->messages[instance->errors->count] = message;
    instance->errors->attributes[instance->errors->count] = attribute;
    instance->errors->count++;
  });

  instance->isValid = req->blockCopy(^() {
    return instance->errors->count == 0;
  });

  instance->set = req->blockCopy(^(char *attribute, char *value) {
    char *existing = instance->get(attribute);
    if (existing) {
      for (int i = 0; i < instance->attributesCount; i++) {
        if (strcmp(instance->attributes[i]->classAttribute->name, attribute) ==
            0) {
          instance->attributes[i]->value = value;
          instance->attributes[i]->isDirty = 1;
          return;
        }
      }
    } else {
      instance->initAttr(attribute, value, 1);
    }
  });

  instance->initAttr = req->blockCopy(^(char *attribute, char *value,
                                        int isDirty) {
    class_attribute_t *classAttribute = model->getAttribute(attribute);
    if (classAttribute == NULL) {
      log_err("'%s' does not have attribute '%s'", model->tableName, attribute);
      return;
    }
    instance_attribute_t *instanceAttribute =
        req->malloc(sizeof(instance_attribute_t));
    instanceAttribute->classAttribute = classAttribute;
    instanceAttribute->value = value;
    instanceAttribute->isDirty = isDirty;
    instance->attributes[instance->attributesCount] = instanceAttribute;
    instance->attributesCount++;
  });

  instance->get = req->blockCopy(^(char *attribute) {
    for (int i = 0; i < instance->attributesCount; i++) {
      if (strcmp(instance->attributes[i]->classAttribute->name, attribute) ==
          0) {
        return instance->attributes[i]->value;
      }
    }
    return (char *)NULL;
  });

  instance->r = req->blockCopy(^(UNUSED char *relationName) {
    model_t *relatedModel = model->lookup(relationName);
    if (relatedModel == NULL) {
      log_err("Could not find model '%s'", relationName);
      return (model_instance_collection_t *)NULL;
    }
    model_instance_collection_t *collection = NULL;
    char *whereForeignKey = NULL;

    char *hasManyForeignKey = NULL;
    for (int i = 0; i < model->hasManyCount; i++) {
      if (strcmp(model->hasManyRelationships[i]->tableName,
                 relatedModel->tableName) == 0) {
        hasManyForeignKey = model->hasManyRelationships[i]->foreignKey;
        break;
      }
    }
    if (hasManyForeignKey) {
      whereForeignKey =
          req->malloc(strlen(hasManyForeignKey) + strlen(instance->id) + 5);
      sprintf(whereForeignKey, "%s = %s", hasManyForeignKey, instance->id);
      collection = relatedModel->query()->where(whereForeignKey)->all();
      return collection;
    }

    char *belongsToForeignKey = NULL;
    for (int i = 0; i < model->belongsToCount; i++) {
      if (strcmp(model->belongsToRelationships[i]->tableName,
                 relatedModel->tableName) == 0) {
        belongsToForeignKey = model->belongsToRelationships[i]->foreignKey;
        break;
      }
    }
    if (belongsToForeignKey) {
      char *foreignKey = instance->get(belongsToForeignKey);
      whereForeignKey = req->malloc(strlen(foreignKey) + 6);
      sprintf(whereForeignKey, "id = %s", foreignKey);
      collection = relatedModel->query()->where(whereForeignKey)->all();
      return collection;
    }

    log_err("Could not find relation '%s' on '%s'", relationName,
            model->tableName);
    return (model_instance_collection_t *)NULL;
  });

  instance->validate = req->blockCopy(^() {
    instance->errors->count = 0;
    for (int i = 0; i < model->validationsCount; i++) {
      validation_t *validation = model->validations[i];
      if (strcmp(validation->validation, "presence") == 0) {
        if (instance->get(validation->attributeName) == NULL) {
          instance->addError(validation->attributeName, "is required");
        }
      }
    }
    for (int i = 0; i < model->validatesCallbacksCount; i++) {
      model->validatesCallbacks[i](instance);
    }
    return instance->errors->count == 0;
  });

  instance->save = req->blockCopy(^() {
    int didSave = false;

    if (!instance->validate()) {
      return didSave;
    }

    char *saveQuery = NULL;

    instance_attribute_t *dirtyAttributes[instance->attributesCount];
    int dirtyAttributesCount = 0;
    for (int i = 0; i < instance->attributesCount; i++) {
      if (instance->attributes[i]->isDirty) {
        dirtyAttributes[dirtyAttributesCount] = instance->attributes[i];
        dirtyAttributesCount++;
      }
    }

    char dirtyAttributeNamesString[512];
    char dirtyAttributePlaceholdersString[64];
    char *dirtyAttributeValues[dirtyAttributesCount];
    dirtyAttributeNamesString[0] = '\0';
    for (int i = 0; i < dirtyAttributesCount; i++) {
      char *attributeName = dirtyAttributes[i]->classAttribute->name;
      char *attributeValue = dirtyAttributes[i]->value;
      dirtyAttributeValues[i] = attributeValue;
      if (i == 0) {
        sprintf(dirtyAttributePlaceholdersString, "$%d", i + 1);
        sprintf(dirtyAttributeNamesString, "%s", attributeName);
      } else {
        sprintf(dirtyAttributePlaceholdersString, "%s, $%d",
                dirtyAttributePlaceholdersString, i + 1);
        sprintf(dirtyAttributeNamesString, "%s, %s", dirtyAttributeNamesString,
                attributeName);
      }
    }

    if (instance->id) {
      // UPDATE
      saveQuery = req->malloc(strlen("UPDATE  SET () = () WHERE id = ") +
                              strlen(model->tableName) + strlen(instance->id) +
                              strlen(dirtyAttributeNamesString) +
                              strlen(dirtyAttributePlaceholdersString) + 1);
      sprintf(saveQuery, "UPDATE %s SET (%s) = (%s) WHERE id = %s",
              model->tableName, dirtyAttributeNamesString,
              dirtyAttributePlaceholdersString, instance->id);

    } else {
      // INSERT
      saveQuery = req->malloc(
          strlen("INSERT INTO  () VALUES () RETURNING id;") +
          strlen(model->tableName) + strlen(dirtyAttributeNamesString) +
          strlen(dirtyAttributePlaceholdersString) + 1);
      sprintf(saveQuery, "INSERT INTO %s (%s) VALUES (%s) RETURNING id;",
              model->tableName, dirtyAttributeNamesString,
              dirtyAttributePlaceholdersString);
    }

    PGresult *pgres = model->execParams(
        saveQuery, dirtyAttributesCount, NULL,
        (const char *const *)dirtyAttributeValues, NULL, NULL, 0);

    if (!instance->id) {
      size_t idLen = strlen(PQgetvalue(pgres, 0, 0));
      if (idLen > 0) {
        instance->id = req->malloc(idLen + 1);
        sprintf(instance->id, "%s", PQgetvalue(pgres, 0, 0));
        didSave = true;
      }
    } else if (PQresultStatus(pgres) != PGRES_COMMAND_OK) {
      log_err("%s", PQresultErrorMessage(pgres));
    } else {
      didSave = true;
    }

    PQclear(pgres);

    for (int i = 0; i < dirtyAttributesCount; i++) {
      dirtyAttributes[i]->isDirty = 0;
    }
    return didSave;
  });

  return instance;
}

model_t *CreateModel(char *tableName, request_t *req, pg_t *pg) {
  model_t *model = req->malloc(sizeof(model_t));

  /* Global model store */
  static int modelCount = 0;
  static model_t *models[100];
  models[modelCount] = model;
  modelCount++;

  model->tableName = tableName;
  model->req = req;
  model->exec = pg->exec;
  model->execParams = pg->execParams;
  model->attributesCount = 0;
  model->validationsCount = 0;
  model->hasManyCount = 0;
  model->belongsToCount = 0;
  model->beforeSaveCallbacksCount = 0;
  model->validatesCallbacksCount = 0;

  model->lookup = req->blockCopy(^(char *lookupTableName) {
    for (int i = 0; i < modelCount; i++) {
      if (strcmp(models[i]->tableName, lookupTableName) == 0) {
        return models[i];
      }
    }
    return (model_t *)NULL;
  });

  model->query = req->blockCopy(^() {
    query_t * (^baseQuery)(const char *) = getPostgresQuery(req, pg);
    query_t *modelQuery = baseQuery(model->tableName);
    void * (^originalAll)(void) = modelQuery->all;
    modelQuery->all = req->blockCopy(^() {
      PGresult *result = originalAll();
      model_instance_collection_t *collection =
          createModelInstanceCollection(model);
      int recordCount = PQntuples(result);
      collection->arr = req->malloc(sizeof(model_instance_t *) * recordCount);
      collection->size = recordCount;
      for (int i = 0; i < recordCount; i++) {
        char *id = PQgetvalue(result, i, 0);
        collection->arr[i] = createModelInstance(model);
        collection->arr[i]->id = id;
        int fieldsCount = PQnfields(result);
        for (int j = 0; j < fieldsCount; j++) {
          char *name = PQfname(result, j);
          if (model->getAttribute(name)) {
            char *pgValue = PQgetvalue(result, i, j);
            char *value = req->malloc(strlen(pgValue) + 1);
            strcpy(value, pgValue);
            collection->arr[i]->initAttr(name, value, 0);
          }
        }
      }
      PQclear(result);
      return collection;
    });
    return modelQuery;
  });

  model->find = req->blockCopy(^(char *id) {
    if (!id) {
      log_err("id is required");
      return (model_instance_t *)NULL;
    }
    void * (^originalFind)(char *) = model->query()->find;
    PGresult *result = originalFind(id);
    int recordCount = PQntuples(result);
    if (recordCount == 0) {
      return (model_instance_t *)NULL;
    }
    model_instance_t *instance = createModelInstance(model);
    instance->id = id;
    int fieldsCount = PQnfields(result);
    for (int i = 0; i < fieldsCount; i++) {
      char *name = PQfname(result, i);
      if (model->getAttribute(name)) {
        char *pgValue = PQgetvalue(result, 0, i);
        char *value = req->malloc(strlen(pgValue) + 1);
        strcpy(value, pgValue);
        instance->initAttr(name, value, 0);
      }
    }
    PQclear(result);
    return instance;
  });

  model->all = req->blockCopy(^() {
    return model->query()->all();
  });

  model->attribute = req->blockCopy(^(char *attributeName, char *type) {
    class_attribute_t *newAttribute = req->malloc(sizeof(class_attribute_t));
    newAttribute->name = attributeName;
    newAttribute->type = type;
    model->attributes[model->attributesCount] = newAttribute;
    model->attributesCount++;
  });

  model->getAttribute = req->blockCopy(^(char *attributeName) {
    for (int i = 0; i < model->attributesCount; i++) {
      if (strcmp(model->attributes[i]->name, attributeName) == 0) {
        return model->attributes[i];
      }
    }
    return (class_attribute_t *)NULL;
  });

  model->validatesAttribute =
      req->blockCopy(^(char *attributeName, char *validation) {
        model->validations[model->validationsCount] =
            req->malloc(sizeof(validation_t));
        model->validations[model->validationsCount]->attributeName =
            attributeName;
        model->validations[model->validationsCount]->validation = validation;
        model->validationsCount++;
      });

  model->new = req->blockCopy(^(UNUSED char *id) {
    model_instance_t *instance = createModelInstance(model);
    return instance;
  });

  model->belongsTo =
      req->blockCopy(^(char *relatedTableName, char *foreignKey) {
        belongs_to_t *newBelongsTo = req->malloc(sizeof(belongs_to_t));
        newBelongsTo->tableName = relatedTableName;
        newBelongsTo->foreignKey = foreignKey;
        model->belongsToRelationships[model->belongsToCount] = newBelongsTo;
        model->belongsToCount++;
      });

  model->hasMany = req->blockCopy(^(char *relatedTableName, char *foreignKey) {
    has_many_t *newHasMany = req->malloc(sizeof(has_many_t));
    newHasMany->tableName = relatedTableName;
    newHasMany->foreignKey = foreignKey;
    model->hasManyRelationships[model->hasManyCount] = newHasMany;
    model->hasManyCount++;
  });

  model->beforeSave = req->blockCopy(^(instanceCallback callback) {
    model->beforeSaveCallbacks[model->beforeSaveCallbacksCount] = callback;
    model->beforeSaveCallbacksCount++;
  });

  model->validates = req->blockCopy(^(instanceCallback callback) {
    model->validatesCallbacks[model->validatesCallbacksCount] = callback;
    model->validatesCallbacksCount++;
  });

  return model;
}