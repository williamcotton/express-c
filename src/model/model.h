#include <express.h>
#include <middleware/postgres-middleware.h>

#ifndef MODEL_H
#define MODEL_H

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
typedef void (^eachInstanceWithIndexCallback)(struct model_instance_t *instance,
                                              int index);
typedef void * (^reducerInstanceCallback)(void *accumulator,
                                          struct model_instance_t *instance);
typedef void * (^mapInstanceCallback)(struct model_instance_t *instance);

typedef struct model_instance_collection_t {
  struct model_instance_t **arr;
  size_t size;
  struct model_instance_t * (^at)(size_t index);
  void (^each)(eachInstanceCallback);
  void (^filter)(eachInstanceCallback);
  void (^find)(eachInstanceCallback);
  void (^eachWithIndex)(eachInstanceWithIndexCallback);
  void * (^reduce)(void *accumulator, reducerInstanceCallback);
  void ** (^map)(mapInstanceCallback);
} model_instance_collection_t;

typedef struct model_instance_t {
  char *id;
  instance_errors_t *errors;
  instance_attribute_t *attributes[100];
  int attributesCount;
  int (^isValid)();
  void (^addError)(char *attribute, char *message);
  char * (^get)(char *attribute);
  void (^set)(char *attribute, char *value);
  void (^initAttr)(char *attribute, char *value, int isDirty);
  model_instance_collection_t * (^r)(char *relationName);
  int (^save)();
  int (^validate)();
  int (^destroy)();
} model_instance_t;

typedef void * (^copyBlock)(void *);

// typedef void (^requestHandler)(request_t *req, response_t *res);
typedef void (^instanceCallback)(model_instance_t *instance);

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
  instanceCallback beforeSaveCallbacks[100];
  int beforeSaveCallbacksCount;
  instanceCallback afterSaveCallbacks[100];
  int afterSaveCallbacksCount;
  instanceCallback beforeDestroyCallbacks[100];
  int beforeDestroyCallbacksCount;
  instanceCallback afterDestroyCallbacks[100];
  int afterDestroyCallbacksCount;
  instanceCallback beforeUpdateCallbacks[100];
  int beforeUpdateCallbacksCount;
  instanceCallback afterUpdateCallbacks[100];
  int afterUpdateCallbacksCount;
  query_t * (^query)();
  PGresult * (^exec)(const char *, ...);
  PGresult * (^execParams)(const char *, int, const Oid *, const char *const *,
                           const int *, const int *, int);
  request_t *req;
  void (^attribute)(char *name, char *type);
  void (^validatesAttribute)(char *name, char *validation);
  void (^belongsTo)(char *tableName, char *foreignKey);
  void (^hasMany)(char *tableName, char *foreignKey);
  void (^hasOne)(char *, char *);
  void (^validates)(instanceCallback);
  void (^beforeSave)(instanceCallback);
  void (^afterSave)(instanceCallback);
  void (^beforeDestroy)(instanceCallback);
  void (^afterDestroy)(instanceCallback);
  void (^beforeUpdate)(instanceCallback);
  void (^afterUpdate)(instanceCallback);
  class_attribute_t * (^getAttribute)(char *name);
  model_instance_t * (^find)(char *);
  model_instance_t ** (^all)();
  model_instance_t * (^new)();
  struct model_t * (^lookup)(char *);
} model_t;

model_t *CreateModel(char *tableName, request_t *req, pg_t *pg);

#endif // MODEL_H