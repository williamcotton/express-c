#include <express.h>
#include <jansson.h>
#include <model/model.h>

// TODO: rename to jansson_postgres_resource

struct resource_instance_collection_t;
struct resource_instance_t;
struct resource_t;

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

typedef struct default_sort_t {
  char *attribute;
  char *direction;
} default_sort_t;

typedef void (^eachResourceInstanceCallback)(
    struct resource_instance_t *instance);
typedef int (^findResourceInstanceCallback)(
    struct resource_instance_t *instance);
typedef int (^filterResourceInstanceCallback)(
    struct resource_instance_t *instance);
typedef void (^eachResourceInstanceWithIndexCallback)(
    struct resource_instance_t *instance, int index);
typedef void * (^reducerResourceInstanceCallback)(
    void *accumulator, struct resource_instance_t *instance);
typedef void * (^mapResourceInstanceCallback)(
    struct resource_instance_t *instance);

typedef struct resource_instance_collection_t {
  struct resource_instance_t **arr;
  size_t size;
  struct resource_instance_t * (^at)(size_t index);
  void (^each)(eachResourceInstanceCallback);
  struct resource_instance_collection_t * (^filter)(
      filterResourceInstanceCallback);
  struct resource_instance_t * (^find)(findResourceInstanceCallback);
  void (^eachWithIndex)(eachResourceInstanceWithIndexCallback);
  void * (^reduce)(void *accumulator, reducerResourceInstanceCallback);
  void ** (^map)(mapResourceInstanceCallback);
  model_instance_collection_t *data;
  json_t * (^toJson)();
  json_t * (^toJSONAPI)();
} resource_instance_collection_t;

typedef struct resource_instance_t {
  instance_errors_t errors;
  model_instance_t *modelInstance;
  instance_errors_t (^save)();
  instance_errors_t (^destroy)();
  instance_errors_t (^update_attributes)();
  json_t * (^toJson)();
  json_t * (^toJSONAPI)();
} resource_instance_t;

typedef void (^filterCallback)(query_t *scope, char *value);
typedef void (^sortCallback)(query_t *scope, char *direction);
typedef void (^paginateCallback)(query_t *scope, int page, int perPage);
typedef void (^statCallback)(query_t *scope, char *attribute);
typedef void (^resolveCallback)(query_t *scope);

typedef struct resource_t {
  char *type;
  model_t *model;
  int defaultPageSize;
  char *endpointNamespace;
  default_sort_t defaultSort;
  class_attribute_t *attributes[100];
  int attributesCount;
  belongs_to_resource_t *belongsToRelationships[100];
  int belongsToCount;
  has_many_resource_t *hasManyRelationships[100];
  int hasManyCount;
  has_one_resource_t *hasOneRelationships[100];
  int hasOneCount;
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
  void (^attribute)(char *, char *, void *);
  void (^belongsTo)(char *, void *);
  void (^hasMany)(char *, void *);
  void (^hasOne)(char *, void *);
  void (^manyToMany)(char *, void *);
  void (^beforeSave)(beforeCallback);
  void (^afterSave)(instanceCallback);
  void (^beforeDestroy)(beforeCallback);
  void (^afterDestroy)(instanceCallback);
  void (^beforeUpdate)(beforeCallback);
  void (^afterUpdate)(instanceCallback);
  void (^beforeCreate)(beforeCallback);
  void (^afterCreate)(instanceCallback);
  void (^filter)(char *attribute, char *operator, filterCallback);
  void (^sort)(char *attribute, sortCallback);
  void (^resolve)(resolveCallback);
  void (^paginate)(paginateCallback);
  void (^stat)(char *attribute, char *stat, statCallback);
  void (^baseScope)(query_t *);
  resource_instance_t * (^find)(json_t *query);
  resource_instance_t ** (^all)(json_t *query);
  resource_instance_t * (^build)(json_t *query);
  request_t *req;
} __attribute__((packed)) resource_t;

resource_t *CreateResource(char *name, request_t *req, model_t *model);
