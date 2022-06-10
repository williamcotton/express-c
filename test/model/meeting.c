#include "meeting.h"

model_t *MeetingModel(memory_manager_t *memoryManager, database_pool_t *db,
                      model_store_t *modelStore) {
  model_t *Meeting = CreateModel("meetings", memoryManager, db, modelStore);

  Meeting->attribute("max_size", "integer", NULL);
  Meeting->attribute("date", "date", NULL);
  Meeting->attribute("timestamp", "timestamp with time zone", NULL);
  Meeting->attribute("max_temp", "real", NULL);
  Meeting->attribute("budget", "decimal", NULL);
  Meeting->attribute("open", "boolean", NULL);

  Meeting->belongsTo("teams", "team_id", NULL);

  return Meeting;
}
