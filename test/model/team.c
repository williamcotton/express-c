#include "team.h"

model_t *TeamModel(memory_manager_t *memoryManager, database_pool_t *db,
                   model_store_t *modelStore) {
  model_t *Team = CreateModel("teams", memoryManager, db, modelStore);

  Team->attribute("name", "text", NULL);

  Team->validatesAttribute("name", "presence", NULL);
  Team->validates(^(team_t *team) {
    if (team->get("name") && strlen(team->get("name")) < 3) {
      team->addError("name", "must be at least 3 characters long");
    }
  });

  Team->hasMany("employees", "team_id", NULL);
  Team->hasMany("meetings", "team_id", NULL);

  return Team;
}
