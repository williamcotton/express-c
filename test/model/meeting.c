#include "meeting.h"

model_t *MeetingModel(memory_manager_t *memoryManager) {
  model_t *Meeting = CreateModel("meetings", memoryManager);

  Meeting->attribute("max_size", "integer", NULL);
  Meeting->attribute("date", "date", NULL);
  Meeting->attribute("timestamp", "timestamp with time zone", NULL);
  Meeting->attribute("max_temp", "real", NULL);
  Meeting->attribute("budget", "decimal", NULL);
  Meeting->attribute("open", "boolean", NULL);
  Meeting->attribute("team_id", "integer", NULL);

  Meeting->belongsTo("teams", "team_id", NULL);

  return Meeting;
}
