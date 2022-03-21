#include "resources.h"

resource_t *PositionResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Position = CreateResource("positions", model, resourceStore);

  Position->attribute("title", "string", NULL);
  Position->attribute("historical_index", "integer", NULL);
  Position->attribute("active", "boolean", NULL);
  Position->attribute("created_at", "datetime", NULL);
  Position->attribute("updated_at", "datetime", NULL);

  Position->belongsTo("employees", NULL);
  Position->belongsTo("departments", NULL);

  Position->hasMany("notes", NULL);

  return Position;
}
