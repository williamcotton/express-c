#include "employee.h"

resource_t *EmployeeResource(model_t *model) {
  resource_t *Employee = CreateResource("employees", model);

  Employee->attribute("name", "string", NULL);
  Employee->attribute("email", "string", NULL);

  Employee->belongsTo("teams", NULL);
  Employee->hasMany("notes", NULL);

  return Employee;
}
