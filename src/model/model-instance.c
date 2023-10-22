#include "model.h"

void modelInstanceInitAttr(model_instance_t *instance, char *attribute,
                           char *value, int isDirty) {
  class_attribute_t *classAttribute = instance->model->getAttribute(attribute);
  if (classAttribute == NULL) {
    log_err("'%s' does not have attribute '%s'", instance->model->tableName,
            attribute);
    return;
  }
  instance_attribute_t *instanceAttribute =
      mmMalloc(instance->model->memoryManager, sizeof(instance_attribute_t));
  instanceAttribute->classAttribute = classAttribute;
  instanceAttribute->value = value;
  instanceAttribute->isDirty = isDirty;
  instance->attributes[instance->attributesCount] = instanceAttribute;
  instance->attributesCount++;
}

char *modelInstanceGet(model_instance_t *instance, char *attribute) {
  if (strcmp(attribute, "id") == 0) {
    return instance->id;
  }
  for (int i = 0; i < instance->attributesCount; i++) {
    // debug("%s", instance->attributes[i]->classAttribute->name);
    if (strcmp(instance->attributes[i]->classAttribute->name, attribute) == 0) {
      return instance->attributes[i]->value;
    }
  }
  return (char *)NULL;
}

void modelInstanceAddError(model_instance_t *instance, char *attribute,
                           char *message) {
  instance->errors->messages[instance->errors->count] = message;
  instance->errors->attributes[instance->errors->count] = attribute;
  instance->errors->count++;
}

model_instance_t *modelInstanceHelpers(model_instance_t *instance) {
  instance->addError = mmBlockCopy(
      instance->model->memoryManager, ^(char *attribute, char *message) {
        modelInstanceAddError(instance, attribute, message);
      });

  instance->isValid = mmBlockCopy(instance->model->memoryManager, ^() {
    return instance->errors->count == 0;
  });

  instance->set = mmBlockCopy(instance->model->memoryManager, ^(char *attribute,
                                                                char *value) {
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
      modelInstanceInitAttr(instance, attribute, value, 1);
    }
  });

  instance->get =
      mmBlockCopy(instance->model->memoryManager, ^(char *attribute) {
        modelInstanceGet(instance, attribute);
      });

  instance->r =
      mmBlockCopy(instance->model->memoryManager, ^(const char *relationName) {
        model_t *relatedModel = instance->model->lookup(relationName);
        if (relatedModel == NULL) {
          log_err("Could not find model '%s'", relationName);
          return (query_t *)NULL;
        }

        char *whereForeignKey = NULL;

        char *hasManyForeignKey = NULL;
        for (int i = 0; i < instance->model->hasManyCount; i++) {
          if (strcmp(instance->model->hasManyRelationships[i]->tableName,
                     relatedModel->tableName) == 0) {
            hasManyForeignKey =
                instance->model->hasManyRelationships[i]->foreignKey;
            break;
          }
        }
        if (hasManyForeignKey) {
          whereForeignKey =
              mmMalloc(instance->model->memoryManager,
                       strlen(hasManyForeignKey) + strlen(instance->id) + 5);
          sprintf(whereForeignKey, "%s = %s", hasManyForeignKey, instance->id);
          return relatedModel->query()->where(whereForeignKey);
        }

        char *hasOneForeignKey = NULL;
        for (int i = 0; i < instance->model->hasOneCount; i++) {
          if (strcmp(instance->model->hasOneRelationships[i]->tableName,
                     relatedModel->tableName) == 0) {
            hasOneForeignKey =
                instance->model->hasOneRelationships[i]->foreignKey;
            break;
          }
        }
        if (hasOneForeignKey) {
          whereForeignKey =
              mmMalloc(instance->model->memoryManager,
                       strlen(hasOneForeignKey) + strlen(instance->id) + 5);
          sprintf(whereForeignKey, "%s = %s", hasOneForeignKey, instance->id);
          return relatedModel->query()->where(whereForeignKey)->limit(1);
        }

        char *belongsToForeignKey = NULL;
        for (int i = 0; i < instance->model->belongsToCount; i++) {
          if (strcmp(instance->model->belongsToRelationships[i]->tableName,
                     relatedModel->tableName) == 0) {
            belongsToForeignKey =
                instance->model->belongsToRelationships[i]->foreignKey;
            break;
          }
        }
        if (belongsToForeignKey) {
          debug("belongsToForeignKey: %s", belongsToForeignKey);
          char *foreignKey = instance->get(belongsToForeignKey);
          debug("foreignKey: %s", foreignKey);
          whereForeignKey =
              mmMalloc(instance->model->memoryManager, strlen(foreignKey) + 6);
          sprintf(whereForeignKey, "id = %s", foreignKey);
          debug("whereForeignKey: %s", whereForeignKey);
          return relatedModel->query()->where(whereForeignKey);
        }

        log_err("Could not find relation '%s' on '%s'", relationName,
                instance->model->tableName);
        return (query_t *)NULL;
      });

  instance->validate = mmBlockCopy(instance->model->memoryManager, ^() {
    instance->errors->count = 0;
    for (int i = 0; i < instance->model->validationsCount; i++) {
      validation_t *validation = instance->model->validations[i];
      if (strcmp(validation->validation, "presence") == 0) {
        if (instance->get(validation->attributeName) == NULL) {
          instance->addError(validation->attributeName, "is required");
        }
      }
    }
    for (int i = 0; i < instance->model->validatesCallbacksCount; i++) {
      instance->model->validatesCallbacks[i](instance);
    }
    return instance->errors->count == 0;
  });

  instance->save = mmBlockCopy(instance->model->memoryManager, ^() {
    bool didSave = false;

    for (int i = 0; i < instance->model->beforeSaveCallbacksCount; i++) {
      int res = instance->model->beforeSaveCallbacks[i](instance);
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
      for (int i = 0; i < instance->model->beforeUpdateCallbacksCount; i++) {
        int res = instance->model->beforeUpdateCallbacks[i](instance);
        if (res == 0) {
          return false;
        }
      }

      saveQuery = mmMalloc(instance->model->memoryManager,
                           strlen("UPDATE  SET () = () WHERE id = ") +
                               strlen(instance->model->tableName) +
                               strlen(instance->id) +
                               strlen(dirtyAttributeNamesString) +
                               strlen(dirtyAttributePlaceholdersString) + 1);
      sprintf(saveQuery, "UPDATE %s SET (%s) = (%s) WHERE id = %s",
              instance->model->tableName, dirtyAttributeNamesString,
              dirtyAttributePlaceholdersString, instance->id);

    } else {
      // INSERT
      for (int i = 0; i < instance->model->beforeCreateCallbacksCount; i++) {
        int res = instance->model->beforeCreateCallbacks[i](instance);
        if (res == 0) {
          return false;
        }
      }

      saveQuery = mmMalloc(instance->model->memoryManager,
                           strlen("INSERT INTO  () VALUES () RETURNING id;") +
                               strlen(instance->model->tableName) +
                               strlen(dirtyAttributeNamesString) +
                               strlen(dirtyAttributePlaceholdersString) + 1);
      sprintf(saveQuery, "INSERT INTO %s (%s) VALUES (%s) RETURNING id;",
              instance->model->tableName, dirtyAttributeNamesString,
              dirtyAttributePlaceholdersString);
    }

    PGresult *pgres = instance->model->db->execParams(
        saveQuery, dirtyAttributesCount, NULL,
        (const char *const *)dirtyAttributeValues, NULL, NULL, 0);

    if (!instance->id) {
      size_t idLen = strlen(PQgetvalue(pgres, 0, 0));
      if (idLen > 0) {
        instance->id = mmMalloc(instance->model->memoryManager, idLen + 1);
        sprintf(instance->id, "%s", PQgetvalue(pgres, 0, 0));

        for (int i = 0; i < instance->model->afterCreateCallbacksCount; i++) {
          instance->model->afterCreateCallbacks[i](instance);
        }

        didSave = true;
      }
    } else if (PQresultStatus(pgres) != PGRES_COMMAND_OK) {
      log_err("%s", PQresultErrorMessage(pgres));
    } else {

      for (int i = 0; i < instance->model->afterUpdateCallbacksCount; i++) {
        instance->model->afterUpdateCallbacks[i](instance);
      }

      didSave = true;
    }

    PQclear(pgres);

    for (int i = 0; i < dirtyAttributesCount; i++) {
      dirtyAttributes[i]->isDirty = 0;
    }

    for (int i = 0; i < instance->model->afterSaveCallbacksCount; i++) {
      instance->model->afterSaveCallbacks[i](instance);
    }

    return didSave;
  });

  instance->destroy = mmBlockCopy(instance->model->memoryManager, ^() {
    bool didDestroy = 0;

    for (int i = 0; i < instance->model->beforeDestroyCallbacksCount; i++) {
      int res = instance->model->beforeDestroyCallbacks[i](instance);
      if (res == 0) {
        return false;
      }
    }

    if (instance->id) {
      char *destroyQuery = mmMalloc(instance->model->memoryManager,
                                    strlen("DELETE FROM  WHERE id = ") +
                                        strlen(instance->model->tableName) +
                                        strlen(instance->id) + 1);
      sprintf(destroyQuery, "DELETE FROM %s WHERE id = %s",
              instance->model->tableName, instance->id);
      PGresult *pgres = instance->model->db->exec(destroyQuery);
      if (PQresultStatus(pgres) != PGRES_COMMAND_OK) {
        log_err("%s", PQresultErrorMessage(pgres));
      } else {
        didDestroy = 1;
      }
      PQclear(pgres);
    }

    for (int i = 0; i < instance->model->afterDestroyCallbacksCount; i++) {
      instance->model->afterDestroyCallbacks[i](instance);
    }

    return didDestroy;
  });

  return instance;
}

model_instance_t *createModelInstance(model_t *model) {
  model_instance_t *instance =
      mmMalloc(model->memoryManager, sizeof(model_instance_t));
  instance->attributesCount = 0;
  instance->errors = mmMalloc(model->memoryManager, sizeof(instance_errors_t));
  instance->errors->count = 0;
  instance->id = NULL;
  instance->model = model;

  modelInstanceHelpers(instance); // TODO: make optional

  return instance;
}
