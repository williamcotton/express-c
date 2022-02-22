#include <express.h>
#include <jansson.h>
#include <jwt.h>
#include <middleware/jwt-middleware.h>
#include <stdlib.h>

router_t *jwtRouter() {
  router_t *router = expressRouter();

  unsigned char key256[32] = "012345678901234567890123456789XY";

  router->use(jwtMiddleware(key256, key256, "HS256"));

  router->get("/sign", ^(request_t *req, response_t *res) {
    jwt_middleware_t *jwt = req->m("jwt");
    char *token = jwt->sign("{\"hello\": \"world\"}");
    res->send(token);
  });

  router->get("/verify", ^(request_t *req, response_t *res) {
    jwt_middleware_t *jwt = req->m("jwt");
    char *token = jwt->sign("{\"hello\": \"world\"}");
    char *decoded = jwt->verify(token);
    res->send(decoded);
  });

  return router;
}
