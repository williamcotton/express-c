#include "../../src/express.h"
#include "../../src/middleware/postgres-middleware.h"

static void setupTestTable(pg_t *pg) {
  pg->exec("DROP TABLE IF EXISTS test");
  pg->exec("CREATE TABLE test (id serial PRIMARY KEY, name text, age integer)");
  pg->exec("INSERT INTO test (name, age) VALUES ('test123', 123)");
  pg->exec("INSERT INTO test (name, age) VALUES ('test456', 456)");
  pg->exec("INSERT INTO test (name, age) VALUES ('test789', 789)");
};

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
    res->send(PQgetvalue(pgres, 0, 0));
    PQclear(pgres);
  });

  router->get("/execParams/:id", ^(request_t *req, UNUSED response_t *res) {
    pg_t *pg = req->m("pg");
    const char *id = req->params("id");
    const char *const paramValues[] = {id};
    PGresult *pgres = pg->execParams("SELECT CONCAT('test-', $1::varchar)", 1,
                                     NULL, paramValues, NULL, NULL, 0);
    res->send(PQgetvalue(pgres, 0, 0));
    PQclear(pgres);
  });

  router->get("/exec/:one/:two", ^(request_t *req, UNUSED response_t *res) {
    pg_t *pg = req->m("pg");
    PGresult *pgres = pg->exec("SELECT CONCAT($1::varchar, $2::varchar)",
                               req->params("one"), req->params("two"));
    res->send(PQgetvalue(pgres, 0, 0));
    PQclear(pgres);
  });

  router->get("/query/all", ^(request_t *req, UNUSED response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    PGresult *pgres = pg->query("test")->all();
    res->send(PQgetvalue(pgres, 0, 0));
    PQclear(pgres);
  });

  router->get("/query/where", ^(request_t *req, UNUSED response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    PGresult *pgres = pg->query("test")->where("age = ?", "456")->all();
    res->send(PQgetvalue(pgres, 0, 0));
    PQclear(pgres);
  });

  router->cleanup(Block_copy(^{
    postgres->free();
  }));

  return router;
}
