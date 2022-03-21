#include "resources.h"

resource_t *EmployeeResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Employee = CreateResource("employees", model, resourceStore);

  Employee->attribute("first_name", "string", NULL);
  Employee->attribute("last_name", "string", NULL);
  Employee->attribute("age", "integer", NULL);
  Employee->attribute("created_at", "datetime", NULL);
  Employee->attribute("updated_at", "datetime", NULL);

  Employee->hasMany("positions", NULL);
  Employee->hasMany("team_memberships", NULL);
  Employee->hasMany("teams", NULL);
  Employee->hasMany("notes", NULL);
  Employee->hasMany("tasks", NULL);

  return Employee;
}
