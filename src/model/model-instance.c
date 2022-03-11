#include "model.h"

model_instance_t *createModelInstance(model_t *model) {
  memory_manager_t *memoryManager = model->instanceMemoryManager;
  model_instance_t *instance = memoryManager->malloc(sizeof(model_instance_t));
  instance->attributesCount = 0;
  instance->errors = memoryManager->malloc(sizeof(instance_errors_t));
  instance->errors->count = 0;
  instance->id = NULL;
  instance->includesCount = 0;
  instance->model = model;

  instance->addError =
      memoryManager->blockCopy(^(char *attribute, char *message) {
        instance->errors->messages[instance->errors->count] = message;
        instance->errors->attributes[instance->errors->count] = attribute;
        instance->errors->count++;
      });

  instance->isValid = memoryManager->blockCopy(^() {
    return instance->errors->count == 0;
  });

  instance->set = memoryManager->blockCopy(^(char *attribute, char *value) {
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

  instance->initAttr = memoryManager->blockCopy(^(char *attribute, char *value,
                                                  int isDirty) {
    class_attribute_t *classAttribute = model->getAttribute(attribute);
    if (classAttribute == NULL) {
      log_err("'%s' does not have attribute '%s'", model->tableName, attribute);
      return;
    }
    instance_attribute_t *instanceAttribute =
        memoryManager->malloc(sizeof(instance_attribute_t));
    instanceAttribute->classAttribute = classAttribute;
    instanceAttribute->value = value;
    instanceAttribute->isDirty = isDirty;
    instance->attributes[instance->attributesCount] = instanceAttribute;
    instance->attributesCount++;
  });

  instance->get = memoryManager->blockCopy(^(char *attribute) {
    for (int i = 0; i < instance->attributesCount; i++) {
      if (strcmp(instance->attributes[i]->classAttribute->name, attribute) ==
          0) {
        return instance->attributes[i]->value;
      }
    }
    return (char *)NULL;
  });

  instance->r = memoryManager->blockCopy(^(const char *relationName) {
    model_t *relatedModel = model->lookup(relationName);
    if (relatedModel == NULL) {
      log_err("Could not find model '%s'", relationName);
      return (query_t *)NULL;
    }

    relatedModel->setPg(model->pg);
    relatedModel->setInstanceMemoryManager(memoryManager);

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
      whereForeignKey = memoryManager->malloc(strlen(hasManyForeignKey) +
                                              strlen(instance->id) + 5);
      sprintf(whereForeignKey, "%s = %s", hasManyForeignKey, instance->id);
      return relatedModel->query()->where(whereForeignKey);
    }

    char *hasOneForeignKey = NULL;
    for (int i = 0; i < model->hasOneCount; i++) {
      if (strcmp(model->hasOneRelationships[i]->tableName,
                 relatedModel->tableName) == 0) {
        hasOneForeignKey = model->hasOneRelationships[i]->foreignKey;
        break;
      }
    }
    if (hasOneForeignKey) {
      whereForeignKey = memoryManager->malloc(strlen(hasOneForeignKey) +
                                              strlen(instance->id) + 5);
      sprintf(whereForeignKey, "%s = %s", hasOneForeignKey, instance->id);
      return relatedModel->query()->where(whereForeignKey)->limit(1);
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
      whereForeignKey = memoryManager->malloc(strlen(foreignKey) + 6);
      sprintf(whereForeignKey, "id = %s", foreignKey);
      return relatedModel->query()->where(whereForeignKey);
    }

    log_err("Could not find relation '%s' on '%s'", relationName,
            model->tableName);
    return (query_t *)NULL;
  });

  instance->validate = memoryManager->blockCopy(^() {
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

  instance->save = memoryManager->blockCopy(^() {
    int didSave = false;

    for (int i = 0; i < model->beforeSaveCallbacksCount; i++) {
      int res = model->beforeSaveCallbacks[i](instance);
      if (res == 0) {
        return false;
      }
    }

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

    if (dirtyAttributesCount == 0) {
      return didSave;
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
      for (int i = 0; i < model->beforeUpdateCallbacksCount; i++) {
        int res = model->beforeUpdateCallbacks[i](instance);
        if (res == 0) {
          return false;
        }
      }

      saveQuery = memoryManager->malloc(
          strlen("UPDATE  SET () = () WHERE id = ") + strlen(model->tableName) +
          strlen(instance->id) + strlen(dirtyAttributeNamesString) +
          strlen(dirtyAttributePlaceholdersString) + 1);
      sprintf(saveQuery, "UPDATE %s SET (%s) = (%s) WHERE id = %s",
              model->tableName, dirtyAttributeNamesString,
              dirtyAttributePlaceholdersString, instance->id);

    } else {
      // INSERT
      for (int i = 0; i < model->beforeCreateCallbacksCount; i++) {
        int res = model->beforeCreateCallbacks[i](instance);
        if (res == 0) {
          return false;
        }
      }

      saveQuery = memoryManager->malloc(
          strlen("INSERT INTO  () VALUES () RETURNING id;") +
          strlen(model->tableName) + strlen(dirtyAttributeNamesString) +
          strlen(dirtyAttributePlaceholdersString) + 1);
      sprintf(saveQuery, "INSERT INTO %s (%s) VALUES (%s) RETURNING id;",
              model->tableName, dirtyAttributeNamesString,
              dirtyAttributePlaceholdersString);
    }

    PGresult *pgres = model->pg->execParams(
        saveQuery, dirtyAttributesCount, NULL,
        (const char *const *)dirtyAttributeValues, NULL, NULL, 0);

    if (!instance->id) {
      size_t idLen = strlen(PQgetvalue(pgres, 0, 0));
      if (idLen > 0) {
        instance->id = memoryManager->malloc(idLen + 1);
        sprintf(instance->id, "%s", PQgetvalue(pgres, 0, 0));

        for (int i = 0; i < model->afterCreateCallbacksCount; i++) {
          model->afterCreateCallbacks[i](instance);
        }

        didSave = true;
      }
    } else if (PQresultStatus(pgres) != PGRES_COMMAND_OK) {
      log_err("%s", PQresultErrorMessage(pgres));
    } else {

      for (int i = 0; i < model->afterUpdateCallbacksCount; i++) {
        model->afterUpdateCallbacks[i](instance);
      }

      didSave = true;
    }

    PQclear(pgres);

    for (int i = 0; i < dirtyAttributesCount; i++) {
      dirtyAttributes[i]->isDirty = 0;
    }

    for (int i = 0; i < model->afterSaveCallbacksCount; i++) {
      model->afterSaveCallbacks[i](instance);
    }

    return didSave;
  });

  instance->destroy = memoryManager->blockCopy(^() {
    int didDestroy = 0;

    for (int i = 0; i < model->beforeDestroyCallbacksCount; i++) {
      int res = model->beforeDestroyCallbacks[i](instance);
      if (res == 0) {
        return false;
      }
    }

    if (instance->id) {
      char *destroyQuery = memoryManager->malloc(
          strlen("DELETE FROM  WHERE id = ") + strlen(model->tableName) +
          strlen(instance->id) + 1);
      sprintf(destroyQuery, "DELETE FROM %s WHERE id = %s", model->tableName,
              instance->id);
      PGresult *pgres = model->pg->exec(destroyQuery);
      if (PQresultStatus(pgres) != PGRES_COMMAND_OK) {
        log_err("%s", PQresultErrorMessage(pgres));
      } else {
        didDestroy = 1;
      }
      PQclear(pgres);
    }

    for (int i = 0; i < model->afterDestroyCallbacksCount; i++) {
      model->afterDestroyCallbacks[i](instance);
    }

    return didDestroy;
  });

  return instance;
}
