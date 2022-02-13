#include <cJSON/cJSON.h>
#include <express.h>
#include <middleware/mustache-middleware.h>
#include <stdlib.h>

router_t *mustacheRouter() {
  router_t *router = expressRouter();

  embedded_files_data_t embeddedFiles = {0};
  router->use(mustacheMiddleware("test/files", embeddedFiles));

  router->get("/", ^(UNUSED request_t *req, response_t *res) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "title", "test");

    res->render("test", json);
  });

  return router;
}
