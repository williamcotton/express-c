# express-c

Fast, unopinionated, minimalist web framework for C

## Features

- User-friendly API using blocks
- Uses libdispatch for concurrency

## Basic Usage

```c
#include <stdlib.h>
#include <stdio.h>
#include "../src/express.h"

char *styles = "<link rel='stylesheet' href='/demo/public/app.css'>";

char *createTodoForm = "<form method='POST' action='/todo/create'>"
                       "  <p><label>Title: <input type='text' name='title'></label></p>"
                       "  <p><label>Body: <input type='text' name='body'></label></p>"
                       "  </p><input type='submit' value='Create'></p>"
                       "</form>";

int main()
{
  app_t app = express();
  int port = 3000;

  app.use(expressStatic("demo/public"));

  app.get("/", ^(UNUSED request_t *req, response_t *res) {
    res->sendf("%s<h1>Todo App</h1><p>%s</p>", styles, createTodoForm);
  });

  app.post("/todo/create", ^(request_t *req, response_t *res) {
    res->status = 201;
    res->sendf("%s<h1>%s</h1><p>%s</p>", styles, req->body("title"), req->body("body"));
  });

  app.listen(port, ^{
    printf("Todo app listening at http://localhost:%d\n", port);
  });

  return 0;
}
```

## Installation

See the Dockerfile in .devcontainer for system requirements.

## Demo App

```
$ make app
$ build/app
Todo app listening at http://localhost:3000
$ open http://localhost:3000
^C
Closing server...
```

## Testing

```
make test
```
