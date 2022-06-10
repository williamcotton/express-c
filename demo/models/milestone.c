#include "models.h"

model_t *MilestoneModel(memory_manager_t *memoryManager, database_pool_t *db,
                        model_store_t *modelStore) {
  model_t *Milestone = CreateModel("milestones", memoryManager, db, modelStore);

  Milestone->attribute("name", "varchar(255)", NULL);
  Milestone->attribute("created_at", "timestamp", NULL);
  Milestone->attribute("updated_at", "timestamp", NULL);

  return Milestone;
}
