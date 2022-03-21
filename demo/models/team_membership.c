#include "models.h"

model_t *TeamMembershipModel(memory_manager_t *memoryManager, pg_t *pg,
                             model_store_t *modelStore) {
  model_t *TeamMembership =
      CreateModel("team_memberships", memoryManager, pg, modelStore);

  TeamMembership->attribute("created_at", "timestamp", NULL);
  TeamMembership->attribute("updated_at", "timestamp", NULL);

  TeamMembership->belongsTo("teams", "team_id", NULL);
  TeamMembership->belongsTo("employees", "employee_id", NULL);

  return TeamMembership;
}
