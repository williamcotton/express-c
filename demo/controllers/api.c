#include "../../src/express.h"
#include <Block.h>
#include <postgresMiddleware/postgresMiddleware.h>

#define POOL_SIZE 30

router_t *apiController(const char *pgUri) {
  router_t *router = expressRouter();

  postgres_connection_t *postgres = initPostgressConnection(pgUri, POOL_SIZE);

  router->use(postgresMiddlewareFactory(postgres));

  router->get("/todos", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    PGresult *pgres = pg->exec(
        "SELECT json_build_object('todos', json_agg(todos)) FROM todos");

    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }

    char *json = PQgetvalue(pgres, 0, 0);
    res->json(json);
    PQclear(pgres);
  });

  router->cleanup(Block_copy(^{
    freePostgresConnection(postgres);
  }));

  return router;
}
