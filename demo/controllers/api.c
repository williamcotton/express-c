#include "../../src/express.h"
#include <Block.h>
#include <postgresMiddleware/postgresMiddleware.h>

router_t *apiController(const char *pgUri) {
  router_t *router = expressRouter(0);

  router->use(postgresMiddlewareFactory(pgUri));

  router->get("/todos", ^(UNUSED request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    PGresult *pgres = pg->exec(
        "SELECT json_build_object('todos', json_agg(todos)) FROM todos");
    char *json = PQgetvalue(pgres, 0, 0);
    res->json(json);
    PQclear(pgres);
  });

  return router;
}
