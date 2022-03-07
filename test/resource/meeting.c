#include "meeting.h"

resource_t *MeetingResource(model_t *model) {
  resource_t *Meeting = CreateResource("meetings", model);

  Meeting->attribute("max_size", "integer", NULL);
  Meeting->attribute("date", "date", NULL);
  Meeting->attribute("timestamp", "datetime", NULL);
  Meeting->attribute("max_temp", "float", NULL);
  Meeting->attribute("budget", "big_decimal", NULL);
  Meeting->attribute("open", "boolean", NULL);
  Meeting->attribute("team_id", "integer", NULL);

  Meeting->belongsTo("teams", "team_id");

  return Meeting;
}
