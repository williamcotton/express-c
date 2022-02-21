#include <cJSON/cJSON.h>
#include <express.h>
#include <middleware/cjson-mustache-middleware.h>
#include <stdlib.h>

router_t *cJSONMustacheRouter() {
  router_t *router = expressRouter();

  embedded_files_data_t embeddedFiles = {0};
  router->use(cJSONMustacheMiddleware("test/files", embeddedFiles));

  router->get("/", ^(UNUSED request_t *req, response_t *res) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "title", "test");

    res->render("test", json);
  });

  return router;
}
