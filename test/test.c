#include "../src/express.h"
#include "tape.h"
#include "test-helpers.h"
#include <Block.h>
#include <curl/curl.h>
#include <dotenv-c/dotenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

#define true 1
#define false 0

static char *errorHTML = "<!DOCTYPE html>\n"
                         "<html lang=\"en\">\n"
                         "<head>\n"
                         "<meta charset=\"utf-8\">\n"
                         "<title>Error</title>\n"
                         "</head>\n"
                         "<body>\n"
                         "<pre>Cannot GET %s</pre>\n"
                         "</body>\n"
                         "</html>\n";

void runTests(int runAndExit, app_t app) {
  tape_t *t = tape();

  int testStatus = t->test("express", ^(tape_t *t) {
    clearState();

    t->test("send garbage", ^(tape_t *t) {
      sendData("garbage\r\n\r\n");
      sendData("GET / HTTP1.1\r\n\r\n");
      sendData("GETSDFDFDF / HTTP/1.1\r\n\r\n");
      sendData("GET HTTP/1.1\r\n\r\n");
      sendData("GET --- HTTP/1.1\r\n\r\n");
      sendData("\r\n\r\n");
      sendData("\r\n");
      sendData("");
      sendData("POST / HTTP/1.1\r\nContent-Type: "
               "application/json\r\nContent-Length: 50\r\n\r\ngarbage");
      sendData("POST / HTTP/1.1\r\nContent-Type: "
               "application/json\r\nContent-Length: "
               "9999999999999999999999999999\r\n\r\ngarbage");
      sendData(
          "POST / HTTP/1.1\r\nContent-Type: application/json\r\n\r\ngarbage");
      sendData("POST / HTTP/1.1\r\n\r\ngarbage");
      sendData("GET /qs?g=&a-?-&r&=b=a&ge HTTP1.1\r\n\r\n");

      t->test("send lots of garbage", ^(tape_t *t) {
        int multipliers[2] = {1, 64};

        for (int i = 0; i < 2; i++) {
          size_t longStringLen = 1024 * multipliers[i];
          log_info("Testing with %zu bytes", longStringLen);

          char *longString = malloc(longStringLen);

          randomString(longString, longStringLen);
          sendData(longString);

          memcpy(longString, "GET / HTTP/1.1\r\n", 16);
          sendData(longString);

          memcpy(longString, "GET / HTTP/1.1\r\nContent-Type: ", 30);
          sendData(longString);

          memcpy(longString,
                 "GET / HTTP/1.1\r\nContent-Type: application/json\r\n", 48);
          sendData(longString);

          memcpy(longString,
                 "POST / HTTP/1.1\r\nContent-Type: "
                 "application/json\r\nContent-Length: 50\r\n\r\n",
                 71);
          sendData(longString);

          memcpy(longString,
                 "POST / HTTP/1.1\r\nContent-Type: "
                 "application/json\r\nContent-Length: 16384\r\n\r\n",
                 74);
          sendData(longString);

          randomString(longString, longStringLen);
          memcpy(longString, "GET / HTTP/1.1\r\nContent-Type: ", 30);
          memcpy(longString + (longStringLen - 30), "\r\n\r\n", 4);

          randomString(longString, longStringLen);
          memcpy(longString, "GET / HTTP/1.1\r\nCookie: ", 24);
          memcpy(longString + (longStringLen - 24), "\r\n\r\n", 4);

          randomString(longString, longStringLen);
          memcpy(longString, "GET /qs?", 8);
          memcpy(longString + (longStringLen - 26), " HTTP/1.1\r\n\r\n", 13);

          free(longString);
        }
      });
    });

    t->test("GET", ^(tape_t *t) {
      t->strEqual("root", curlGet("/"), "Hello World!");
      t->strEqual("basic route", curlGet("/test"), "Testing, testing!");
      t->strEqual(
          "route params", curlGet("/one/123/two/345/567.jpg"),
          "<h1>Params</h1><p>One: 123</p><p>Two: 345</p><p>Three: 567</p>");
      t->strEqual("send status", curlGet("/status"), "I'm a teapot");
      t->strEqual(
          "query string", curlGet("/qs\?value1=123\\&value2=34%205"),
          "<h1>Query String</h1><p>Value 1: 123</p><p>Value 2: 34 5</p>");
      t->strEqual("send file", curlGet("/file"), "hello, world!\n");
    });

    t->test("POST", ^(tape_t *t) {
      t->strEqual("form data",
                  curlPost("/post/form123", "param1=12%2B3&param2=3+4%205"),
                  "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    });

    t->test("PUT", ^(tape_t *t) {
      t->strEqual("put form data",
                  curlPut("/put/form123", "param1=12%2B3&param2=3+4%205"),
                  "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    });

    t->test("PATCH", ^(tape_t *t) {
      t->strEqual("patch form data",
                  curlPatch("/patch/form123", "param1=12%2B3&param2=3+4%205"),
                  "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    });

    t->test("DELETE", ^(tape_t *t) {
      t->strEqual("delete form data", curlDelete("/delete/form123"),
                  "<h1>Delete</h1><p>ID: form123</p>");
    });

    t->test("Middleware", ^(tape_t *t) {
      t->strEqual("static file middleware", curlGet("/test/test2.txt"),
                  "this is a test!!!");
      t->strEqual("custom request middleware", curlGet("/m"), "super test");
    });

    t->test("File", ^(tape_t *t) {
      t->strEqual("file", curlGet("/file"), "hello, world!\n");
      char error[1024];
      sprintf(error, errorHTML, "/test/test3.txt");
      t->strEqual("file not found", curlGet("/test/test3.txt"), error);
    });

    t->test("Session", ^(tape_t *t) {
      t->strEqual("session set", curlPost("/session", "param1=session-data"),
                  "ok");
      t->strEqual("session get", curlGet("/session"), "session-data");
    });

    t->test("Header", ^(tape_t *t) {
      t->strEqual(
          "headers", curlGet("/headers"),
          "<h1>Headers</h1><p>Host: 127.0.0.1:3032</p><p>Accept: */*</p>");
      t->strEqual("set header", curlGet("/set_header"), "test1");
    });

    t->test("Cookies", ^(tape_t *t) {
      t->strEqual("set cookie", curlGet("/set_cookie\?session=123\\&user=test"),
                  "ok");
      t->strEqual("get cookie", curlGet("/get_cookie"),
                  "session: 123 - user: test");
    });

    t->test("Redirect", ^(tape_t *t) {
      t->strEqual("redirect", curlGet("/redirect"),
                  "Redirecting to /redirected");
      t->strEqual("redirect back", curlGet("/redirect/back"),
                  "Redirecting to back");
    });

    t->test("Error", ^(tape_t *t) {
      char error[1024];
      sprintf(error, errorHTML, "/error");
      t->strEqual("error", curlGet("/error"), error);
    });

    t->test("Router", ^(tape_t *t) {
      t->strEqual("root", curlGet("/base"), "Hello Router!");
      t->strEqual("basic route", curlGet("/base/test"), "Testing Router!");
      t->strEqual("route params", curlGet("/base/one/123/two/345/567.jpg"),
                  "<h1>Base Params</h1><p>One: 123</p><p>Two: 345</p><p>Three: "
                  "567</p>");
      t->strEqual("custom request middleware", curlGet("/base/m"),
                  "super-router test");

      t->test("Nested router", ^(tape_t *t) {
        t->strEqual("root", curlGet("/base/nested"), "Hello Nested Router!");
        t->strEqual("basic route", curlGet("/base/nested/test"),
                    "Testing Nested Router!");
        t->strEqual("route params",
                    curlGet("/base/nested/one/123/two/345/567.jpg"),
                    "<h1>Nested Params</h1><p>One: 123</p><p>Two: "
                    "345</p><p>Three: 567</p>");
        t->strEqual("custom request middleware", curlGet("/base/nested/m"),
                    "super-nested-router test");
      });

      t->test("Params router", ^(tape_t *t) {
        t->strEqual("basic route", curlGet("/base/params/blip/test"),
                    "Testing Nested blip Router!");
        t->strEqual("route params",
                    curlGet("/base/params/blip/one/123/two/345/567.jpg"),
                    "<h1>Nested Params blip</h1><p>One: 123</p><p>Two: "
                    "345</p><p>Three: 567</p>");
        t->strEqual("root", curlGet("/base/params/blip"),
                    "Hello Params blip Router!");
        t->strEqual("custom request middleware", curlGet("/base/params/blip/m"),
                    "super-nested-router test blip");
      });
    });
  });

  Block_release(t->test);
  free(t);

  if (runAndExit) {
    app.closeServer();
    exit(testStatus);
  }
}

#pragma clang diagnostic pop

int main() {
  env_load(".", false);

  int runXTimes = getenv("RUN_X_TIMES") ? atoi(getenv("RUN_X_TIMES")) : 1;
  int sleepTime = getenv("SLEEP_TIME") ? atoi(getenv("SLEEP_TIME")) : 0;

  sleep(sleepTime);

  app_t app = express();
  int port = 3032;

  char *staticFilesPath = cwdFullPath("test");
  embedded_files_data_t embeddedFiles = {0};
  app.use(expressStatic("test", staticFilesPath, embeddedFiles));

  mem_session_t *memSession = malloc(sizeof(mem_session_t));
  memSession->stores = malloc(sizeof(mem_store_t *) * 100);
  memSession->count = 0;
  dispatch_queue_t memSessionQueue =
      dispatch_queue_create("memSessionQueue", NULL);
  app.use(memSessionMiddlewareFactory(memSession, memSessionQueue));

  typedef struct super_t {
    char *uuid;
  } super_t;

  app.use(^(request_t *req, UNUSED response_t *res, void (^next)(),
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

  app.get("/", ^(UNUSED request_t *req, response_t *res) {
    res->send("Hello World!");
  });

  app.get("/test", ^(UNUSED request_t *req, response_t *res) {
    res->status = 201;
    res->send("Testing, testing!");
  });

  app.get("/status", ^(UNUSED request_t *req, response_t *res) {
    res->sendStatus(418);
  });

  app.get("/qs", ^(request_t *req, response_t *res) {
    char *value1 = req->query("value1");
    char *value2 = req->query("value2");
    res->sendf("<h1>Query String</h1><p>Value 1: %s</p><p>Value 2: %s</p>",
               value1, value2);
    curl_free(value1);
    curl_free(value2);
  });

  app.get("/headers", ^(request_t *req, response_t *res) {
    char *host = req->get("Host");
    char *accept = req->get("Accept");
    res->sendf("<h1>Headers</h1><p>Host: %s</p><p>Accept: %s</p>", host,
               accept);
    free(host);
    free(accept);
  });

  app.get("/file", ^(UNUSED request_t *req, response_t *res) {
    res->sendFile("./test/test.txt");
  });

  app.get("/one/:one/two/:two/:three.jpg", ^(request_t *req, response_t *res) {
    char *one = req->params("one");
    char *two = req->params("two");
    char *three = req->params("three");
    res->sendf("<h1>Params</h1><p>One: %s</p><p>Two: %s</p><p>Three: %s</p>",
               one, two, three);
    free(one);
    free(two);
    free(three);
  });

  app.get("/form", ^(UNUSED request_t *req, response_t *res) {
    res->send("<form method=\"POST\" action=\"/post/new\">"
              "  <input type=\"text\" name=\"param1\">"
              "  <input type=\"text\" name=\"param2\">"
              "  <input type=\"submit\" value=\"Submit\">"
              "</form>");
  });

  app.post("/post/:form", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    char *param2 = req->body("param2");
    res->status = 201;
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", param1,
               param2);
    curl_free(param1);
    curl_free(param2);
  });

  app.post("/session", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    req->session->set("param1", param1);
    res->send("ok");
  });

  app.get("/session", ^(request_t *req, response_t *res) {
    char *param1 = req->session->get("param1");
    res->send(param1);
    curl_free(param1);
  });

  app.put("/put/:form", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    char *param2 = req->body("param2");
    res->status = 201;
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", param1,
               param2);
    curl_free(param1);
    curl_free(param2);
  });

  app.patch("/patch/:form", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    char *param2 = req->body("param2");
    res->status = 201;
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", param1,
               param2);
    curl_free(param1);
    curl_free(param2);
  });

  app.delete("/delete/:id", ^(request_t *req, response_t *res) {
    char *id = req->params("id");
    res->sendf("<h1>Delete</h1><p>ID: %s</p>", id);
    free(id);
  });

  app.get("/m", ^(request_t *req, response_t *res) {
    super_t *super = req->m("super");
    res->send(super->uuid);
  });

  app.get("/set_header", ^(UNUSED request_t *req, response_t *res) {
    res->set("X-Test-1", "test1");
    res->set("X-Test-2", "test2");
    char *xTest1 = res->get("X-Test-1");
    res->send(xTest1);
  });

  app.get("/set_cookie", ^(request_t *req, response_t *res) {
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
    curl_free(session);
    curl_free(user);
  });

  app.get("/get_cookie", ^(request_t *req, response_t *res) {
    const char *session = req->cookie("session");
    const char *user = req->cookie("user");
    res->sendf("session: %s - user: %s", session, user);
  });

  app.get("/redirect", ^(UNUSED request_t *req, response_t *res) {
    res->redirect("/redirected");
  });

  app.get("/redirect/back", ^(UNUSED request_t *req, response_t *res) {
    res->redirect("back");
  });

  router_t *router = expressRouter(0);

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
        free(one);
        free(two);
        free(three);
      });

  router->get("/m", ^(request_t *req, response_t *res) {
    super_t *super = req->m("super-router");
    res->send(super->uuid);
  });

  router_t *nestedRouter = expressRouter(0);

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
    free(one);
    free(two);
    free(three);
  });

  nestedRouter->get("/m", ^(request_t *req, response_t *res) {
    super_t *super = req->m("super-nested-router");
    res->send(super->uuid);
  });

  router_t *paramsRouter = expressRouter(0);

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
    free(id);
  });

  paramsRouter->get("/test", ^(UNUSED request_t *req, response_t *res) {
    char *id = req->params("id");
    res->sendf("Testing Nested %s Router!", id);
    free(id);
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
    free(id);
    free(one);
    free(two);
    free(three);
  });

  paramsRouter->get("/m", ^(request_t *req, response_t *res) {
    char *id = req->params("id");
    super_t *super = req->m("super-nested-router");
    res->sendf("%s %s", super->uuid, id);
    free(id);
  });

  // TODO: Order matters, fix this!
  app.useRouter("/base", router);

  router->useRouter("/nested", nestedRouter);
  router->useRouter("/params/:id", paramsRouter);

  app.cleanup(^{
    free(staticFilesPath);
    dispatch_release(memSessionQueue);
    for (int i = 0; i < memSession->count; i++) {
      free(memSession->stores[i]->sessionStore);
      free(memSession->stores[i]->uuid);
      free(memSession->stores[i]);
    }
    free(memSession->stores);
    free(memSession);
  });

  app.listen(port, ^{
    for (int i = 0; i < runXTimes; i++) {
      runTests(runXTimes == 1, app);
    }
    if (runXTimes > 1)
      exit(0);
  });

  return 0;
}
