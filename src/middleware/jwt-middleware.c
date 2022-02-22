#include "jwt-middleware.h"
#include <jwt.h>

middlewareHandler jwtMiddleware(const unsigned char *privateKey,
                                UNUSED const unsigned char *publicKey,
                                char *algorithm) {

  jwt_alg_t jwtAlgorithm = jwt_str_alg(algorithm);

  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)(),
                      UNUSED void (^cleanup)(cleanupHandler)) {
    jwt_middleware_t *jwtmw = malloc(sizeof(jwt_middleware_t));

    jwtmw->sign = Block_copy(^(char *payload) {
      jwt_t *jwt = NULL;
      jwt_new(&jwt);
      jwt_add_grants_json(jwt, payload);
      jwt_set_alg(jwt, jwtAlgorithm, privateKey, sizeof(privateKey));
      return jwt_encode_str(jwt);
    });

    jwtmw->verify = Block_copy(^(char *token) {
      jwt_t *jwt = NULL;
      jwt_new(&jwt);
      jwt_decode(&jwt, token, privateKey, sizeof(privateKey));
      return jwt_get_grants_json(jwt, NULL);
    });

    req->mSet("jwt", jwtmw);

    // Authorization: Bearer <token>
    next();
  });
}
