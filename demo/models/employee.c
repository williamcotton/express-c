#include "models.h"

model_t *EmployeeModel(memory_manager_t *memoryManager, database_pool_t *db,
                       model_store_t *modelStore) {
  model_t *Employee = CreateModel("employees", memoryManager, db, modelStore);

  Employee->attribute("first_name", "varchar(255)", NULL);
  Employee->attribute("last_name", "varchar(255)", NULL);
  Employee->attribute("age", "integer", NULL);
  Employee->attribute("created_at", "timestamp", NULL);
  Employee->attribute("updated_at", "timestamp", NULL);

  Employee->hasMany("positions", "employee_id", NULL);
  Employee->hasMany("team_memberships", "employee_id", NULL);
  Employee->hasMany("teams", "employee_id", NULL);
  Employee->hasMany("notes", "employee_id", NULL);
  Employee->hasMany("tasks", "employee_id", NULL);

  return Employee;
}
