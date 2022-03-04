#include <express.h>
#include <jansson.h>
#include <model/model.h>

typedef struct belongs_to_resource_t {
  char *resourceName;
  char *foreignKey;
} belongs_to_resource_t;

typedef struct has_many_resource_t {
  char *resourceName;
  char *foreignKey;
} has_many_resource_t;

typedef struct has_one_resource_t {
  char *resourceName;
  char *foreignKey;
} has_one_resource_t;

typedef struct resource_instance_t {
  instance_errors_t errors;
  model_instance_t *modelInstance;
  instance_errors_t (^save)();
  instance_errors_t (^destroy)();
  instance_errors_t (^update_attributes)();
} resource_instance_t;

typedef struct resource_t {
  char *name;
  model_t *model;
  class_attribute_t *attributes[100];
  int attributesCount;
  belongs_to_resource_t *belongsToRelationships[100];
  int belongsToCount;
  has_many_resource_t *hasManyRelationships[100];
  int hasManyCount;
  has_one_resource_t *hasOneRelationships[100];
  int hasOneCount;
  void (^attribute)(char *, char *);
  void (^belongsTo)(char *, char *);
  void (^hasMany)(char *, char *);
  void (^hasOne)(char *, char *);
  resource_instance_t * (^find)(json_t *);
  resource_instance_t ** (^all)(json_t *);
  resource_instance_t * (^build)(json_t *);
} resource_t;

resource_t *CreateResource(char *name, request_t *req, model_t *model);
