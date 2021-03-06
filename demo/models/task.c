#include "models.h"

model_t *TaskModel(memory_manager_t *memoryManager, database_pool_t *db,
                   model_store_t *modelStore) {
  model_t *Task = CreateModel("tasks", memoryManager, db, modelStore);

  Task->attribute("type", "varchar(255)", NULL);
  Task->attribute("created_at", "timestamp", NULL);
  Task->attribute("updated_at", "timestamp", NULL);

  Task->belongsTo("employees", "employee_id", NULL);
  Task->belongsTo("teams", "team_id", NULL);

  return Task;
}