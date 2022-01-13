#include <stdlib.h>
#include <stdio.h>
#include <hash/hash.h>
#include <Block.h>
#include "../src/express.h"

char *styles = "<link rel='stylesheet' href='/demo/public/app.css'>";

char *createTodoForm = "<form method='POST' action='/todo/create'>"
                       "  <p><label>Title: <input type='text' name='title'></label></p>"
                       "  <p><label>Body: <input type='text' name='body'></label></p>"
                       "  </p><input type='submit' value='Create'></p>"
                       "</form>";

char *todoHtml = "<li>%s: %s</li>";

typedef struct todo_store_t
{
  hash_t *store;
  void * (^get)(char *title);
  void (^set)(char *title, char *body);
} todo_store_t;

char *buildTodos(request_t *req)
{
  char todos[4096];
  memset(todos, 0, sizeof(todos));
  todo_store_t *todoStore = req->session->get("todoStore");
  hash_each(todoStore->store, {
    char *todoHtmlFormatted = malloc(strlen(todoHtml) + strlen(key) + strlen(val) + 1);
    sprintf(todoHtmlFormatted, todoHtml, key, (char *)val);
    strcat(todos, todoHtmlFormatted);
    free(todoHtmlFormatted);
  });
  return strdup(todos);
}

middlewareHandler todoStoreMiddleware()
{
  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)()) {
    todo_store_t *todoStore = req->session->get("todoStore");
    if (todoStore == NULL)
    {
      todoStore = malloc(sizeof(todo_store_t));
      todoStore->store = hash_new();
      todoStore->get = Block_copy(^(char *title) {
        return hash_get(todoStore->store, title);
      });
      todoStore->set = Block_copy(^(char *title, char *body) {
        hash_set(todoStore->store, title, body);
      });
      req->session->set("todoStore", todoStore);
    }
    req->mSet("todoStore", todoStore);
    next();
  });
}

int main()
{
  app_t app = express();
  int port = 3000;

  app.use(expressStatic("demo/public"));
  app.use(memSessionMiddlewareFactory());
  app.use(todoStoreMiddleware());

  app.get("/", ^(request_t *req, response_t *res) {
    char *todos = buildTodos(req);
    res->send("%s<h1>Todo App</h1><p>%s</p><p>%s</p>", styles, createTodoForm, todos);
  });

  app.post("/todo/create", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    todoStore->set(
        req->body("title"),
        req->body("body"));
    res->status = 201;
    res->send("%s<h1>%s</h1><p>%s</p>", styles, req->body("title"), req->body("body"));
  });

  app.listen(port, ^{
    printf("Todo app listening at http://localhost:%d\n", port);
  });

  return 0;
}
