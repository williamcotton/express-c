#include "models.h"

model_t *TeamModel(memory_manager_t *memoryManager, database_pool_t *db,
                   model_store_t *modelStore) {
  model_t *Team = CreateModel("teams", memoryManager, db, modelStore);

  Team->attribute("name", "varchar(255)", NULL);
  Team->attribute("created_at", "timestamp", NULL);
  Team->attribute("updated_at", "timestamp", NULL);

  Team->belongsTo("departments", "department_id", NULL);
  Team->hasMany("team_memberships", "team_id", NULL);
  Team->hasMany("employees", "team_id", NULL);
  Team->hasMany("notes", "team_id", NULL);
  Team->hasMany("tasks", "team_id", NULL);

  return Team;
}
