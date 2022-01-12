#include <stdlib.h>
#include <stdio.h>
#include <uuid/uuid.h>
#include <Block.h>
#include "../src/express.h"
#include "test-harnass.h"

static char *generateUuid()
{
  char *guid = malloc(37);
  if (guid == NULL)
  {
    return NULL;
  }
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, guid);
  return guid;
}

typedef struct super_t
{
  char *uuid;
  char * (^get)(char *key);
  void (^set)(char *key, char *value);
} super_t;

void runTests()
{
  clearState();
  testEq("root", curlGet("/"), "Hello World!");
  testEq("basic route", curlGet("/test"), "Testing, testing!");
  testEq("query string", curlGet("/qs\?value1=123\\&value2=34%205"), "<h1>Query String</h1><p>Value 1: 123</p><p>Value 2: 34 5</p>");
  testEq("headers", curlGet("/headers"), "<h1>Headers</h1><p>Host: 127.0.0.1:3032</p><p>Accept: */*</p>");
  testEq("route params", curlGet("/one/123/two/345/567.jpg"), "<h1>Params</h1><p>One: 123</p><p>Two: 345</p><p>Three: 567</p>");
  testEq("send file", curlGet("/file"), "hello, world!\n");
  testEq("static file middleware", curlGet("/test/test2.txt"), "this is a test!!!");
  testEq("form data", curlPost("/post/form123", "param1=12%2B3&param2=3+4%205"), "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
  testEq("session set", curlPost("/session", "param1=session-data"), "ok");
  testEq("session get", curlGet("/session"), "session-data");
  testEq("custom request middleware", curlGet("/m"), "super test");
  testEq("set header", curlGet("/set_header"), "ok");
  testEq("set cookie", curlGet("/set_cookie\?session=123\\&user=test"), "ok");
  testEq("get cookie", curlGet("/get_cookie"), "session: 123 - user: test");
  exit(EXIT_SUCCESS);
}

middlewareHandler sessionMiddlewareFactory()
{
  __block hash_t *sessionStore = hash_new();
  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)()) {
    // TODO: get session from cookie
    req->session->uuid = generateUuid();

    req->session->get = ^(char *key) {
      return (char *)hash_get(sessionStore, key);
    };

    req->session->set = ^(char *key, char *value) {
      hash_set(sessionStore, key, value);
    };

    next();
  });
}

int main()
{
  app_t app = express();
  int port = 3032;

  app.use(expressStatic("test"));

  app.use(^(request_t *req, UNUSED response_t *res, void (^next)()) {
    super_t *super = malloc(sizeof(super_t));
    super->uuid = "super test";
    req->mSet("super", super);
    next();
  });

  app.use(sessionMiddlewareFactory());

  app.get("/", ^(UNUSED request_t *req, response_t *res) {
    res->send("Hello World!");
  });

  app.get("/test", ^(UNUSED request_t *req, response_t *res) {
    res->status = 201;
    res->send("Testing, testing!");
  });

  app.get("/qs", ^(request_t *req, response_t *res) {
    res->sendf("<h1>Query String</h1><p>Value 1: %s</p><p>Value 2: %s</p>", req->query("value1"), req->query("value2"));
  });

  app.get("/headers", ^(request_t *req, response_t *res) {
    res->sendf("<h1>Headers</h1><p>Host: %s</p><p>Accept: %s</p>", req->get("Host"), req->get("Accept"));
  });

  app.get("/file", ^(UNUSED request_t *req, response_t *res) {
    res->sendFile("./test/test.txt");
  });

  app.get("/one/:one/two/:two/:three.jpg", ^(request_t *req, response_t *res) {
    res->sendf("<h1>Params</h1><p>One: %s</p><p>Two: %s</p><p>Three: %s</p>", req->param("one"), req->param("two"), req->param("three"));
  });

  app.get("/form", ^(UNUSED request_t *req, response_t *res) {
    res->send("<form method=\"POST\" action=\"/post/new\">"
              "  <input type=\"text\" name=\"param1\">"
              "  <input type=\"text\" name=\"param2\">"
              "  <input type=\"submit\" value=\"Submit\">"
              "</form>");
  });

  app.post("/post/:form", ^(request_t *req, response_t *res) {
    res->status = 201;
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", req->body("param1"), req->body("param2"));
  });

  app.post("/session", ^(request_t *req, response_t *res) {
    req->session->set("param1", req->body("param1"));
    res->send("ok");
  });

  app.get("/session", ^(request_t *req, response_t *res) {
    res->send(req->session->get("param1"));
  });

  app.get("/m", ^(request_t *req, response_t *res) {
    super_t *super = req->m("super");
    res->send(super->uuid);
  });

  app.get("/set_header", ^(UNUSED request_t *req, response_t *res) {
    res->set("X-Test-1", "test1");
    res->set("X-Test-2", "test2");
    res->send("ok");
  });

  app.get("/set_cookie", ^(UNUSED request_t *req, response_t *res) {
    res->cookie("session", req->query("session"));
    res->cookie("user", req->query("user"));
    res->send("ok");
  });

  app.get("/get_cookie", ^(UNUSED request_t *req, response_t *res) {
    res->sendf("session: %s - user: %s", req->cookie("session"), req->cookie("user"));
  });

  app.listen(port, ^{
    runTests();
  });

  return 0;
}
