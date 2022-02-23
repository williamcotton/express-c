# Postgres Middleware

```c
#include "postgres-middleware.h"
#include <Block.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
```

### Initialize Connection

Given a postgres database URI, eg, `postgresql://postgres:postgres@localhost:5432/express-demo?sslmode=disable`, and a pool size, eg, `5`.

Our connection pool is thread-safe through the use of a [`libdispatch`](https://apple.github.io/swift-corelibs-libdispatch/tutorial/) semaphore and serial queue.

```c
postgres_connection_t *initPostgressConnection(const char *pgUri,
                                               int poolSize) {
  postgres_connection_t *postgres = malloc(sizeof(postgres_connection_t));
  check(postgres, "Failed to allocate postgres connection pool");

  postgres->uri = pgUri;

  postgres->poolSize = poolSize;

  postgres->pool = malloc(sizeof(pg_t *) * postgres->poolSize);
  check(postgres->pool, "Failed to allocate postgres connection pool");

  postgres->semaphore = dispatch_semaphore_create(postgres->poolSize);

  postgres->queue = dispatch_queue_create("postgres", NULL);

  for (int i = 0; i < postgres->poolSize; i++) {
    postgres->pool[i] = malloc(sizeof(pg_t));
    postgres->pool[i]->connection = PQconnectdb(postgres->uri);
    postgres->pool[i]->used = 0;
    postgres->pool[i]->exec = Block_copy(^(const char *sql) {
      PGresult *pgres = PQexec(postgres->pool[i]->connection, sql);
      return pgres;
    });
    postgres->pool[i]->close = Block_copy(^{
      PQfinish(postgres->pool[i]->connection);
    });
  }

  postgres->free = Block_copy(^() {
    for (int i = 0; i < postgres->poolSize; i++) {
      postgres->pool[i]->close();
      Block_release(postgres->pool[i]->exec);
      Block_release(postgres->pool[i]->execParams);
      Block_release(postgres->pool[i]->close);
      free(postgres->pool[i]);
    }
    free(postgres->pool);
    dispatch_release(postgres->semaphore);
    dispatch_release(postgres->queue);
    dispatch_async(dispatch_get_main_queue(), ^() {
      Block_release(postgres->free);
      free(postgres);
    });
  });

  return postgres;
error:
  return NULL;
}
```

### Middleware Factory

Before returning our middleware we check that we can connect to the database.

```c
middlewareHandler postgresMiddlewareFactory(postgres_connection_t *postgres) {
  /* Test connection */
  PGconn *connection = PQconnectdb(postgres->uri);
  check(PQstatus(connection) == CONNECTION_OK,
        "Failed to connect to postgres: %s (%s)", PQerrorMessage(connection),
        postgres->uri);
  PQfinish(connection);
```

### Middleware

On each request we acquire a connection from the pool and release it upon completion.

First we call `dispatch_semaphore_wait` to wait for a connection to become available.

Once we have a connection we obtain a mutex using `dispatch_sync` on our serial dispatch queue and then mark the connection as in use.

We then set the middleware on the request.

In the cleanup block we obtain another mutex, mark the connection as available, and then signal a release using `dispatch_semaphore_signal`.

```c
  /* Create middleware, getting a postgress connection at the beginning of every
   * request and releasing it after the request has finished */
  return Block_copy(^(UNUSED request_t *req, UNUSED response_t *res,
                      void (^next)(), void (^cleanup)(cleanupHandler)) {
    /* Wait for a connection */
    dispatch_semaphore_wait(postgres->semaphore, DISPATCH_TIME_FOREVER);

    __block pg_t *pg;

    /* Get connection */
    dispatch_sync(postgres->queue, ^{
      for (int i = 0; i < postgres->poolSize; i++) {
        if (postgres->pool[i]->used == 0) {
          postgres->pool[i]->used = 1;
          pg = postgres->pool[i];
          break;
        }
      }
    });

    req->mSet("pg", pg);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq) {
      /* Release connection */
      dispatch_sync(postgres->queue, ^{
        pg->used = 0;
      });

      /* Signal a release */
      dispatch_semaphore_signal(postgres->semaphore);
    }));

    next();
  });
```

### Error Handling

If there was no postgres connection available we free the connection and return an empty middlware.

```c
error:
  PQfinish(connection);
  return ^(UNUSED request_t *req, UNUSED response_t *res, void (^next)(),
           void (^cleanup)(cleanupHandler)) {
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));
    next();
  };
}
```
