#include <express.h>
#include <jansson.h>
#include <middleware/jansson-jsonapi-middleware.h>
#include <stdlib.h>

router_t *janssonJsonapiRouter() {
  router_t *router = expressRouter();

  embedded_files_data_t embeddedFiles = {0};
  router->use(janssonJsonapiMiddleware("test/files", embeddedFiles));

  router->get("/", ^(UNUSED request_t *req, response_t *res) {
    res->send("ok");
  });

  return router;
}