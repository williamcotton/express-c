#include <express.h>
#include <jansson.h>
#include <middleware/jansson-mustache-middleware.h>
#include <stdlib.h>

router_t *janssonMustacheRouter() {
  router_t *router = expressRouter();

  embedded_files_data_t embeddedFiles = {0};
  router->use(janssonMustacheMiddleware("test/files", embeddedFiles));

  router->get("/", ^(UNUSED request_t *req, response_t *res) {
    json_t *json = json_object();
    json_object_set_new(json, "title", json_string("test"));

    res->render("test", json);
  });

  return router;
}
