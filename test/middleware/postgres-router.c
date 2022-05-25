#include <express.h>
#include <middleware/postgres-middleware.h>

static void setupTestTable(pg_t *pg) {
  PQclear(pg->exec("DROP TABLE IF EXISTS test"));
  PQclear(pg->exec(
      "CREATE TABLE test (id serial PRIMARY KEY, name text, age integer, "
      "city text)"));
  PQclear(pg->exec("INSERT INTO test (name, age, city) VALUES ('test123', 123, "
                   "'testcity')"));
  PQclear(pg->exec("INSERT INTO test (name, age, city) VALUES ('test456', 456, "
                   "'testcity')"));
  PQclear(pg->exec("INSERT INTO test (name, age, city) VALUES ('test789', 789, "
                   "'anothercity')"));
  PQclear(
      pg->exec("INSERT INTO test (name, age, city) VALUES ('another123', 123, "
               "'anothercity')"));
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

  router->get("/execParams/:id", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    const char *id = req->params("id");
    const char *const paramValues[] = {id};
    PGresult *pgres = pg->execParams("SELECT CONCAT('test-', $1::varchar)", 1,
                                     NULL, paramValues, NULL, NULL, 0);
    res->send(PQgetvalue(pgres, 0, 0));
    PQclear(pgres);
  });

  router->get("/exec/:one/:two", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    PGresult *pgres = pg->exec("SELECT CONCAT($1::varchar, $2::varchar)",
                               req->params("one"), req->params("two"));
    res->send(PQgetvalue(pgres, 0, 0));
    PQclear(pgres);
  });

  router->get("/query/find", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    PGresult *pgres = pg->query("test")->find("2");
    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }
    res->send(PQgetvalue(pgres, 0, 1));
    PQclear(pgres);
  });

  router->get("/query/all", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    PGresult *pgres = pg->query("test")->all();
    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }
    res->send(PQgetvalue(pgres, 0, 1));
    PQclear(pgres);
  });

  router->get("/query/select", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    PGresult *pgres = pg->query("test")->select("city")->all();
    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }
    res->send(PQgetvalue(pgres, 0, 0));
    PQclear(pgres);
  });

  router->get("/query/where", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    PGresult *pgres = pg->query("test")
                          ->where("age = $", "123")
                          ->where("city = $", "anothercity")
                          ->all();
    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }
    res->send(PQgetvalue(pgres, 0, 1));
    PQclear(pgres);
  });

  router->get("/query/where_in", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    const char **paramValues;
    paramValues = (const char **)malloc(sizeof(char *) * 2);
    paramValues[0] = "test123";
    paramValues[1] = "test456";
    PGresult *pgres =
        pg->query("test")->whereIn("name", true, paramValues, 2)->all();
    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }
    res->sendf("%s,%s", PQgetvalue(pgres, 0, 2), PQgetvalue(pgres, 1, 2));
    free(paramValues);
    PQclear(pgres);
  });

  router->get("/query/count", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    int count = pg->query("test")->where("age = $", "123")->count();
    if (count == -1) {
      res->status = 500;
      res->send("<pre>Error</pre>");
      return;
    }
    res->sendf("%d", count);
  });

  router->get("/query/limit", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    PGresult *pgres =
        pg->query("test")->where("age = $", "123")->limit(1)->all();
    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }
    int count = PQntuples(pgres);
    res->sendf("%d", count);
    PQclear(pgres);
  });

  router->get("/query/offset", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    PGresult *pgres =
        pg->query("test")->where("age = $", "123")->offset(1)->all();
    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }
    res->send(PQgetvalue(pgres, 0, 1));
    PQclear(pgres);
  });

  router->get("/query/order", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    PGresult *pgres = pg->query("test")->order("id", "DESC")->all();
    if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
      res->status = 500;
      res->sendf("<pre>%s</pre>", PQresultErrorMessage(pgres));
      PQclear(pgres);
      return;
    }
    res->send(PQgetvalue(pgres, 0, 1));
    PQclear(pgres);
  });

  router->get("/query/tosql", ^(request_t *req, response_t *res) {
    pg_t *pg = req->m("pg");
    setupTestTable(pg);
    char *sql = pg->query("test")
                    ->select("city")
                    ->distinct()
                    ->where("city = $", "test")
                    ->group("city")
                    ->joins("INNER JOIN test2 ON test.name = test2.name")
                    ->having("city = $", "test")
                    ->having("name = $", "test")
                    ->limit(1)
                    ->offset(1)
                    ->order("id", "DESC")
                    ->toSql();
    res->send(sql);
  });

  router->cleanup(Block_copy(^{
    postgres->free();
  }));

  return router;
}
