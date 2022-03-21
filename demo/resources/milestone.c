#include "resources.h"

resource_t *MilestoneResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Milestone = CreateResource("milestones", model, resourceStore);

  Milestone->attribute("name", "string", NULL);
  Milestone->attribute("created_at", "datetime", NULL);
  Milestone->attribute("updated_at", "datetime", NULL);

  return Milestone;
}
