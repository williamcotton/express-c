#include "../../src/express.h"
#include "../../src/middleware/postgres-middleware.h"

router_t *postgresRouter(const char *pgUri, int poolSize) {
  __block router_t *router = expressRouter();

  postgres_connection_t *postgres = initPostgressConnection(pgUri, poolSize);

  router->use(postgresMiddlewareFactory(postgres));

  router->get("/exec", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    PGresult *pgres = pg->exec("SELECT CONCAT('test', '123')");

    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }

    char *now = PQgetvalue(pgres, 0, 0);
    res->send(now);
    PQclear(pgres);
  });

  router->get("/execParams/:id", ^(request_t *req, UNUSED response_t *res) {
    pg_t *pg = req->m("pg");
    const char *id = req->params("id");
    const char *const paramValues[] = {id};
    PGresult *pgres = pg->execParams("SELECT CONCAT('test-', $1::varchar)", 1,
                                     NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }

    const char *test = PQgetvalue(pgres, 0, 0);
    res->send(test);
    PQclear(pgres);
  });

  router->get("/exec/:one/:two", ^(request_t *req, UNUSED response_t *res) {
    pg_t *pg = req->m("pg");
    PGresult *pgres = pg->exec("SELECT CONCAT($1::varchar, $2::varchar)",
                               req->params("one"), req->params("two"));

    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }

    const char *test = PQgetvalue(pgres, 0, 0);
    res->send(test);
    PQclear(pgres);
  });

  router->cleanup(Block_copy(^{
    postgres->free();
  }));

  return router;
}
