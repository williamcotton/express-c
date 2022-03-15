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
  char *type;
  struct resource_instance_collection_t *includedResourceInstances[100];
  int includedResourceInstancesCount;
  struct resource_instance_t * (^at)(size_t index);
  void (^each)(eachResourceInstanceCallback);
  struct resource_instance_collection_t * (^filter)(
      filterResourceInstanceCallback);
  struct resource_instance_t * (^find)(findResourceInstanceCallback);
  void (^eachWithIndex)(eachResourceInstanceWithIndexCallback);
  void * (^reduce)(void *accumulator, reducerResourceInstanceCallback);
  void ** (^map)(mapResourceInstanceCallback);
  model_instance_collection_t *modelCollection;
  json_t * (^toJSONAPI)();
  json_t * (^includedToJSONAPI)();
} resource_instance_collection_t;

typedef struct resource_instance_t {
  char *id;
  char *type;
  void *context;
  instance_errors_t errors;
  model_instance_t *modelInstance;
  resource_instance_collection_t *includedResourceInstances[100];
  int includedResourceInstancesCount;
  instance_errors_t (^save)();              // TODO: implement save
  instance_errors_t (^destroy)();           // TODO: implement destroy
  instance_errors_t (^update_attributes)(); // TODO: implement update_attributes
  json_t * (^toJSONAPI)();
  json_t * (^dataJSONAPI)();
  json_t * (^includedToJSONAPI)();
} resource_instance_t;

typedef query_t * (^filterCallback)(query_t *scope, const char **values,
                                    int count);
typedef query_t * (^sortCallback)(query_t *scope, const char *direction);
typedef query_t * (^paginateCallback)(query_t *scope, int page, int perPage);
typedef void (^statCallback)(query_t *scope, const char *attribute);
typedef model_instance_collection_t * (^resolveAllCallback)(query_t *scope);
typedef model_instance_t * (^resolveFindCallback)(query_t *scope, char *id);
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

typedef struct resource_resolve_all_t {
  resolveAllCallback callback;
} resource_resolve_all_t;

typedef struct resource_resolve_find_t {
  resolveFindCallback callback;
} resource_resolve_find_t;

typedef struct resource_base_scope_t {
  baseScopeCallback callback;
} resource_base_scope_t;

typedef struct resource_t {
  char *type;
  model_t *model;
  int defaultPageSize;
  const char *endpointNamespace;
  default_sort_t defaultSort;
  class_attribute_t *attributes[100];
  int attributesCount;
  belongs_to_t *belongsToModelRelationships[100];
  int belongsToModelCount;
  belongs_to_resource_t *belongsToRelationships[100];
  int belongsToCount;
  has_many_resource_t *hasManyRelationships[100];
  int hasManyCount;
  has_one_resource_t *hasOneRelationships[100];
  int hasOneCount;
  int relationshipsCount;
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
  resource_resolve_all_t *allResolver;
  resource_resolve_find_t *findResolver;
  resource_base_scope_t *baseScoper;
  void (^attribute)(char *, char *, void *);
  void (^belongsTo)(char *, void *);
  void (^hasMany)(char *, void *);
  void (^hasOne)(char *, void *);
  char ** (^relationshipNames)();
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
  void (^resolveAll)(resolveAllCallback);
  void (^resolveFind)(resolveFindCallback);
  void (^paginate)(paginateCallback);
  void (^stat)(char *attribute, char *stat, statCallback);
  void (^baseScope)(baseScopeCallback);
  resource_instance_t * (^find)(jsonapi_params_t *params, char *id);
  resource_instance_collection_t * (^all)(jsonapi_params_t *params);
  resource_instance_t * (^build)(jsonapi_params_t *params);
  struct resource_t * (^lookup)(const char *);
  struct resource_t * (^lookupByModel)(const char *);
} __attribute__((packed)) resource_t;

typedef model_t *(*ModelFunction)(memory_manager_t *);
typedef resource_t *(*ResourceFunction)(model_t *);

typedef struct resource_library_item_t {
  const char *name;
  model_t *Model;
  resource_t *Resource;

} resource_library_item_t;

typedef struct resource_library_t {
  resource_library_item_t *items[100];
  int count;
  void (^add)(const char *name, ModelFunction, ResourceFunction);
} resource_library_t;

query_t *applyFiltersToScope(json_t *filters, query_t *baseScope,
                             resource_t *resource);

query_t *applySortersToScope(json_t *sorters, query_t *baseScope,
                             resource_t *resource);

query_t *applyPaginatorToScope(json_t *paginator, query_t *baseScope,
                               resource_t *resource);

query_t *applyIncludeToScope(json_t *include, query_t *baseScope,
                             resource_t *resource);

query_t *applyQueryToScope(json_t *query, query_t *baseScope,
                           resource_t *resource);

query_t *applyFieldsToScope(json_t *fields, query_t *baseScope,
                            resource_t *resource);

query_t *applyAllFieldsToScope(query_t *baseScope, resource_t *resource);

void nestedIncludes(json_t *includedJSONAPI,
                    resource_instance_collection_t *collection);

void addDefaultFiltersToAttribute(resource_t *resource, model_t *model,
                                  memory_manager_t *appMemoryManager,
                                  char *attributeName, char *attributeType);

resource_instance_collection_t *
createResourceInstanceCollection(resource_t *resource,
                                 model_instance_collection_t *modelCollection,
                                 jsonapi_params_t *params);
resource_instance_t *createResourceInstance(resource_t *resource,
                                            model_instance_t *modelInstance,
                                            jsonapi_params_t *params);
resource_t *CreateResource(char *type, model_t *model);
resource_library_t *initResourceLibrary();

#endif // RESOURCE_H
