#include "employee.h"

model_t *EmployeeModel(request_t *req, pg_t *pg) {
  model_t *Employee = CreateModel("employees", req, pg);

  Employee->attribute("name", "text", NULL);
  Employee->attribute("email", "text", NULL);
  Employee->attribute("team_id", "integer", NULL);

  Employee->belongsTo("teams", "team_id", NULL);

  Employee->validatesAttribute("team_id", "presence", NULL);

  return Employee;
}
