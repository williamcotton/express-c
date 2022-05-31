#include <express.h>

int main() {
  app_t *app = express();

  int port = 3030;

  /* Close app on Ctrl+C */
  signal(SIGINT, SIG_IGN);
  dispatch_source_t sig_src = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, dispatch_get_main_queue());
  dispatch_source_set_event_handler(sig_src, ^{
    app->closeServer();
    exit(0);
  });
  dispatch_resume(sig_src);

  app->get("/", ^(UNUSED request_t *req, response_t *res) {
    expressResSend(res, "OK");
  });

  app->listen(port, ^{
    printf("app listening at http://localhost:%d\n", port);
  });
}
