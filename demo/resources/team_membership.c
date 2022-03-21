#include "resources.h"

resource_t *TeamMembershipResource(model_t *model,
                                   resource_store_t *resourceStore) {
  resource_t *TeamMembership =
      CreateResource("team_memberships", model, resourceStore);

  TeamMembership->attribute("created_at", "datetime", NULL);
  TeamMembership->attribute("updated_at", "datetime", NULL);

  TeamMembership->belongsTo("teams", NULL);
  TeamMembership->belongsTo("employees", NULL);

  return TeamMembership;
}
