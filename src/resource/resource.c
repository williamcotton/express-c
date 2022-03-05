#include "resource.h"

resource_t *CreateResource(char *type, request_t *req, model_t *model) {
  resource_t *resource = req->malloc(sizeof(resource_t));
  resource->type = type;
  resource->model = model;

  resource->attributesCount = 0;
  resource->belongsToCount = 0;
  resource->hasManyCount = 0;

  resource->attribute =
      req->blockCopy(^(char *attributeName, char *attributeType) {
        class_attribute_t *newAttribute =
            req->malloc(sizeof(class_attribute_t));
        newAttribute->name = attributeName;
        newAttribute->type = attributeType;
        resource->attributes[resource->attributesCount] = newAttribute;
        resource->attributesCount++;
      });

  resource->belongsTo =
      req->blockCopy(^(char *relatedResourceName, char *foreignKey) {
        belongs_to_resource_t *newBelongsTo =
            req->malloc(sizeof(belongs_to_resource_t));
        newBelongsTo->resourceName = relatedResourceName;
        newBelongsTo->foreignKey = foreignKey;
        resource->belongsToRelationships[resource->belongsToCount] =
            newBelongsTo;
        resource->belongsToCount++;
      });

  resource->hasMany =
      req->blockCopy(^(char *relatedResourceName, char *foreignKey) {
        has_many_resource_t *newHasMany =
            req->malloc(sizeof(has_many_resource_t));
        newHasMany->resourceName = relatedResourceName;
        newHasMany->foreignKey = foreignKey;
        resource->hasManyRelationships[resource->hasManyCount] = newHasMany;
        resource->hasManyCount++;
      });

  return resource;
}
