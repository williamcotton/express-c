#include "resources.h"

resource_t *TaskResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Task = CreateResource("tasks", model, resourceStore);

  Task->attribute("type", "string", NULL);
  Task->attribute("created_at", "datetime", NULL);
  Task->attribute("updated_at", "datetime", NULL);

  Task->belongsTo("employees", NULL);
  Task->belongsTo("teams", NULL);

  return Task;
}