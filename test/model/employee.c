#include "employee.h"

model_t *EmployeeModel(request_t *req, pg_t *pg) {
  model_t *Employee = CreateModel("employees", req, pg);

  Employee->attribute("name", "text");
  Employee->attribute("email", "text");

  Employee->belongsTo("teams", "team_id");

  return Employee;
}
