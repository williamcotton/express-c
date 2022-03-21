#include "models.h"

model_t *MilestoneModel(memory_manager_t *memoryManager) {
  model_t *Milestone = CreateModel("milestones", memoryManager);

  Milestone->attribute("name", "varchar(255)", NULL);
  Milestone->attribute("created_at", "timestamp", NULL);
  Milestone->attribute("updated_at", "timestamp", NULL);

  return Milestone;
}
