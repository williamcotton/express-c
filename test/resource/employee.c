#include "employee.h"

resource_t *EmployeeResource(request_t *req, model_t *model) {
  resource_t *Employee = CreateResource("employees", req, model);

  Employee->attribute("name", "text");
  Employee->attribute("email", "text");

  Employee->belongsTo("teams", "team_id");

  return Employee;
}
