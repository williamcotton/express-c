#include <express.h>
#include <jansson.h>
#include <middleware/jansson-jsonapi-middleware.h>
#include <model/model.h>

// TODO: rename to jansson_postgres_resource

#ifndef RESOURCE_H
#define RESOURCE_H

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
  json_t * (^toJSONAPI)();
} resource_instance_collection_t;

typedef struct resource_instance_t {
  char *id;
  char *type;
  void *context;
  instance_errors_t errors;
  model_instance_t *modelInstance;
  instance_errors_t (^save)();              // TODO: implement save
  instance_errors_t (^destroy)();           // TODO: implement destroy
  instance_errors_t (^update_attributes)(); // TODO: implement update_attributes
  json_t * (^toJSONAPI)();
} resource_instance_t;

typedef query_t * (^filterCallback)(query_t *scope, const char *value);
typedef query_t * (^sortCallback)(query_t *scope, const char *direction);
typedef query_t * (^paginateCallback)(query_t *scope, int page, int perPage);
typedef void (^statCallback)(query_t *scope, const char *attribute);
typedef model_instance_collection_t * (^resolveCallback)(query_t *scope);
typedef query_t * (^baseScopeCallback)(model_t *model);

typedef struct resource_filter_t {
  char *attribute;
  char *operator;
  filterCallback callback;
} resource_filter_t;

typedef struct resource_sort_t {
  char *attribute;
  sortCallback callback;
} resource_sort_t;

typedef struct resource_paginate_t {
  paginateCallback callback;
} resource_paginate_t;

typedef struct resource_stat_t {
  char *attribute;
  char *stat;
  statCallback callback;
} resource_stat_t;

typedef struct resource_resolve_t {
  resolveCallback callback;
} resource_resolve_t;

typedef struct resource_base_scope_t {
  baseScopeCallback callback;
} resource_base_scope_t;

typedef struct resource_t {
  char *type;
  model_t *model;
  memory_manager_t *memoryManager;
  int defaultPageSize;
  const char *endpointNamespace;
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
  resource_filter_t *filters[100];
  int filtersCount;
  resource_sort_t *sorters[100];
  int sortersCount;
  resource_paginate_t *paginator;
  resource_stat_t *stats[100];
  int statsCount;
  resource_resolve_t *resolver;
  resource_base_scope_t *baseScoper;
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
  void (^baseScope)(baseScopeCallback);
  resource_instance_t * (^find)(jsonapi_params_t *params, char *id);
  resource_instance_collection_t * (^all)(jsonapi_params_t *params);
  resource_instance_t * (^build)(jsonapi_params_t *params);
  struct resource_t * (^lookup)(char *);
  request_t *req;
} __attribute__((packed)) resource_t;

resource_t *CreateResource(char *type, model_t *model);

#endif // RESOURCE_H
