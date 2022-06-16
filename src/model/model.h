#include <middleware/postgres-middleware.h>

#ifndef MODEL_H
#define MODEL_H

// TODO: rename to postgres_model

struct model_t;
struct model_instance_t;
struct model_instance_collection_t;

typedef struct class_attribute_t {
  char *name;
  char *type;
} class_attribute_t;

typedef struct instance_attribute_t {
  class_attribute_t *classAttribute;
  char *value;
  int isDirty;
} instance_attribute_t;

typedef struct validation_t {
  char *attributeName;
  char *validation;
} validation_t;

typedef struct belongs_to_t {
  char *tableName;
  char *foreignKey;
} belongs_to_t;

typedef struct has_many_t {
  char *tableName;
  char *foreignKey;
} has_many_t;

typedef struct has_one_t {
  char *tableName;
  char *foreignKey;
} has_one_t;

typedef struct instance_errors_t {
  char *messages[100];
  char *attributes[100];
  int count;
} instance_errors_t;

typedef void (^eachInstanceCallback)(struct model_instance_t *instance);
typedef int (^findInstanceCallback)(struct model_instance_t *instance);
typedef int (^filterInstanceCallback)(struct model_instance_t *instance);
typedef void (^eachInstanceWithIndexCallback)(struct model_instance_t *instance,
                                              int index);
typedef void * (^reducerInstanceCallback)(void *accumulator,
                                          struct model_instance_t *instance);
typedef void * (^mapInstanceCallback)(struct model_instance_t *instance);

typedef struct model_instance_collection_t {
  struct model_instance_t **arr;
  size_t size;
  struct model_instance_collection_t *includedModelInstanceCollections[100];
  query_t * (^r)(const char *relationName);
  struct model_instance_t * (^at)(size_t index);
  void (^each)(eachInstanceCallback);
  struct model_instance_collection_t * (^filter)(filterInstanceCallback);
  struct model_instance_t * (^find)(findInstanceCallback);
  void (^eachWithIndex)(eachInstanceWithIndexCallback);
  // void * (^reduce)(void *accumulator, reducerInstanceCallback);
  // void ** (^map)(mapInstanceCallback);
} model_instance_collection_t;

typedef struct model_instance_t {
  char *id;
  struct model_t *model;
  instance_errors_t *errors;
  instance_attribute_t *attributes[100];
  int attributesCount;
  model_instance_collection_t *includedModelInstanceCollections[100];
  int (^isValid)();
  void (^addError)(char *attribute, char *message);
  char * (^get)(char *attribute);
  void (^set)(char *attribute, char *value);
  query_t * (^r)(const char *relationName);
  int (^save)();
  int (^validate)();
  int (^destroy)();
} model_instance_t;

void modelInstanceInitAttr(model_instance_t *instance, char *attribute,
                           char *value, int isDirty);

typedef void * (^copyBlock)(void *);
typedef void (^instanceCallback)(model_instance_t *instance);
typedef int (^beforeCallback)(model_instance_t *instance);

typedef struct model_t {
  char *tableName;
  class_attribute_t *attributes[100];
  int attributesCount;
  validation_t *validations[100];
  int validationsCount;
  belongs_to_t *belongsToRelationships[100];
  int belongsToCount;
  has_many_t *hasManyRelationships[100];
  int hasManyCount;
  has_one_t *hasOneRelationships[100];
  int hasOneCount;
  instanceCallback validatesCallbacks[100];
  int validatesCallbacksCount;
  beforeCallback beforeSaveCallbacks[100];
  int beforeSaveCallbacksCount;
  instanceCallback afterSaveCallbacks[100];
  int afterSaveCallbacksCount;
  beforeCallback beforeDestroyCallbacks[100];
  int beforeDestroyCallbacksCount;
  instanceCallback afterDestroyCallbacks[100];
  int afterDestroyCallbacksCount;
  beforeCallback beforeUpdateCallbacks[100];
  int beforeUpdateCallbacksCount;
  instanceCallback afterUpdateCallbacks[100];
  int afterUpdateCallbacksCount;
  beforeCallback beforeCreateCallbacks[100];
  int beforeCreateCallbacksCount;
  instanceCallback afterCreateCallbacks[100];
  int afterCreateCallbacksCount;
  query_t * (^query)();
  database_pool_t *db;
  void (^attribute)(char *name, char *type, void *);
  void (^validatesAttribute)(char *name, char *validation, void *);
  void (^belongsTo)(char *tableName, char *foreignKey, void *);
  void (^hasMany)(char *tableName, char *foreignKey, void *);
  void (^hasOne)(char *tableName, char *foreignKey, void *);
  const char * (^getForeignKey)(const char *relationName);
  const char * (^getBelongsToKey)(const char *relationName);
  void (^validates)(instanceCallback);
  void (^beforeSave)(beforeCallback);
  void (^afterSave)(instanceCallback);
  void (^beforeDestroy)(beforeCallback);
  void (^afterDestroy)(instanceCallback);
  void (^beforeUpdate)(beforeCallback);
  void (^afterUpdate)(instanceCallback);
  void (^beforeCreate)(beforeCallback);
  void (^afterCreate)(instanceCallback);
  class_attribute_t * (^getAttribute)(char *name);
  model_instance_t * (^find)(char *);
  model_instance_collection_t * (^all)();
  model_instance_t * (^new)();
  struct model_t * (^lookup)(const char *);
  memory_manager_t *memoryManager;
} __attribute__((packed)) model_t;

typedef struct model_store_t {
  model_t *models[100];
  int count;
  model_t * (^lookup)(const char *);
  int (^add)(model_t *);
} model_store_t;

model_instance_collection_t *createModelInstanceCollection(model_t *model);
model_instance_t *createModelInstance(model_t *model);
model_t *CreateModel(char *tableName, memory_manager_t *memoryManager,
                     database_pool_t *db, model_store_t *modelStore);
model_store_t *createModelStore(memory_manager_t *memoryManager);

#endif // MODEL_H
