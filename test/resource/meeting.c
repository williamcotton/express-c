#include "meeting.h"

resource_t *MeetingResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Meeting = CreateResource("meetings", model, resourceStore);

  Meeting->attribute("max_size", "integer", NULL);
  Meeting->attribute("date", "date", NULL);
  Meeting->attribute("timestamp", "datetime", NULL);
  Meeting->attribute("max_temp", "float", NULL);
  Meeting->attribute("budget", "big_decimal", NULL);
  Meeting->attribute("open", "boolean", NULL);

  Meeting->belongsTo("teams", NULL);

  return Meeting;
}
