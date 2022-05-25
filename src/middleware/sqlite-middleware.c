#include "sqlite-middleware.h"

middlewareHandler sqliteMiddleware(sqlite3 *db) {
  // the sql we will turn in to a prepared statement
  char *sql = "SELECT name, state FROM people WHERE state = ?1;";
  sqlite3_stmt *pstmt = NULL; // prepared statements corresponding to sql
  const unsigned char *name = NULL,
                      *state = NULL; // text columns from our queries
  char *zErrMsg = 0;

  if (sql && db && pstmt && name && state && zErrMsg) {
    debug("%s", sql);
  }

  return Block_copy(^(UNUSED request_t *req, UNUSED response_t *res,
                      void (^next)(), void (^cleanup)(cleanupHandler)) {
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));
    next();
  });
};
