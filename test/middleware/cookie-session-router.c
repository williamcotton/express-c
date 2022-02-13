#include <cJSON/cJSON.h>
#include <express.h>
#include <middleware/cookie-session-middleware.h>
#include <stdlib.h>

router_t *cookieSessionRouter() {
  router_t *router = expressRouter();

  router->use(cookieSessionMiddlewareFactory());

  router->get("/set", ^(UNUSED request_t *req, response_t *res) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "title", "test");
    req->session->set("test", json);
    res->send("ok");
  });

  router->get("/get", ^(UNUSED request_t *req, response_t *res) {
    cJSON *json = req->session->get("test");
    char *str = cJSON_PrintUnformatted(json);
    res->send(str);
    free(str);
  });

  return router;
}
