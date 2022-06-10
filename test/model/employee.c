#include "employee.h"

model_t *EmployeeModel(memory_manager_t *memoryManager, database_pool_t *db,
                       model_store_t *modelStore) {
  model_t *Employee = CreateModel("employees", memoryManager, db, modelStore);

  Employee->attribute("name", "text", NULL);
  Employee->attribute("email", "text", NULL);

  Employee->belongsTo("teams", "team_id", NULL);
  Employee->hasMany("notes", "employee_id", NULL);

  Employee->validatesAttribute("team_id", "presence", NULL);

  return Employee;
}
