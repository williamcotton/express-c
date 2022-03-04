#include "model.h"

model_instance_t *createModelInstance(model_t *model) {
  request_t *req = model->req;
  model_instance_t *instance = req->malloc(sizeof(model_instance_t));
  instance->attributesCount = 0;
  instance->errors = malloc(sizeof(instance_errors_t));
  instance->errors->count = 0;

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
          return;
        }
      }
    } else {
      class_attribute_t *classAttribute = model->getAttribute(attribute);
      instance_attribute_t *instanceAttribute =
          req->malloc(sizeof(instance_attribute_t));
      instanceAttribute->classAttribute = classAttribute;
      instanceAttribute->value = value;
      instance->attributes[instance->attributesCount] = instanceAttribute;
      instance->attributesCount++;
    }
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
    model_instance_collection_t *collection =
        req->malloc(sizeof(model_instance_collection_t));

    model_t *relatedModel = model->lookup(relationName);
    char *foreignKey = NULL;
    for (int i = 0; i < model->hasManyCount; i++) {
      if (strcmp(model->hasManyRelationships[i]->tableName,
                 relatedModel->tableName) == 0) {
        foreignKey = model->hasManyRelationships[i]->foreignKey;
        break;
      }
    }
    char *whereForeignKey = req->malloc(strlen(foreignKey) + 5);
    sprintf(whereForeignKey, "%s = $", foreignKey);
    PGresult *pgres =
        relatedModel->query()->where(whereForeignKey, instance->id)->all();

    int recordCount = PQntuples(pgres);
    collection->arr = req->malloc(sizeof(model_instance_t *) * recordCount);
    collection->size = recordCount;

    collection->at = req->blockCopy(^(size_t index) {
      return collection->arr[index];
    });

    for (int i = 0; i < recordCount; i++) {
      char *id = PQgetvalue(pgres, i, 0);
      collection->arr[i] = createModelInstance(relatedModel);
      collection->arr[i]->id = id;
      int fieldsCount = PQnfields(pgres);
      for (int j = 0; j < fieldsCount; j++) {
        char *name = PQfname(pgres, j);
        if (relatedModel->getAttribute(name)) {
          char *pgValue = PQgetvalue(pgres, i, j);
          char *value = req->malloc(strlen(pgValue) + 1);
          strcpy(value, pgValue);
          collection->arr[i]->set(name, value);
        }
      }
      PQclear(pgres);
    }

    return collection;
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
    return instance->errors;
  });

  return instance;
}

model_t *CreateModel(char *tableName, request_t *req, pg_t *pg) {
  static int modelCount = 0;
  static model_t *models[100];

  model_t *model = req->malloc(sizeof(model_t));

  models[modelCount] = model;
  modelCount++;

  model->tableName = tableName;
  model->req = req;

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
    return baseQuery(model->tableName);
  });

  model->find = req->blockCopy(^(char *id) {
    void * (^originalFind)(char *) = model->query()->find;
    PGresult *result = originalFind(id);
    model_instance_t *instance = createModelInstance(model);
    instance->id = id;
    int fieldsCount = PQnfields(result);
    for (int i = 0; i < fieldsCount; i++) {
      char *name = PQfname(result, i);
      if (model->getAttribute(name)) {
        char *pgValue = PQgetvalue(result, 0, i);
        char *value = req->malloc(strlen(pgValue) + 1);
        strcpy(value, pgValue);
        instance->set(name, value);
      }
    }
    PQclear(result);
    return instance;
  });

  model->attributesCount = 0;
  model->validationsCount = 0;
  model->hasManyCount = 0;
  model->belongsToCount = 0;
  model->beforeSaveCallbacksCount = 0;
  model->validatesCallbacksCount = 0;

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
