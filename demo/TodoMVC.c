#include "controllers/api/v1.h"
#include "controllers/todo.h"
#include <dotenv-c/dotenv.h>
#include <express.h>
#include <stdlib.h>

#ifdef EMBEDDED_FILES
#include "embeddedFiles.h"
#else
embedded_files_data_t embeddedFiles = {0};
#endif // EMBEDDED_FILES

int main() {
  app_t *app = express();

  /* Use req helpers, eg, req->get("Host") vs expressReqGet(req, "Host") */
  app->use(expressHelpersMiddleware());

  /* Load .env file */
  env_load(".", false);

  /* Environment variables */
  char *PORT = getenv("PORT");
  char *DATABASE_URL = getenv("DATABASE_URL");
  char *DATABASE_POOL_SIZE = getenv("DATABASE_POOL_SIZE");
  int port = PORT ? atoi(PORT) : 3495;
  int databasePoolSize = DATABASE_POOL_SIZE ? atoi(DATABASE_POOL_SIZE) : 5;
  const char *databaseUrl =
      DATABASE_URL ? DATABASE_URL : "postgresql://localhost/express-demo";

  /* Close app on Ctrl+C */
  signal(SIGINT, SIG_IGN);
  dispatch_source_t sig_src = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, dispatch_get_main_queue());
  dispatch_source_set_event_handler(sig_src, ^{
    app->closeServer();
    exit(0);
  });
  dispatch_resume(sig_src);

  /* Load static files */
  char *staticFilesPath = cwdFullPath("public");
  app->use(expressStatic("public", staticFilesPath, embeddedFiles));

  /* Health check */
  app->get("/healthz", ^(UNUSED request_t *req, response_t *res) {
    res->send("OK");
  });

  /* Controllers */
  app->useRouter("/", todosController(embeddedFiles));
  app->useRouter("/api/v1", resourceRouter(databaseUrl, databasePoolSize));

  /* Clean up */
  app->cleanup(^{
    free(staticFilesPath);
  });

  app->listen(port, ^{
    printf("TodoMVC app listening at http://localhost:%d\n", port);
    writePid("server.pid");
  });
}
