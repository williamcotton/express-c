#include "employee.h"

model_t *EmployeeModel(request_t *req, pg_t *pg) {
  model_t *Employee = CreateModel("employees", req, pg);

  Employee->attribute("name", "text");
  Employee->attribute("email", "text");
  Employee->attribute("team_id", "integer");

  Employee->belongsTo("teams", "team_id");

  Employee->validatesAttribute("team_id", "presence");

  return Employee;
}
