#include "models.h"

model_t *PositionModel(memory_manager_t *memoryManager, pg_t *pg,
                       model_store_t *modelStore) {
  model_t *Position = CreateModel("positions", memoryManager, pg, modelStore);

  Position->attribute("title", "varchar(255)", NULL);
  Position->attribute("historical_index", "integer", NULL);
  Position->attribute("active", "boolean", NULL);
  Position->attribute("created_at", "timestamp", NULL);
  Position->attribute("updated_at", "timestamp", NULL);

  Position->belongsTo("employees", "employee_id", NULL);
  Position->belongsTo("departments", "department_id", NULL);

  Position->hasMany("notes", "employee_id", NULL);

  return Position;
}