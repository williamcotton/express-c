#include "employee.h"

model_t *EmployeeModel(memory_manager_t *memoryManager, pg_t *pg,
                       model_store_t *modelStore) {
  model_t *Employee = CreateModel("employees", memoryManager, pg, modelStore);

  Employee->attribute("name", "text", NULL);
  Employee->attribute("email", "text", NULL);

  Employee->belongsTo("teams", "team_id", NULL);
  Employee->hasMany("notes", "employee_id", NULL);

  Employee->validatesAttribute("team_id", "presence", NULL);

  return Employee;
}
