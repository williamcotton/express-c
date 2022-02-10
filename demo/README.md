# TodoMVC

There was once a tradition of building a [TodoMVC app](https://todomvc.com) to showcase a given web framework. While this has fallen out of favor and the website is unfortunately no longer actively maintained, it is still a great way to give an example of how to build a web app using `express-c`.

The UI is powered by [mustache templates](http://mustache.github.io/mustache.5.html) with data stored as JSON in a browser cookie.

#### Example API

In addition there is an example API powered by a Postgres database. Migrations are handled by [dbmate](https://github.com/amacneil/dbmate) and can be found in the [`.db/migrations`](https://github.com/williamcotton/express-c/tree/master/db/migrations) directory.

```
$ dbmate up
```

#### Example Deployment

The app is deployed as a Docker container to Heroku: [https://express-c-demo.herokuapp.com](express-c-demo.herokuapp.com).

```yml
build:
  docker:
    web: Dockerfile.deploy
run:
  web: /app/build/TodoMVC
release:
  image: web
  command:
    - dbmate --migrations-dir /app/db/migrations up
```

It uses a multi-stage build process to ensure that the app only includes the code that is needed for the production environment. The built image comes in at under 100MB.

```Dockerfile
# Start with the express-c development image
FROM ghcr.io/williamcotton/express-c:release AS buildstage

ENV LD_LIBRARY_PATH /usr/local/lib

WORKDIR /app

COPY . .

# Build the production app
RUN make TodoMVC-prod

# Copy the shared objects used by the app
RUN ldd /app/build/TodoMVC | tr -s '[:blank:]' '\n' | grep '^/' | \
  xargs -I % sh -c 'mkdir -p $(dirname sdeps%); cp % sdeps%;'

# Download dbmate
RUN curl -fsSL -o /usr/local/bin/dbmate https://github.com/amacneil/dbmate/releases/latest/download/dbmate-linux-amd64
RUN chmod +x /usr/local/bin/dbmate

# Grab a fresh copy of our linux image
FROM ubuntu:hirsute AS deploystage

WORKDIR /app

# Copy the shared objects to our new image
COPY --from=buildstage /app/sdeps/lib /usr/lib
COPY --from=buildstage /app/sdeps/usr/local/lib /usr/lib

# Copy the app executable
COPY --from=buildstage /app/build/TodoMVC /app/build/TodoMVC

# Copy our database migrations
COPY --from=buildstage /app/db/migrations /app/db/migrations
COPY --from=buildstage /usr/local/bin/dbmate /usr/local/bin/dbmate

# Run the app
CMD /app/build/TodoMVC

```

## Code Overview

The core of the application is found in `TodoMVC.c`, which is broken down below.

### Included Headers

Besides the usual `#include` statements we find a conditional include for `embeddedFiles.h`.

```c
#include "../src/express.h"
#include "controllers/api.h"
#include "controllers/todo.h"
#include <dotenv-c/dotenv.h>
#include <stdlib.h>

#ifdef EMBEDDED_FILES
#include "embeddedFiles.h"
#else
embedded_files_data_t embeddedFiles = {0};
#endif // EMBEDDED_FILES
```

#### Embedded Files

The `embeddedFiles.h` file is included in the production build of the app through the use of a compile-time definition of `EMBEDDED_FILES`.

Our build process uses the `xdd` utility to convert our static files and mustache templates views into C binary data.

### Environment Variables

Using the `dotenv-c` library, we can read the environment variables from the `.env` file in a manner that should be familiar to most web application developers.

```c
int main() {
  app_t *app = express();

  /* Load .env file */
  env_load(".", false);

  /* Environment variables */
  char *PORT = getenv("PORT");
  char *DATABASE_URL = getenv("DATABASE_URL");
  char *DATABASE_POOL_SIZE = getenv("DATABASE_POOL_SIZE");
  int port = PORT ? atoi(PORT) : 3000;
  int databasePoolSize = DATABASE_POOL_SIZE ? atoi(DATABASE_POOL_SIZE) : 5;
  const char *databaseUrl =
      DATABASE_URL ? DATABASE_URL : "postgresql://localhost/express-demo";

```

### Terminate on SIGINT

A common development pattern is to terminate the application on a SIGINT (Ctrl-C) signal. We use `libdispatch` to handle this as opposed to the standard `signal` library.

```c
  /* Close app on Ctrl+C */
  signal(SIGINT, SIG_IGN);
  dispatch_source_t sig_src = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, dispatch_get_main_queue());
  dispatch_source_set_event_handler(sig_src, ^{
    app->closeServer();
    exit(0);
  });
  dispatch_resume(sig_src);
```

### Middleware

Here we define the middleware that will be used by the application. The only application-wide middleware is for loading static files. In development we load these files directly from the file system, but in production we load them from the embedded files.

```c
  /* Load static files */
  char *staticFilesPath = cwdFullPath("demo/public");
  app->use(expressStatic("demo/public", staticFilesPath, embeddedFiles));
```

### Basic Routes

Not much to see here other than a standard health check.

```c
  /* Health check */
  app->get("/healthz", ^(UNUSED request_t *req, response_t *res) {
    res->send("OK");
  });
```

### Controllers and Routers

Here we wire up the routers for the [Todos controller](https://github.com/williamcotton/express-c/tree/master/demo/controllers#todos-controller) and [API router](https://github.com/williamcotton/express-c/tree/master/demo/controllers#api-router).

```c
  /* Controllers */
  app->useRouter("/", todosController(embeddedFiles));
  app->useRouter("/api/v1", apiController(databaseUrl, databasePoolSize));
```

### Cleanup

Here we free the dynamically allocated memory used by the application.

```c
  /* Clean up */
  app->cleanup(^{
    free(staticFilesPath);
  });
```

### Start Server

Finally, we start the server.

```c
  app->listen(port, ^{
    printf("TodoMVC app listening at http://localhost:%d\n", port);
    writePid("server.pid");
  });
}
```
