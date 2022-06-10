#include "island.h"

resource_t *IslandResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Island = CreateResource("islands", model, resourceStore);

  Island->attribute("name", "string", NULL);

  return Island;
}
