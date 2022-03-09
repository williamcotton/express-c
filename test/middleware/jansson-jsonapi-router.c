#include <express.h>
#include <jansson.h>
#include <middleware/jansson-jsonapi-middleware.h>
#include <stdlib.h>

router_t *janssonJsonapiRouter() {
  router_t *router = expressRouter();

  router->use(janssonJsonapiMiddleware("/jansson-jsonapi"));

  router->get("/", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");
    check(jsonapi != NULL, "jsonapi middleware not found");
    return res->send("ok");
  error:
    res->send("not ok");
  });

  router->get("/query", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");
    check(jsonapi != NULL, "jsonapi middleware not found");
    check(jsonapi->params->query != NULL, "query not found");
    return res->s("jsonapi", jsonapi->params->query);
  error:
    res->send("not ok");
  });

  router->post("/", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");
    check(jsonapi != NULL, "jsonapi middleware not found");
    json_t *data = json_object_get(jsonapi->params->body, "data");
    check(data != NULL, "data not found");
    json_t *type = json_object_get(data, "type");
    check(type != NULL, "type not found");
    const char *typeString = json_string_value(type);
    check(typeString != NULL, "type not a string");
    return res->send(typeString);
  error:
    res->send("not ok");
  });

  return router;
}
