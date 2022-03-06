#include "employee.h"

resource_t *EmployeeResource(request_t *req, model_t *model) {
  resource_t *Employee =
      CreateResource("employees", model, req, req->memoryManager);

  Employee->attribute("name", "string", NULL);
  Employee->attribute("email", "string", NULL);

  Employee->belongsTo("teams", NULL);

  return Employee;
}
