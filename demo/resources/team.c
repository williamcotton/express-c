#include "resources.h"

resource_t *TeamResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Team = CreateResource("teams", model, resourceStore);

  Team->attribute("name", "string", NULL);
  Team->attribute("created_at", "datetime", NULL);
  Team->attribute("updated_at", "datetime", NULL);

  Team->belongsTo("departments", NULL);
  Team->hasMany("team_memberships", NULL);
  Team->hasMany("employees", NULL);
  Team->hasMany("notes", NULL);
  Team->hasMany("tasks", NULL);

  return Team;
}
