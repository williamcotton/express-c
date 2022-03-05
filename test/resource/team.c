#include "team.h"

resource_t *TeamResource(request_t *req, model_t *model) {
  resource_t *Team = CreateResource("teams", req, model);

  Team->attribute("name", "text", NULL);

  Team->hasMany("employees", NULL);

  return Team;
}
