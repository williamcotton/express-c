#include "adapter.h"

int pgParamCount(const char *query);

database_pool_t *databasePoolCreate(const char *uri, int size) {
  database_pool_t *pool = malloc(sizeof(database_pool_t));
  pool->uri = uri;
  pool->size = size;
  pool->head = NULL;
  pool->semaphore = dispatch_semaphore_create(size);
  pool->queue = dispatch_queue_create(pool->uri, NULL);
  return pool;
}

database_connection_t *createPostgresConnection(const char *uri) {
  database_connection_t *connection = malloc(sizeof(database_connection_t));
  connection->next = NULL;
  connection->connection = PQconnectdb(uri);
  return connection;
}

database_pool_t *createPostgresPool(const char *uri, int size) {
  database_pool_t *pool = databasePoolCreate(uri, size);
  database_connection_t *connection = createPostgresConnection(uri);
  database_connection_t *c = connection;
  for (int i = 0; i < size; i++) {
    c->next = createPostgresConnection(uri);
    c = c->next;
  }
  pool->head = connection;

  pool->borrow = Block_copy(^() {
    __block database_connection_t *current = NULL;
    dispatch_semaphore_wait(pool->semaphore, DISPATCH_TIME_FOREVER);
    dispatch_sync(pool->queue, ^{
      current = pool->head;
      pool->head = current->next;
    });
    if (PQstatus(current->connection) != CONNECTION_OK) {
      PQreset(current->connection);
    }
    return current;
  });

  pool->release = Block_copy(^(database_connection_t *current) {
    dispatch_sync(pool->queue, ^{
      current->next = pool->head;
      pool->head = current;
    });
    dispatch_semaphore_signal(pool->semaphore);
  });

  pool->exec = Block_copy(^(const char *sql, ...) {
    int nParams = pgParamCount(sql);
    va_list args;
    va_start(args, sql);

    const char **paramValues =
        nParams == 0 ? NULL : malloc(sizeof(char *) * nParams);
    for (int i = 0; i < nParams; i++) {
      paramValues[i] = va_arg(args, const char *);
      if (i > 1024) {
        log_err("Too many parameters");
        return (PGresult *)NULL;
      }
    }
    va_end(args);

    database_connection_t *current = pool->borrow();

    PGresult *pgres = PQexecParams(current->connection, sql, nParams, NULL,
                                   paramValues, NULL, NULL, 0);

    pool->release(current);

    free(paramValues);

    return pgres;
  });

  pool->execParams =
      Block_copy(^(const char *sql, int nParams, const Oid *paramTypes,
                   const char *const *paramValues, const int *paramLengths,
                   const int *paramFormats, int resultFormat) {
        database_connection_t *current = pool->borrow();

        PGresult *pgres =
            PQexecParams(current->connection, sql, nParams, paramTypes,
                         paramValues, paramLengths, paramFormats, resultFormat);

        pool->release(current);
        return pgres;
      });

  pool->free = Block_copy(^() {
    database_connection_t *current = pool->head;
    database_connection_t *last;
    while (current != NULL) {
      PQfinish(current->connection);
      last = current;
      current = current->next;
      free(last);
    }
    Block_release(pool->exec);
    Block_release(pool->execParams);
    Block_release(pool->borrow);
    Block_release(pool->release);
    dispatch_async(dispatch_get_main_queue(), ^() {
      Block_release(pool->free);
      free(pool);
    });
  });

  return pool;
}
