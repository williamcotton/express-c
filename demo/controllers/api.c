#include "../../src/express.h"

router_t *apiController() {
  router_t *router = expressRouter(0);

  router->get("/", ^(UNUSED request_t *req, response_t *res) {
    debug("GET /api");
    res->send("Hello from API");
  });

  return router;
}
