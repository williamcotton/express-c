#include "resources.h"

resource_t *DepartmentResource(model_t *model,
                               resource_store_t *resourceStore) {
  resource_t *Department = CreateResource("departments", model, resourceStore);

  Department->attribute("name", "string", NULL);
  Department->attribute("created_at", "datetime", NULL);
  Department->attribute("updated_at", "datetime", NULL);

  Department->hasMany("positions", NULL);
  Department->hasMany("teams", NULL);
  Department->hasMany("notes", NULL);

  return Department;
}
