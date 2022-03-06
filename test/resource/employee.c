#include "employee.h"

resource_t *EmployeeResource(request_t *req, model_t *model) {
  resource_t *Employee =
      CreateResource("employees", model, req, req->memoryManager);

  Employee->attribute("name", "text", NULL);
  Employee->attribute("email", "text", NULL);

  Employee->belongsTo("teams", NULL);

  Employee->filter("name", "eq", ^(query_t *scope, char *value) {
    scope->where("name = $", value);
  });

  Employee->sort("name", ^(query_t *scope, char *direction) {
    scope->order("name $", direction);
  });

  Employee->resolve(^(query_t *scope) {
    scope->all();
  });

  return Employee;
}
