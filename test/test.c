#include "../src/express.h"
#include "../src/status_message.h"
#include "tape.h"
#include "test-helpers.h"
#include <Block.h>
#include <curl/curl.h>
#include <dotenv-c/dotenv.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif

/* mocks */

#ifdef __linux__
int stat_fail = 0;
int __real_stat(const char *path, struct stat *buf);
int __wrap_stat(const char *path, struct stat *buf) {
  if (stat_fail) {
    return -1;
  } else {
    return __real_stat(path, buf);
  }
}

int regcomp_fail = 0;
int __real_regcomp(regex_t *preg, const char *regex, int cflags);
int __wrap_regcomp(regex_t *preg, const char *regex, int cflags) {
  if (regcomp_fail) {
    return -1;
  } else {
    return __real_regcomp(preg, regex, cflags);
  }
}

int accept_fail = 0;
int __real_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int __wrap_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  if (accept_fail) {
    return -1;
  } else {
    return __real_accept(sockfd, addr, addrlen);
  }
}

int epoll_ctl_fail = 0;
int __real_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int __wrap_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  if (epoll_ctl_fail) {
    debug("epoll_ctl failed");
    return -1;
  } else {
    return __real_epoll_ctl(epfd, op, fd, event);
  }
}

int socket_fail = 0;
int __real_socket(int domain, int type, int protocol);
int __wrap_socket(int domain, int type, int protocol) {
  if (socket_fail) {
    return -1;
  } else {
    return __real_socket(domain, type, protocol);
  }
}
#endif

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

#ifdef __linux__
    t->test("mocked system calls", ^(tape_t *t) {
      t->test("server fail", ^(tape_t *t) {
        app_t app1 = express();

        socket_fail = 1;
        app1.listen(5000, ^{
                    });
        t->ok("server socket fail", 1);
        Block_release(app1.listen);
        Block_release(app1.closeServer);
        app1.server->close();
        usleep(1);
        free(app1.server);
        socket_fail = 0;

        app_t app2 = express();
        regcomp_fail = 1;
        app2.get("/test/:params", ^(request_t *req, response_t *res) {
          res->send("test");
        });
        app2.listen(0, ^{
                    });
        t->ok("server regcomp fail", 1);
        Block_release(app2.listen);
        Block_release(app2.closeServer);
        app2.server->close();
        usleep(1);
        free(app2.server);
        regcomp_fail = 0;

        app_t app3 = express();
        epoll_ctl_fail = 1;
        app3.listen(0, ^{
                    });
        t->ok("server epoll fail", 1);
        Block_release(app3.listen);
        Block_release(app3.closeServer);
        app3.server->close();
        usleep(1);
        free(app3.server);
        epoll_ctl_fail = 0;
      });

      t->test("file failures", ^(tape_t *t) {
        stat_fail = 1;
        t->strEqual("stat fail", curlGet("/file"), "");
        stat_fail = 0;

        regcomp_fail = 1;
        t->strEqual("regcomp fail", curlGet("/file"), "hello, world!\n");
        regcomp_fail = 0;
      });

      t->test("client failures", ^(tape_t *t) {
        regcomp_fail = 1;
        t->strEqual("regcomp fail", curlGet("/"), "Hello World!");
        regcomp_fail = 0;

        // TODO: this hangs
        // accept_fail = 1;
        // t->strEqual("root, accept fail", curlGet("/"), "Hello World!");
        // accept_fail = 0;
      });
    });
#endif

    t->test("statusMessage", ^(tape_t *t) {
      t->ok("100", strlen(getStatusMessage(100)));
      t->ok("101", strlen(getStatusMessage(101)));
      t->ok("102", strlen(getStatusMessage(102)));
      t->ok("103", strlen(getStatusMessage(103)));
      t->ok("200", strlen(getStatusMessage(200)));
      t->ok("201", strlen(getStatusMessage(201)));
      t->ok("202", strlen(getStatusMessage(202)));
      t->ok("203", strlen(getStatusMessage(203)));
      t->ok("204", strlen(getStatusMessage(204)));
      t->ok("205", strlen(getStatusMessage(205)));
      t->ok("206", strlen(getStatusMessage(206)));
      t->ok("207", strlen(getStatusMessage(207)));
      t->ok("208", strlen(getStatusMessage(208)));
      t->ok("226", strlen(getStatusMessage(226)));
      t->ok("300", strlen(getStatusMessage(300)));
      t->ok("301", strlen(getStatusMessage(301)));
      t->ok("302", strlen(getStatusMessage(302)));
      t->ok("303", strlen(getStatusMessage(303)));
      t->ok("304", strlen(getStatusMessage(304)));
      t->ok("305", strlen(getStatusMessage(305)));
      t->ok("306", strlen(getStatusMessage(306)));
      t->ok("307", strlen(getStatusMessage(307)));
      t->ok("308", strlen(getStatusMessage(308)));
      t->ok("400", strlen(getStatusMessage(400)));
      t->ok("401", strlen(getStatusMessage(401)));
      t->ok("402", strlen(getStatusMessage(402)));
      t->ok("403", strlen(getStatusMessage(403)));
      t->ok("404", strlen(getStatusMessage(404)));
      t->ok("405", strlen(getStatusMessage(405)));
      t->ok("406", strlen(getStatusMessage(406)));
      t->ok("407", strlen(getStatusMessage(407)));
      t->ok("408", strlen(getStatusMessage(408)));
      t->ok("409", strlen(getStatusMessage(409)));
      t->ok("410", strlen(getStatusMessage(410)));
      t->ok("411", strlen(getStatusMessage(411)));
      t->ok("412", strlen(getStatusMessage(412)));
      t->ok("413", strlen(getStatusMessage(413)));
      t->ok("414", strlen(getStatusMessage(414)));
      t->ok("415", strlen(getStatusMessage(415)));
      t->ok("416", strlen(getStatusMessage(416)));
      t->ok("417", strlen(getStatusMessage(417)));
      t->ok("418", strlen(getStatusMessage(418)));
      t->ok("421", strlen(getStatusMessage(421)));
      t->ok("422", strlen(getStatusMessage(422)));
      t->ok("423", strlen(getStatusMessage(423)));
      t->ok("424", strlen(getStatusMessage(424)));
      t->ok("425", strlen(getStatusMessage(425)));
      t->ok("426", strlen(getStatusMessage(426)));
      t->ok("428", strlen(getStatusMessage(428)));
      t->ok("429", strlen(getStatusMessage(429)));
      t->ok("431", strlen(getStatusMessage(431)));
      t->ok("451", strlen(getStatusMessage(451)));
      t->ok("500", strlen(getStatusMessage(500)));
      t->ok("501", strlen(getStatusMessage(501)));
      t->ok("502", strlen(getStatusMessage(502)));
      t->ok("503", strlen(getStatusMessage(503)));
      t->ok("504", strlen(getStatusMessage(504)));
      t->ok("505", strlen(getStatusMessage(505)));
      t->ok("506", strlen(getStatusMessage(506)));
      t->ok("507", strlen(getStatusMessage(507)));
      t->ok("508", strlen(getStatusMessage(508)));
      t->ok("510", strlen(getStatusMessage(510)));
      t->ok("511", strlen(getStatusMessage(511)));
      t->ok("0", strlen(getStatusMessage(0)));
    });

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

    t->test("matchEmbeddedFile", ^(tape_t *t) {
      unsigned char demo_public_app_css[] = {0x68};
      unsigned int demo_public_app_css_len = 1;
      unsigned char *embeddedFilesData[] = {demo_public_app_css};
      int embeddedFilesLengths[] = {1};
      char *embeddedFilesNames[] = {"demo_public_app_css"};
      const int embeddedFilesCount = 1;
      embedded_files_data_t embeddedFiles = {
          embeddedFilesData, embeddedFilesLengths, embeddedFilesNames,
          embeddedFilesCount};
      t->strEqual("matching file",
                  matchEmbeddedFile("demo/public/app.css", embeddedFiles), "h");

      const char *bogus = matchEmbeddedFile("bogus", embeddedFiles);
      t->ok("bogus file", bogus == NULL);
    });

    t->test("writePid", ^(tape_t *t) {
      char *pidFile = "test.pid";
      writePid(pidFile);
      t->ok("writePid", readPid(pidFile) > 0);
      unlink(pidFile);
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
      t->strEqual(
          "body method",
          curlPost("/put/form123", "param1=12%2B3&param2=3+4%205&_method=put"),
          "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    });

    t->test("PATCH", ^(tape_t *t) {
      t->strEqual("patch form data",
                  curlPatch("/patch/form123", "param1=12%2B3&param2=3+4%205"),
                  "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
      t->strEqual("body method",
                  curlPost("/patch/form123",
                           "param1=12%2B3&param2=3+4%205&_method=patch"),
                  "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    });

    t->test("DELETE", ^(tape_t *t) {
      t->strEqual("delete form data", curlDelete("/delete/form123"),
                  "<h1>Delete</h1><p>ID: form123</p>");
      t->strEqual("body method", curlPost("/delete/form123", "_method=delete"),
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
      t->strEqual("download", curlGet("/download"), "hello, world!\n");
      char *missingFile = curlGet("/download_missing");
      t->ok("download missing", missingFile != NULL);
      free(missingFile);
    });

    t->test("Session", ^(tape_t *t) {
      t->strEqual("session set", curlPost("/session", "param1=session-data"),
                  "ok");
      t->strEqual("session get", curlGet("/session"), "session-data");
    });

    t->test("Header", ^(tape_t *t) {
      t->strEqual(
          "headers", curlGetHeaders("/headers"),
          "<h1>Headers</h1><p>Host: one.two.three.test.com</p><p>Accept: "
          "*/*</p><p>3 Subdomains: one two three</p><p>3 IPs: 1.1.1.1 2.2.2.2 "
          "3.3.3.3</p>");
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
                    "super-params-router test blip");
      });

      t->test("Root router", ^(tape_t *t) {
        t->strEqual("root route", curlGet("/root"), "Hello Root Router!");
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
  });

  app.get("/headers", ^(request_t *req, response_t *res) {
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

  app.get("/file", ^(UNUSED request_t *req, response_t *res) {
    res->sendFile("./test/test.txt");
  });

  app.get("/download", ^(UNUSED request_t *req, response_t *res) {
    res->download("./test/test.txt", NULL);
  });

  app.get("/download_missing", ^(UNUSED request_t *req, response_t *res) {
    res->download("test_missing.txt", NULL);
  });

  app.get("/one/:one/two/:two/:three.jpg", ^(request_t *req, response_t *res) {
    char *one = req->params("one");
    char *two = req->params("two");
    char *three = req->params("three");
    res->sendf("<h1>Params</h1><p>One: %s</p><p>Two: %s</p><p>Three: %s</p>",
               one, two, three);
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
  });

  app.post("/session", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    req->session->set("param1", strdup(param1));
    res->send("ok");
  });

  app.get("/session", ^(request_t *req, response_t *res) {
    char *param1 = req->session->get("param1");
    res->send(param1);
    free(param1);
  });

  app.put("/put/:form", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    char *param2 = req->body("param2");
    res->status = 201;
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", param1,
               param2);
  });

  app.patch("/patch/:form", ^(request_t *req, response_t *res) {
    char *param1 = req->body("param1");
    char *param2 = req->body("param2");
    res->status = 201;
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", param1,
               param2);
  });

  app.delete("/delete/:id", ^(request_t *req, response_t *res) {
    char *id = req->params("id");
    res->sendf("<h1>Delete</h1><p>ID: %s</p>", id);
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

  app.useRouter("/", rootRouter);
  router->useRouter("/params/:id", paramsRouter);
  app.useRouter("/base", router);
  router->useRouter("/nested", nestedRouter);

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
