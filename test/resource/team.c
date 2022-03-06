#include "team.h"

resource_t *TeamResource(request_t *req, model_t *model) {
  resource_t *Team = CreateResource("teams", model, req, req->memoryManager);

  Team->attribute("name", "text", NULL);

  Team->hasMany("employees", NULL);

  return Team;
}
