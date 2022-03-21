#include "models.h"

model_t *DepartmentModel(memory_manager_t *memoryManager, pg_t *pg,
                         model_store_t *modelStore) {
  model_t *Department =
      CreateModel("departments", memoryManager, pg, modelStore);

  Department->attribute("name", "varchar(255)", NULL);
  Department->attribute("created_at", "timestamp", NULL);
  Department->attribute("updated_at", "timestamp", NULL);

  Department->hasMany("positions", "department_id", NULL);
  Department->hasMany("teams", "department_id", NULL);
  Department->hasMany("notes", "department_id", NULL);

  return Department;
}
