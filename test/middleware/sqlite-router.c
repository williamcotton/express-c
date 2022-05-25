#include <express.h>
#include <middleware/sqlite-middleware.h>

UNUSED static void setupTestTable(UNUSED sqlite3 *db){};

router_t *sqliteRouter(sqlite3 *db) {
  __block router_t *router = expressRouter();

  // sqlite3 *db = NULL;

  router->use(sqliteMiddleware(db));

  router->get("/exec", ^(UNUSED request_t *req, UNUSED response_t *res) {
    res->send("test123");
  });

  router->cleanup(Block_copy(^{
  }));

  return router;
}
