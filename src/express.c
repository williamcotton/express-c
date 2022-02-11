/*
  Copyright (c) 2022 William Cotton

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "express.h"

char *errorHTML = "<!DOCTYPE html>\n"
                  "<html lang=\"en\">\n"
                  "<head>\n"
                  "<meta charset=\"utf-8\">\n"
                  "<title>Error</title>\n"
                  "</head>\n"
                  "<body>\n"
                  "<pre>%s</pre>\n"
                  "</body>\n"
                  "</html>\n";

int initClientAcceptEventHandler(server_t *server, router_t *router);

app_t *express() {
  app_t *app = malloc(sizeof(app_t));

  server_t *server = expressServer();
  router_t *router = expressRouter();

  router->basePath = "";
  router->mountPath = "";
  router->isBaseRouter = 1;

  app->get = router->get;
  app->post = router->post;
  app->put = router->put;
  app->patch = router->patch;
  app->delete = router->delete;
  app->all = router->all;
  app->use = router->use;
  app->error = router->error;
  app->useRouter = router->useRouter;
  app->param = router->param;
  app->cleanup = router->cleanup;
  app->server = server;

  app->error(^(error_t *err, UNUSED request_t *req, response_t *res,
               UNUSED void (^next)()) {
    res->status = err->status;
    res->sendf(errorHTML, err->message);
  });

  app->closeServer = Block_copy(^() {
    printf("\nClosing server...\n");
    server->close();
  });

  app->listen = Block_copy(^(int port, void (^callback)()) {
    check(server->initSocket() >= 0, "Failed to initialize server socket");
    check(server->listen(port) >= 0, "Failed to listen on port %d", port);
    dispatch_async(server->serverQueue, ^{
      check(initClientAcceptEventHandler(server, router),
            "Failed to initialize client accept event handler");
    error:
      return;
    });
    callback();
    if (port > 0)
      dispatch_main();
  error:
    router->free();
    free(router);

    return;
  });

  app->free = Block_copy(^() {
    server->free();
    free(server);
    router->free();
    free(router);
    Block_release(app->closeServer);
    Block_release(app->listen);
    Block_release(app->free);
  });

  return app;
};

void shutdownAndFreeApp(app_t *app) {
  app->closeServer();
  usleep(100);
  app->free();
  free(app);
}
