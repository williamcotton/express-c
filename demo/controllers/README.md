# Controllers and Routers

A [Todos controller](https://github.com/williamcotton/express-c/tree/master/demo/controllers#todos-controller) following the MVC pattern and an [API router](https://github.com/williamcotton/express-c/tree/master/demo/controllers#api-router) that returns JSON.

## Todos Controller

```c
#include "../models/todo.h"
#include "../../src/express.h"
#include <cJSON/cJSON.h>
#include <middleware/cookie-session-middleware.h>
#include <middleware/mustache-middleware.h>
#include <stdlib.h>
```

The todosController is based around an instance of `expressRouter`

```c
router_t *todosController(embedded_files_data_t embeddedFiles) {
  router_t *router = expressRouter();
```

### Middleware

The application is built on top of the [cookie session middleware](https://github.com/williamcotton/express-c/blob/master/src/middleware/cookie-session-middleware.md), [mustache middleware](https://github.com/williamcotton/express-c/blob/master/src/middleware/mustache-middleware.md) and [todo model](https://github.com/williamcotton/express-c/tree/master/demo/models) middleware.

In production the mustache views are compiled into C binary data. In development we load the views from the file system for a streamlined experience.

```c
  router->use(cookieSessionMiddlewareFactory());
  router->use(todoStoreMiddleware());
  router->use(cJSONMustacheMiddleware("demo/views", embeddedFiles));
```

### Front Page

Since this is a single-page application the majority of the application logic is contained in the front page.

The `cJSON` library is used to build the JSON data that will be used to render the front page mustache template.

The `express-c` framework provides `req->query("filter")` where as the todo model middleware provides the `req->m("todoStore")` struct.

```c
  router->get("/", ^(request_t *req, response_t *res) {
    char *queryFilter = req->query("filter");
    char *filter = queryFilter ? queryFilter : "all";
    todo_store_t *todoStore = req->m("todoStore");

    __block int completedCount = 0;
    __block int total = 0;

    collection_t *todosCollection = todoStore->filter(^(void *item) {
      todo_t *todo = (todo_t *)item;
      total++;
      if (todo->completed)
        completedCount++;
      return strcmp(filter, "all") == 0 ||
             (strcmp(filter, "active") == 0 && !todo->completed) ||
             (strcmp(filter, "completed") == 0 && todo->completed);
    });

    int uncompletedCount = total - completedCount;

    cJSON *json = cJSON_CreateObject();
    cJSON *todos = cJSON_AddArrayToObject(json, "todos");

    cJSON_AddStringToObject(json, "filterAll",
                            strcmp(filter, "all") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterActive",
                            strcmp(filter, "active") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterCompleted",
                            strcmp(filter, "completed") == 0 ? "selected" : "");

    todosCollection->each(^(void *item) {
      todo_t *todo = item;
      cJSON_AddItemToArray(todos, todo->toJSON());
    });

    cJSON_AddNumberToObject(json, "uncompletedCount", uncompletedCount);

    uncompletedCount == 1
        ? cJSON_AddTrueToObject(json, "hasUncompletedSingular")
        : cJSON_AddFalseToObject(json, "hasUncompletedSingular");
    completedCount > 0 ? cJSON_AddTrueToObject(json, "hasCompleted")
                       : cJSON_AddFalseToObject(json, "hasCompleted");
    total == 0 ? cJSON_AddTrueToObject(json, "noTodos")
               : cJSON_AddFalseToObject(json, "noTodos");

    res->render("index", json);

    if (strcmp(filter, "all") != 0)
      free(filter);
  });
```

#### Add Todo

```c
  router->post("/todo", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    char *title = req->body("title");

    todo_t *newTodo = todoStore->new (title);
    todoStore->create(newTodo);
    res->redirect("back");

    free(title);
  });
```

#### Update Todo

```c
  router->put("/todo/:id", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    char *idString = req->params("id");

    int id = atoi(idString);
    todo_t *todo = todoStore->find(id);
    if (todo != NULL) {
      todo->completed = !todo->completed;
      todoStore->update(todo);
    }
    res->redirect("back");

    free(idString);
  });
```

#### Delete Todo

```c
  router->delete ("/todo/:id", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    char *idString = req->params("id");

    int id = atoi(idString);
    todoStore->delete (id);
    res->redirect("back");

    free(idString);
  });
```

#### Clear Completed

```c
  router->post("/todo_clear_all_completed", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");

    collection_t *todosCollection = todoStore->filter(^(void *item) {
      todo_t *todo = item;
      return todo->completed;
    });

    todosCollection->each(^(void *item) {
      todo_t *todo = item;
      todoStore->delete (todo->id);
    });

    res->redirect("back");
  });

  return router;
}
```

## API Router

```c
#include "../../src/express.h"
#include <Block.h>
#include <postgres-middleware/postgres-middleware.h>
```

### Postgres Database

sendf

```c
router_t *apiController(const char *pgUri, int poolSize) {
  router_t *router = expressRouter();
```

### Middleware

Our API is built on top of the [postgres middleware](https://github.com/williamcotton/express-c/blob/master/src/middleware/postgres-middleware.md) library.

We first get an instance of the postgres connection with a given URI and pool size.

```c
  postgres_connection_t *postgres = initPostgressConnection(pgUri, poolSize);

  router->use(postgresMiddlewareFactory(postgres));
```

### Routes

Out basic GET request wraps a SQL query to return all rows in the `todos` database in JSON format.

The postgres middleware provides a `req->m("pg")` struct that wraps [libpq](https://www.postgresql.org/docs/14/libpq.html) and provides an easy way to execute SQL queries using a connection pool in a thread-safe manner.

```c
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
```

### Cleanup

We need to clean up the postgres connection when the router is destroyed.

```c
  router->cleanup(Block_copy(^{
    freePostgresConnection(postgres);
  }));

  return router;
}
```
