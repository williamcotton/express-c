#include "employee.h"

resource_t *EmployeeResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Employee = CreateResource("employees", model, resourceStore);

  Employee->attribute("name", "string", NULL);
  Employee->attribute("email", "string", NULL);

  Employee->belongsTo("teams", NULL);
  Employee->hasMany("notes", NULL);

  return Employee;
}
