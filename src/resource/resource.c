#include "resource.h"

resource_t *CreateResource(char *type, model_t *model, void *context,
                           memory_manager_t *memoryManager) {
  resource_t *resource = memoryManager->malloc(sizeof(resource_t));
  resource->type = type;
  resource->model = model;
  resource->context = context;

  resource->attributesCount = 0;
  resource->belongsToCount = 0;
  resource->hasManyCount = 0;

  resource->attribute =
      memoryManager->blockCopy(^(char *attributeName, char *attributeType) {
        class_attribute_t *newAttribute =
            memoryManager->malloc(sizeof(class_attribute_t));
        newAttribute->name = attributeName;
        newAttribute->type = attributeType;
        resource->attributes[resource->attributesCount] = newAttribute;
        resource->attributesCount++;
      });

  resource->belongsTo =
      memoryManager->blockCopy(^(char *relatedResourceName, char *foreignKey) {
        belongs_to_resource_t *newBelongsTo =
            memoryManager->malloc(sizeof(belongs_to_resource_t));
        newBelongsTo->resourceName = relatedResourceName;
        newBelongsTo->foreignKey = foreignKey;
        resource->belongsToRelationships[resource->belongsToCount] =
            newBelongsTo;
        resource->belongsToCount++;
      });

  resource->hasMany =
      memoryManager->blockCopy(^(char *relatedResourceName, char *foreignKey) {
        has_many_resource_t *newHasMany =
            memoryManager->malloc(sizeof(has_many_resource_t));
        newHasMany->resourceName = relatedResourceName;
        newHasMany->foreignKey = foreignKey;
        resource->hasManyRelationships[resource->hasManyCount] = newHasMany;
        resource->hasManyCount++;
      });

  return resource;
}
