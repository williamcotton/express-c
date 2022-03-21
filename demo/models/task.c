#include "models.h"

model_t *TaskModel(memory_manager_t *memoryManager) {
  model_t *Task = CreateModel("tasks", memoryManager);

  Task->attribute("type", "varchar(255)", NULL);
  Task->attribute("created_at", "timestamp", NULL);
  Task->attribute("updated_at", "timestamp", NULL);

  Task->belongsTo("employees", "employee_id", NULL);
  Task->belongsTo("teams", "team_id", NULL);

  return Task;
}