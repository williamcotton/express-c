#include <stdlib.h>
#include <stdio.h>
#include <hash/hash.h>
#include <Block.h>
#include <string.h>
#include "../src/express.h"

char *styles = "<link rel='stylesheet' href='/demo/public/app.css'>";

char *createTodoForm = "<form method='POST' action='/todo'>"
                       "  <p><input type='text' name='title' autofocus></p>"
                       "</form>";

char *completedFalse = "☐";
char *completedTrue = "☑";

char *todoHtml = "<li class='%s'>"
                 "  <form method='POST' action='/todo/%d'>"
                 "    <input type='hidden' name='_method' value='put' />"
                 "    <input type='submit' value='%s'>"
                 "  </form>"
                 "  <h4>%s</h4>"
                 "  <form method='POST' action='/todo/%d'>"
                 "    <input type='hidden' name='_method' value='delete' />"
                 "    <input type='submit' value='☒'>"
                 "  </form>"
                 "</li>";

typedef struct todo_t
{
  int id;
  char *title;
  int completed;
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

void printTodo(todo_t *todo)
{
  printf("id: %d title: %s complete: %d\n", todo->id, todo->title, todo->completed);
}

char *buildTodos(request_t *req)
{
  todo_store_t *todoStore = req->session->get("todoStore");

  char todos[4096];
  memset(todos, 0, sizeof(todos));

  hash_each(todoStore->store, {
    todo_t *todo = val;
    char *completed = todo->completed ? completedTrue : completedFalse;
    char *liClass = todo->completed ? "completed" : "";
    char *todoHtmlFormatted = malloc(strlen(todoHtml) + strlen(liClass) + strlen(todo->title) + 1);
    sprintf(todoHtmlFormatted, todoHtml, liClass, todo->id, completed, todo->title, todo->id);
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
      todoStore->count = 0;

      todoStore->new = Block_copy(^(char *title) {
        todo_t *newTodo = malloc(sizeof(todo_t));
        newTodo->title = title;
        newTodo->completed = 0;
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
  app_t app = express();
  int port = 3000;

  app.use(expressStatic("demo/public"));
  app.use(memSessionMiddlewareFactory());
  app.use(todoStoreMiddleware());

  app.get("/", ^(request_t *req, response_t *res) {
    char *todos = buildTodos(req);
    res->send("%s<section><h1>Todo App</h1><p>%s</p><p>%s</p></section>", styles, createTodoForm, todos);
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

  app.listen(port, ^{
    printf("Todo app listening at http://localhost:%d\n", port);
  });

  return 0;
}
