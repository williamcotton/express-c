#include "team.h"

resource_t *TeamResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Team = CreateResource("teams", model, resourceStore);

  Team->attribute("name", "string", NULL);

  Team->hasMany("employees", NULL);
  Team->hasMany("meetings", NULL);

  return Team;
}
