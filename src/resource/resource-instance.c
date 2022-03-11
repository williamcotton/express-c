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

resource_instance_t *createResourceInstance(resource_t *resource,
                                            model_instance_t *modelInstance,
                                            jsonapi_params_t *params) {
  memory_manager_t *memoryManager = resource->model->instanceMemoryManager;

  resource_instance_t *instance =
      memoryManager->malloc(sizeof(resource_instance_t));

  instance->modelInstance = modelInstance;
  instance->id = modelInstance->id;
  instance->type = resource->type;
  instance->includedResourceInstancesCount = modelInstance->includesCount;

  for (int i = 0; i < modelInstance->includesCount; i++) {
    char *includedModelTableName = modelInstance->includesArray[i];
    resource_t *includedResource =
        resource->lookupByModel(includedModelTableName);
    if (includedResource == NULL)
      continue;
    model_instance_collection_t *relatedModelInstances =
        modelInstance->includedModelInstanceCollections[i];
    instance->includedResourceInstances[i] = createResourceInstanceCollection(
        includedResource, relatedModelInstances, params);
  }

  instance->includedToJSONAPI = memoryManager->blockCopy(^json_t *() {
    json_t *includedJSONAPI = json_array();

    for (int i = 0; i < instance->includedResourceInstancesCount; i++) {
      nestedIncludes(includedJSONAPI, instance->includedResourceInstances[i]);
      instance->includedResourceInstances[i]->each(
          ^(resource_instance_t *relatedInstance) {
            json_t *relatedJSONAPI = relatedInstance->dataJSONAPI();
            json_array_append_new(includedJSONAPI, relatedJSONAPI);
          });
    }

    if (json_array_size(includedJSONAPI) == 0) {
      json_decref(includedJSONAPI);
      return (json_t *)NULL;
    }

    return includedJSONAPI;
  });

  instance->dataJSONAPI = memoryManager->blockCopy(^json_t *() {
    json_t *attributes = json_object();

    json_t *fields = json_object_get(params->query, "fields");
    json_t *resourceFields = NULL;
    if (fields) {
      resourceFields = json_object_get(fields, resource->type);
    }

    for (int i = 0; i < resource->attributesCount; i++) {
      class_attribute_t *attribute = resource->attributes[i];
      json_t *value = NULL;
      char *attributeType = attribute->type;

      if (modelInstance->get(attribute->name) == NULL) {
        continue;
      }

      if (strcmp(attributeType, "integer") == 0 ||
          strcmp(attributeType, "integer_id") == 0) {
        value = json_integer(atoll(modelInstance->get(attribute->name)));
      } else if (strcmp(attributeType, "big_decimal") == 0 ||
                 strcmp(attributeType, "float") == 0) {
        char *eptr;
        value = json_real(strtod(modelInstance->get(attribute->name), &eptr));
      } else if (strcmp(attributeType, "boolean") == 0) {
        value = json_boolean(
            strcmp(modelInstance->get(attribute->name), "t") == 0 ||
            strcmp(modelInstance->get(attribute->name), "true") == 0);
      } else {
        value = json_string(modelInstance->get(attribute->name));
      }

      if (resourceFields) {
        if (isMemberOfJsonArray(resourceFields, attribute->name)) {
          json_object_set(attributes, attribute->name, value);
        }
      } else {
        json_object_set(attributes, attribute->name, value);
      }

      memoryManager->cleanup(memoryManager->blockCopy(^{
        json_decref(value);
      }));
    }

    json_t *relationships = json_object();

    for (int i = 0; i < modelInstance->includesCount; i++) {
      instance->includedResourceInstances[i]->each(
          ^(resource_instance_t *relatedInstance) {
            json_t *relatedData = json_object();
            json_object_set_new(relatedData, "id",
                                json_string(relatedInstance->id));
            json_object_set_new(relatedData, "type",
                                json_string(relatedInstance->type));
            json_t *related = json_object();
            json_object_set_new(related, "data", relatedData);
            json_object_set_new(relationships, relatedInstance->type, related);
          });
    }

    for (int i = 0; i < resource->relationshipsCount; i++) {
      char *relatedType = resource->relationshipNames()[i];
      json_t *relatedRelationship = json_object_get(relationships, relatedType);
      if (relatedRelationship == NULL) {
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
  });

  instance->toJSONAPI = memoryManager->blockCopy(^json_t *() {
    json_t *data = instance->dataJSONAPI();

    json_t *meta = json_object();

    __block json_t *response =
        json_pack("{s:o, s:o}", "data", data, "meta", meta);

    json_t *included = instance->includedToJSONAPI();

    if (included) {
      json_object_set_new(response, "included", included);
    }

    memoryManager->cleanup(memoryManager->blockCopy(^{
      json_decref(response);
    }));

    return response;
  });

  return instance;
};
