#include <stdlib.h>
#include <stdio.h>
#include <hash/hash.h>
#include <Block.h>
#include <string.h>
#include <dotenv-c/dotenv.h>
#include <cJSON/cJSON.h>
#include <cJSONMustacheMiddleware/cJSONMustacheMiddleware.h>
#include <cJSONCookieSessionMiddleware/cJSONCookieSessionMiddleware.h>
#include "../src/express.h"
#include "models/todo.h"

int main()
{
  env_load(".", false);

  app_t app = express();
  int port = getenv("PORT") ? atoi(getenv("PORT")) : 3000;

  app.use(expressStatic("demo/public"));
  app.use(cJSONCookieSessionMiddlewareFactory());
  app.use(todoStoreMiddleware());
  app.use(cJSONMustacheMiddleware("demo/views"));

  app.get("/", ^(request_t *req, response_t *res) {
    char *filter = req->query("filter") ? req->query("filter") : "all";

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

    cJSON_AddStringToObject(json, "filterAll", strcmp(filter, "all") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterActive", strcmp(filter, "active") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterCompleted", strcmp(filter, "completed") == 0 ? "selected" : "");

    todosCollection->each(^(void *item) {
      todo_t *todo = item;
      cJSON_AddItemToArray(todos, todo->toJSON());
    });

    cJSON_AddNumberToObject(json, "uncompletedCount", uncompletedCount);

    uncompletedCount == 1 ? cJSON_AddTrueToObject(json, "hasUncompletedSingular") : cJSON_AddFalseToObject(json, "hasUncompletedSingular");
    completedCount > 0 ? cJSON_AddTrueToObject(json, "hasCompleted") : cJSON_AddFalseToObject(json, "hasCompleted");
    total == 0 ? cJSON_AddTrueToObject(json, "noTodos") : cJSON_AddFalseToObject(json, "noTodos");

    res->render("index", json);

    Block_release(todosCollection->each);
    free(todosCollection);
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
      todoStore->update(todo);
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

  app.listen(port, ^{
    printf("TodoMVC app listening at http://localhost:%d\n", port);
  });

  return 0;
}
