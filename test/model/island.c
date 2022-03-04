#include "island.h"

model_t *IslandModel(request_t *req, pg_t *pg) {
  model_t *Island = CreateModel("islands", req, pg);

  return Island;
}
