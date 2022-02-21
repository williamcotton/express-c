#ifdef __linux__

#include "../src/express.h"
#include <Block.h>
#include <stdlib.h>
#include <string.h>
#include <tape/tape.h>

static void shutdownBrokenApp(app_t *app) {
  app->server->close();
  usleep(100);
  app->server->free();
  usleep(100);
  Block_release(app->listen);
  Block_release(app->closeServer);
  Block_release(app->free);
  usleep(100);
  free(app->server);
  free(app);
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

void expressMockSystemCalls(tape_t *t) {
  t->test("mocked system calls", ^(tape_t *t) {
    t->test("server fail", ^(tape_t *t) {
      app_t *app1 = express();
      t->mockFailOnce("socket");
      app1->listen(5000, ^{
                   });
      t->ok("server socket fail", 1);
      shutdownBrokenApp(app1);

      app_t *app2 = express();
      t->mockFailOnce("regcomp");
      app2->get("/test/:params", ^(request_t *req, response_t *res) {
        res->send("test");
      });
      app2->listen(0, ^{
                   });
      t->ok("server regcomp fail", 1);
      shutdownBrokenApp(app2);

      app_t *app3 = express();
      t->mockFailOnce("epoll_ctl");
      app3->listen(0, ^{
                   });
      t->ok("server epoll fail", 1);
      shutdownBrokenApp(app3);

      app_t *app4 = express();
      t->mockFailOnce("listen");
      app4->listen(0, ^{
                   });
      t->ok("server listen fail", 1);
      shutdownBrokenApp(app4);
    });

    // t->test("file failures", ^(tape_t *t) {
    //   t->mockFailOnce("stat");
    //   t->strEqual("stat fail", t->get("/file"), "");

    //   t->mockFailOnce("regcomp");
    //   t->strEqual("regcomp fail", t->get("/file"), "hello, world!\n");
    // });

    t->test("client failures", ^(tape_t *t) {
      t->mockFailOnce("regcomp");
      t->strEqual("regcomp fail", t->get("/"), "Hello World!");

      // TODO: this hangs
      // t->mockFailOnce("accept");
      // t->strEqual("root, accept fail", t->get("/"), "Hello World!");
    });
  });
}

#pragma clang diagnostic pop

#endif // __linux__
