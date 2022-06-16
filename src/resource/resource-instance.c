#include "resource.h"

static int isMemberOfJsonArray(json_t *array, char *value) {
  json_t *element;

  for (size_t i = 0; i < json_array_size(array); i++) {
    element = json_array_get(array, i);
    if (strcmp(json_string_value(element), value) == 0) {
      return 1;
    }
  }
  return 0;
}

json_t *resourceInstanceIncludeToJSONAPI(resource_instance_t *instance) {
  json_t *includedJSONAPI = json_array();

  for (int i = 0; i < instance->includedResourceInstancesCount; i++) {
    nestedIncludes(includedJSONAPI, instance->includedResourceInstances[i]);

    resourceInstanceCollectionEach(instance->includedResourceInstances[i], ^(
                                       resource_instance_t *relatedInstance) {
      json_t *relatedJSONAPI = resourceInstanceDataJSONAPI(relatedInstance);
      json_array_append_new(includedJSONAPI, relatedJSONAPI);
    });
  }

  if (json_array_size(includedJSONAPI) == 0) {
    json_decref(includedJSONAPI);
    return (json_t *)NULL;
  }

  return includedJSONAPI;
}

json_t *resourceInstanceDataJSONAPI(resource_instance_t *instance) {
  json_t *attributes = json_object();

  json_t *fields = json_object_get(instance->params->query, "fields");
  json_t *resourceFields = NULL;
  if (fields) {
    resourceFields = json_object_get(fields, instance->resource->type);
  }

  for (int i = 0; i < instance->resource->attributesCount; i++) {
    class_attribute_t *attribute = instance->resource->attributes[i];
    json_t *value = NULL;
    char *attributeType = attribute->type;

    if (modelInstanceGet(instance->modelInstance, attribute->name) == NULL) {
      continue;
    }

    if (strcmp(attributeType, "integer") == 0 ||
        strcmp(attributeType, "integer_id") == 0) {
      value = json_integer(
          atoll(modelInstanceGet(instance->modelInstance, attribute->name)));
    } else if (strcmp(attributeType, "big_decimal") == 0 ||
               strcmp(attributeType, "float") == 0) {
      char *eptr;
      value = json_real(strtod(
          modelInstanceGet(instance->modelInstance, attribute->name), &eptr));
    } else if (strcmp(attributeType, "boolean") == 0) {
      value = json_boolean(
          strcmp(modelInstanceGet(instance->modelInstance, attribute->name),
                 "t") == 0 ||
          strcmp(modelInstanceGet(instance->modelInstance, attribute->name),
                 "true") == 0);
    } else {
      value = json_string(
          modelInstanceGet(instance->modelInstance, attribute->name));
    }

    if (resourceFields) {
      if (isMemberOfJsonArray(resourceFields, attribute->name)) {
        json_object_set(attributes, attribute->name, value);
      }
    } else {
      json_object_set(attributes, attribute->name, value);
    }

    mmCleanup(instance->memoryManager, mmBlockCopy(instance->memoryManager, ^{
                json_decref(value);
              }));
  }

  json_t *relationships = json_object();

  for (int i = 0; i < instance->includedResourceInstancesCount; i++) {
    json_t *relatedDataArray = json_array();
    resourceInstanceCollectionEach(instance->includedResourceInstances[i], ^(
                                       resource_instance_t *relatedInstance) {
      json_t *relatedData = json_object();
      json_object_set_new(relatedData, "id", json_string(relatedInstance->id));
      json_object_set_new(relatedData, "type",
                          json_string(relatedInstance->type));

      json_array_append_new(relatedDataArray, relatedData);
    });
    json_t *related = json_object();
    json_object_set_new(related, "data", relatedDataArray);
    json_object_set_new(relationships,
                        instance->includedResourceInstances[i]->type, related);
  }

  for (int i = 0; i < instance->resource->relationshipsCount; i++) {
    char *relatedType = instance->resource->relationshipNames()[i];
    json_t *relatedRelationship = json_object_get(relationships, relatedType);
    int addRelationships = 0;
    if (relatedRelationship == NULL) {
      addRelationships = 1;
    } else {
      json_t *relatedDataArray = json_object_get(relatedRelationship, "data");
      if (json_array_size(relatedDataArray) == 0) {
        addRelationships = 1;
      }
    }
    if (addRelationships) {
      json_t *relatedMeta = json_object();
      json_object_set_new(relatedMeta, "included", json_false());
      json_t *related = json_object();
      json_object_set_new(related, "meta", relatedMeta);
      json_object_set_new(relationships, relatedType, related);
    }
  }

  // TODO: add links

  json_t *dataJSONAPI = json_pack(
      "{s:s, s:s:, s:o, s:o}", "type", instance->type, "id", instance->id,
      "attributes", attributes, "relationships", relationships);

  return dataJSONAPI;
}

json_t *resourceInstanceToJSONAPI(resource_instance_t *instance) {
  json_t *data = resourceInstanceDataJSONAPI(instance);

  json_t *meta = json_object();

  __block json_t *response =
      json_pack("{s:o, s:o}", "data", data, "meta", meta);

  json_t *included = resourceInstanceIncludeToJSONAPI(instance);

  if (included) {
    json_object_set_new(response, "included", included);
  }

  // debug("\n%s", json_dumps(response, JSON_INDENT(2)));

  return response;
}

void instanceHelpers(resource_instance_t *instance) {
  instance->toJSONAPI = mmBlockCopy(instance->memoryManager, ^json_t *() {
    return resourceInstanceToJSONAPI(instance);
  });
}

resource_instance_t *createResourceInstance(resource_t *resource,
                                            model_instance_t *modelInstance,
                                            jsonapi_params_t *params) {
  memory_manager_t *memoryManager = resource->model->memoryManager;

  resource_instance_t *instance =
      mmMalloc(memoryManager, sizeof(resource_instance_t));

  instance->modelInstance = modelInstance;
  instance->params = params;
  instance->memoryManager = memoryManager;
  instance->resource = resource;
  instance->id = modelInstance->id;
  instance->type = resource->type;
  instance->includedResourceInstancesCount = 0;

  return instance;
};
