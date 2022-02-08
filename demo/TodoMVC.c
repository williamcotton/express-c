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

int main() {
  app_t app = express();

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

  /* Close app on Ctrl+C */
  signal(SIGINT, SIG_IGN);
  dispatch_source_t sig_src = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, dispatch_get_main_queue());
  dispatch_source_set_event_handler(sig_src, ^{
    app.closeServer();
    exit(0);
  });
  dispatch_resume(sig_src);

  /* Load static files */
  char *staticFilesPath = cwdFullPath("demo/public");
  app.use(expressStatic("demo/public", staticFilesPath, embeddedFiles));

  /* Health check */
  app.get("/healthz", ^(UNUSED request_t *req, response_t *res) {
    debug("ip %s", req->ip);
    debug("X-Forwarded-For: %s", req->get("X-Forwarded-For"));
    debug("Hostname %s %s", req->get("Hostname"), req->hostname);
    res->send("OK");
  });

  /* Controllers */
  app.useRouter("/", todosController(embeddedFiles));
  app.useRouter("/api/v1", apiController(databaseUrl, databasePoolSize));

  /* Clean up */
  app.cleanup(^{
    free(staticFilesPath);
  });

  app.listen(port, ^{
    printf("TodoMVC app listening at http://localhost:%d\n", port);
    writePid("server.pid");
  });
}
