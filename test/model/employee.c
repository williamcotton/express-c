#include "employee.h"

model_t *EmployeeModel(memory_manager_t *memoryManager) {
  model_t *Employee = CreateModel("employees", memoryManager);

  Employee->attribute("name", "text", NULL);
  Employee->attribute("email", "text", NULL);
  Employee->attribute("team_id", "integer", NULL);

  Employee->belongsTo("teams", "team_id", NULL);
  Employee->hasMany("notes", "employee_id", NULL);

  Employee->validatesAttribute("team_id", "presence", NULL);

  return Employee;
}
