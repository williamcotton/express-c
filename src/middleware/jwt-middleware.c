#include "jwt-middleware.h"
#include <jwt.h>

middlewareHandler jwtMiddleware(const unsigned char *privateKey,
                                UNUSED const unsigned char *publicKey,
                                char *algorithm) {

  jwt_alg_t jwtAlgorithm = jwt_str_alg(algorithm);

  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    jwt_middleware_t *jwtmw = malloc(sizeof(jwt_middleware_t));

    jwtmw->sign = Block_copy(^(char *jsonPayload) {
      jwt_t *jwt = NULL;
      jwt_new(&jwt);
      jwt_add_grants_json(jwt, jsonPayload);
      jwt_set_alg(jwt, jwtAlgorithm, privateKey, sizeof(privateKey));
      char *jwtToken = jwt_encode_str(jwt);
      size_t jwtTokenLength = strlen(jwtToken) + 1;
      char *token = req->malloc(jwtTokenLength);
      strlcpy(token, jwtToken, jwtTokenLength);
      free(jwtToken);
      jwt_free(jwt);
      return token;
    });

    jwtmw->verify = Block_copy(^(char *token) {
      jwt_t *jwt = NULL;
      jwt_decode(&jwt, token, privateKey, sizeof(privateKey));
      char *jwtDecoded = jwt_get_grants_json(jwt, NULL);
      size_t jwtDecodedLength = strlen(jwtDecoded) + 1;
      char *decodedJson = req->malloc(jwtDecodedLength);
      strlcpy(decodedJson, jwtDecoded, jwtDecodedLength);
      free(jwtDecoded);
      jwt_free(jwt);
      return decodedJson;
    });

    req->mSet("jwt", jwtmw);

    cleanup(Block_copy(^(request_t *finishedReq) {
      jwt_middleware_t *jwtF = finishedReq->m("jwt");
      Block_release(jwtF->sign);
      Block_release(jwtF->verify);
      free(jwtF);
    }));

    // Authorization: Bearer <token>
    next();
  });
}
