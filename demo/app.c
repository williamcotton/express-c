#include <stdlib.h>
#include <stdio.h>
#include <hash/hash.h>
#include <Block.h>
#include <string.h>
#include <dotenv-c/dotenv.h>
#include <cJSON/cJSON.h>
#include <cJSONMustacheMiddleware/cJSONMustacheMiddleware.h>
#include "../src/express.h"

#ifndef PORT
#define PORT 3000
#endif // !PORT

typedef struct todo_t
{
  int id;
  char *title;
  int completed;
  cJSON * (^toJSON)();
} todo_t;

typedef struct todo_store_t
{
  hash_t *store;
  int count;
  todo_t * (^new)(char *);
  todo_t * (^create)(todo_t *todo);
  todo_t * (^find)(int id);
  void (^update)(int id, todo_t *todo);
  void (^delete)(int id);
} todo_store_t;

middlewareHandler todoStoreMiddleware()
{
  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)()) {
    todo_store_t *todoStore = req->session->get("todoStore");

    if (todoStore == NULL)
    {
      todoStore = malloc(sizeof(todo_store_t));
      todoStore->store = hash_new();
      todoStore->count = 0;

      todoStore->new = Block_copy(^(char *title) {
        __block todo_t *newTodo = malloc(sizeof(todo_t));
        newTodo->title = title;
        newTodo->completed = 0;
        newTodo->toJSON = Block_copy(^() {
          cJSON *json = cJSON_CreateObject();
          cJSON_AddNumberToObject(json, "id", newTodo->id);
          cJSON_AddStringToObject(json, "title", newTodo->title);
          newTodo->completed ? cJSON_AddTrueToObject(json, "completed") : cJSON_AddFalseToObject(json, "completed");
          return json;
        });
        return newTodo;
      });

      todoStore->create = Block_copy(^(todo_t *todo) {
        todo->id = todoStore->count++;
        char *key = malloc(sizeof(int));
        sprintf(key, "%d", todo->id);
        hash_set(todoStore->store, key, todo);
        return todo;
      });

      todoStore->update = Block_copy(^(int id, todo_t *todo) {
        char key[10];
        sprintf(key, "%d", id);
        hash_set(todoStore->store, key, todo);
      });

      todoStore->delete = Block_copy(^(int id) {
        char key[10];
        sprintf(key, "%d", id);
        hash_del(todoStore->store, key);
      });

      todoStore->find = Block_copy(^(int id) {
        char key[10];
        sprintf(key, "%d", id);
        return (todo_t *)hash_get(todoStore->store, key);
      });

      req->session->set("todoStore", todoStore);
    }

    req->mSet("todoStore", todoStore);

    next();
  });
}

int main()
{
  env_load(".", false);

  app_t app = express();
  int port = getenv("PORT") ? atoi(getenv("PORT")) : PORT;

  app.use(expressStatic("demo/public"));
  app.use(memSessionMiddlewareFactory());
  app.use(todoStoreMiddleware());
  app.use(cJSONMustacheMiddleware("demo/views"));

  app.get("/", ^(request_t *req, response_t *res) {
    cJSON *json = cJSON_CreateObject();

    char *filter = req->query("filter") ? req->query("filter") : "all";
    cJSON_AddStringToObject(json, "filterAll", strcmp(filter, "all") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterActive", strcmp(filter, "active") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterCompleted", strcmp(filter, "completed") == 0 ? "selected" : "");

    int completedCount = 0;
    int total = 0;

    cJSON *todos = cJSON_AddArrayToObject(json, "todos");

    todo_store_t *todoStore = req->m("todoStore");
    hash_each(todoStore->store, {
      todo_t *todo = (todo_t *)val;
      total++;
      if (todo->completed)
        completedCount++;
      if (strcmp(filter, "all") == 0 ||
          (strcmp(filter, "active") == 0 && !todo->completed) ||
          (strcmp(filter, "completed") == 0 && todo->completed))
      {
        cJSON_AddItemToArray(todos, todo->toJSON());
      }
    });

    int uncompletedCount = total - completedCount;

    cJSON_AddNumberToObject(json, "uncompletedCount", uncompletedCount);
    cJSON_AddNumberToObject(json, "completedCount", completedCount);
    cJSON_AddNumberToObject(json, "totalCount", total);

    total == completedCount ? cJSON_AddTrueToObject(json, "allCompleted") : cJSON_AddFalseToObject(json, "allCompleted");
    completedCount > 0 ? cJSON_AddTrueToObject(json, "hasCompleted") : cJSON_AddFalseToObject(json, "hasCompleted");
    total == 0 ? cJSON_AddTrueToObject(json, "noTodos") : cJSON_AddFalseToObject(json, "noTodos");

    res->render("index", json);
  });

  app.post("/todo", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    todo_t *newTodo = todoStore->new (req->body("title"));
    todoStore->create(newTodo);
    res->redirect("back");
  });

  app.put("/todo/:id", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    int id = atoi(req->params("id"));
    todo_t *todo = todoStore->find(id);
    if (todo != NULL)
    {
      todo->completed = !todo->completed;
      todoStore->update(id, todo);
    }
    res->redirect("back");
  });

  app.delete("/todo/:id", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    int id = atoi(req->params("id"));
    todoStore->delete (id);
    res->redirect("back");
  });

  app.post("/todo_clear_all_completed", ^(UNUSED request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    hash_each(todoStore->store, {
      todo_t *todo = (todo_t *)val;
      if (todo->completed)
      {
        todoStore->delete (todo->id);
      }
    });
    res->redirect("back");
  });

  app.listen(port, ^{
    printf("Todo app listening at http://localhost:%d\n", port);
  });

  return 0;
}
