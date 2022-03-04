#include "team.h"

resource_t *TeamResource(request_t *req, model_t *model) {
  resource_t *Team = CreateResource("teams", req, model);

  Team->attribute("name", "text");

  Team->hasMany("employees", "employee.team_id");

  return Team;
}
