#include "test-harness.h"
#include "../src/express.h"
#include <Block.h>
#include <stdlib.h>

test_harness_t *testHarnessFactory() {
  __block app_t *app = express();
  int port = 3032;

  char *staticFilesPath = cwdFullPath("test");
  embedded_files_data_t embeddedFiles = {0};
  app->use(expressStatic("test", staticFilesPath, embeddedFiles));

  mem_session_t *memSession = malloc(sizeof(mem_session_t));
  memSession->stores = malloc(sizeof(mem_store_t *) * 100);
  memSession->count = 0;
  dispatch_queue_t memSessionQueue =
      dispatch_queue_create("memSessionQueue", NULL);
  app->use(memSessionMiddlewareFactory(memSession, memSessionQueue));

  typedef struct super_t {
    char *uuid;
  } super_t;

  app->use(^(request_t *req, UNUSED response_t *res, void (^next)(),
             void (^cleanup)(cleanupHandler)) {
    super_t *super = malloc(sizeof(super_t));
    super->uuid = "super test";
    req->mSet("super", super);
    cleanup(Block_copy(^(request_t *finishedReq) {
      super_t *s = finishedReq->m("super");
      free(s);
    }));
    next();
  });

  app->get("/", ^(UNUSED request_t *req, response_t *res) {
    res->send("Hello World!");
  });

  app->get("/test", ^(UNUSED request_t *req, response_t *res) {
    res->status = 201;
    res->send("Testing, testing!");
  });

  app->get("/status", ^(UNUSED request_t *req, response_t *res) {
    res->sendStatus(418);
  });

  app->get("/qs", ^(request_t *req, response_t *res) {
    char *value1 = req->query("value1");
    char *value2 = req->query("value2");
    res->sendf("<h1>Query String</h1><p>Value 1: %s</p><p>Value 2: %s</p>",
               value1, value2);
  });

  app->get("/headers", ^(request_t *req, response_t *res) {
    char *host = req->get("Host");
    char *accept = req->get("Accept");
    const char **subdomains = req->subdomains;
    const char **ips = req->ips;
    res->sendf(
        "<h1>Headers</h1><p>Host: %s</p><p>Accept: %s</p><p>%d Subdomains: "
        "%s %s %s</p><p>%d IPs: %s %s %s</p>",
        host, accept, req->subdomainsCount, subdomains[0], subdomains[1],
        subdomains[2], req->ipsCount, ips[0], ips[1], ips[2]);
  });

  app->get("/file", ^(UNUSED request_t *req, response_t *res) {
    res->sendFile("./test/test.txt");
  });

  app->get("/download", ^(UNUSED request_t *req, response_t *res) {
    res->download("./test/test.txt", NULL);
  });

  app->get("/download_missing", ^(UNUSED request_t *req, response_t *res) {
    res->download("test_missing.txt", NULL);
  });

  app->get("/one/:one/two/:two/:three.jpg", ^(request_t *req, response_t *res) {
    char *one = req->params("one");
    char *two = req->params("two");
    char *three = req->params("three");
    res->sendf("<h1>Params</h1><p>One: %s</p><p>Two: %s</p><p>Three: %s</p>",
               one, two, three);
  });

  app->get("/form", ^(UNUSED request_t *req, response_t *res) {
    res->send("<form method=\"POST\" action=\"/post/new\">"
              "  <input type=\"text\" name=\"param1\">"
              "  <input type=\"text\" name=\"param2\">"
              "  <input type=\"submit\" value=\"Submit\">"
              "</form>");
  });

  app->post("/post/:form", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    char *param2 = req->body("param2");
    res->status = 201;
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", param1,
               param2);
  });

  app->post("/session", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    req->session->set("param1", strdup(param1));
    res->send("ok");
  });

  app->get("/session", ^(request_t *req, response_t *res) {
    char *param1 = req->session->get("param1");
    res->send(param1);
    free(param1);
  });

  app->put("/put/:form", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    char *param2 = req->body("param2");
    res->status = 201;
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", param1,
               param2);
  });

  app->patch("/patch/:form", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    char *param2 = req->body("param2");
    res->status = 201;
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", param1,
               param2);
  });

  app->delete ("/delete/:id", ^(request_t *req, response_t *res) {
    char *id = req->params("id");
    res->sendf("<h1>Delete</h1><p>ID: %s</p>", id);
  });

  app->get("/m", ^(request_t *req, response_t *res) {
    super_t *super = req->m("super");
    res->send(super->uuid);
  });

  app->get("/set_header", ^(UNUSED request_t *req, response_t *res) {
    res->set("X-Test-1", "test1");
    res->set("X-Test-2", "test2");
    char *xTest1 = res->get("X-Test-1");
    res->send(xTest1);
  });

  app->get("/set_cookie", ^(request_t *req, response_t *res) {
    char *session = req->query("session");
    char *user = req->query("user");
    res->cookie("session", session, (cookie_opts_t){});
    res->cookie("user", user, (cookie_opts_t){});
    res->cookie("all", session,
                (cookie_opts_t){.secure = 1,
                                .httpOnly = 1,
                                .domain = "test.com",
                                .path = "/test",
                                .expires = "55",
                                .maxAge = 1000});
    res->send("ok");
  });

  app->get("/get_cookie", ^(request_t *req, response_t *res) {
    const char *session = req->cookie("session");
    const char *user = req->cookie("user");
    res->sendf("session: %s - user: %s", session, user);
  });

  app->get("/redirect", ^(UNUSED request_t *req, response_t *res) {
    res->redirect("/redirected");
  });

  app->get("/redirect/back", ^(UNUSED request_t *req, response_t *res) {
    res->redirect("back");
  });

  router_t *router = expressRouter();

  router->use(^(request_t *req, UNUSED response_t *res, void (^next)(),
                void (^cleanup)(cleanupHandler)) {
    super_t *super = malloc(sizeof(super_t));
    super->uuid = "super-router test";
    req->mSet("super-router", super);
    cleanup(Block_copy(^(request_t *finishedReq) {
      super_t *s = finishedReq->m("super-router");
      free(s);
    }));
    next();
  });

  router->get("/", ^(UNUSED request_t *req, response_t *res) {
    res->send("Hello Router!");
  });

  router->get("/test", ^(UNUSED request_t *req, response_t *res) {
    res->send("Testing Router!");
  });

  router->get(
      "/one/:one/two/:two/:three.jpg", ^(request_t *req, response_t *res) {
        char *one = req->params("one");
        char *two = req->params("two");
        char *three = req->params("three");
        res->sendf(
            "<h1>Base Params</h1><p>One: %s</p><p>Two: %s</p><p>Three: %s</p>",
            one, two, three);
      });

  router->get("/m", ^(request_t *req, response_t *res) {
    super_t *super = req->m("super-router");
    res->send(super->uuid);
  });

  router_t *nestedRouter = expressRouter();

  nestedRouter->use(^(request_t *req, UNUSED response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    super_t *super = malloc(sizeof(super_t));
    super->uuid = "super-nested-router test";
    req->mSet("super-nested-router", super);
    cleanup(Block_copy(^(request_t *finishedReq) {
      super_t *s = finishedReq->m("super-nested-router");
      free(s);
    }));
    next();
  });

  nestedRouter->get("/", ^(UNUSED request_t *req, response_t *res) {
    res->send("Hello Nested Router!");
  });

  nestedRouter->get("/test", ^(UNUSED request_t *req, response_t *res) {
    res->send("Testing Nested Router!");
  });

  nestedRouter->get("/one/:one/two/:two/:three.jpg", ^(request_t *req,
                                                       response_t *res) {
    char *one = req->params("one");
    char *two = req->params("two");
    char *three = req->params("three");
    res->sendf(
        "<h1>Nested Params</h1><p>One: %s</p><p>Two: %s</p><p>Three: %s</p>",
        one, two, three);
  });

  nestedRouter->get("/m", ^(request_t *req, response_t *res) {
    super_t *super = req->m("super-nested-router");
    res->send(super->uuid);
  });

  router_t *paramsRouter = expressRouter();

  paramsRouter->use(^(request_t *req, UNUSED response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    super_t *super = malloc(sizeof(super_t));
    super->uuid = "super-params-router test";
    req->mSet("super-params-router", super);
    cleanup(Block_copy(^(request_t *finishedReq) {
      super_t *s = finishedReq->m("super-params-router");
      free(s);
    }));
    next();
  });

  paramsRouter->get("/", ^(UNUSED request_t *req, response_t *res) {
    char *id = req->params("id");
    res->sendf("Hello Params %s Router!", id);
  });

  paramsRouter->get("/test", ^(UNUSED request_t *req, response_t *res) {
    char *id = req->params("id");
    res->sendf("Testing Nested %s Router!", id);
  });

  paramsRouter->get("/one/:one/two/:two/:three.jpg", ^(request_t *req,
                                                       response_t *res) {
    char *id = req->params("id");
    char *one = req->params("one");
    char *two = req->params("two");
    char *three = req->params("three");
    res->sendf(
        "<h1>Nested Params %s</h1><p>One: %s</p><p>Two: %s</p><p>Three: %s</p>",
        id, one, two, three);
  });

  paramsRouter->get("/m", ^(request_t *req, response_t *res) {
    char *id = req->params("id");
    super_t *super = req->m("super-params-router");
    res->sendf("%s %s", super->uuid, id);
  });

  router_t *rootRouter = expressRouter();

  rootRouter->get("/", ^(UNUSED request_t *req, response_t *res) {
    res->send("Hello Root Router!");
  });

  rootRouter->get("/root", ^(UNUSED request_t *req, response_t *res) {
    res->send("Hello Root Router!");
  });

  app->useRouter("/", rootRouter);
  router->useRouter("/params/:id", paramsRouter);
  app->useRouter("/base", router);
  router->useRouter("/nested", nestedRouter);

  app->cleanup(Block_copy(^{
    free(staticFilesPath);
    dispatch_release(memSessionQueue);
    for (int i = 0; i < memSession->count; i++) {
      free(memSession->stores[i]->sessionStore);
      free(memSession->stores[i]->uuid);
      free(memSession->stores[i]);
    }
    free(memSession->stores);
    free(memSession);
  }));

  test_harness_t *testHarness = malloc(sizeof(test_harness_t));

  testHarness->teardown = Block_copy(^{
    shutdownAndFreeApp(app);
  });

  testHarness->setup = Block_copy(^(void (^callback)()) {
    app->listen(port, ^{
      callback();
    });
  });

  return testHarness;
}