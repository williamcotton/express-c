#include <express.h>
#include <jansson.h>
#include <middleware/jansson-jsonapi-middleware.h>
#include <stdlib.h>

router_t *janssonJsonapiRouter() {
  router_t *router = expressRouter();

  router->use(janssonJsonapiMiddleware());

  router->get("/", ^(UNUSED request_t *req, response_t *res) {
    jsonapi_params_t *jsonapi = req->m("jsonapi");
    if (jsonapi != NULL) {
      res->send("ok");
      return;
    }
    res->send("not ok");
  });

  router->get("/query", ^(request_t *req, response_t *res) {
    jsonapi_params_t *jsonapi = req->m("jsonapi");
    if (jsonapi != NULL && jsonapi->query != NULL) {
      char *jsonString = json_dumps(jsonapi->query, 0);
      res->send(jsonString);
      free(jsonString);
    }
    res->send("not ok");
  });

  router->post("/", ^(UNUSED request_t *req, response_t *res) {
    jsonapi_params_t *jsonapi = req->m("jsonapi");
    if (jsonapi != NULL) {
      json_t *data = json_object_get(jsonapi->body, "data");
      json_t *type = json_object_get(data, "type");
      const char *typeString = json_string_value(type);
      res->send(typeString);
      return;
    }
    res->send("not ok");
  });

  return router;
}