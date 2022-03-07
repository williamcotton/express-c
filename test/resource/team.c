#include "team.h"

resource_t *TeamResource(model_t *model) {
  resource_t *Team = CreateResource("teams", model);

  Team->attribute("name", "string", NULL);

  Team->hasMany("employees", NULL);

  return Team;
}
