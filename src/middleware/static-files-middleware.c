#include <express.h>

error_t *error404(request_t *req);

static char *matchFilepath(request_t *req, const char *path) {
  regex_t regex;
  int reti;
  size_t nmatch = 2;
  regmatch_t pmatch[2];
  size_t patternLen = strlen(path) + strlen("//(.*)") + 1;
  char *pattern = req->malloc(sizeof(char) * patternLen);
  snprintf(pattern, patternLen, "/%s/(.*)", path);
  size_t pathLen = strlen(req->path) + 1;
  char *buffer = req->malloc(sizeof(char) * pathLen);
  strncpy(buffer, req->path, pathLen);
  reti = regcomp(&regex, pattern, REG_EXTENDED);
  if (reti) {
    log_err("regcomp() failed");
    return NULL;
  }
  reti = regexec(&regex, buffer, nmatch, pmatch, 0);
  if (reti == 0) {
    char *fileName = buffer + pmatch[1].rm_so;
    fileName[pmatch[1].rm_eo - pmatch[1].rm_so] = 0;
    size_t filePathLen = strlen(fileName) + strlen(".//") + strlen(path) + 1;
    char *filePath = req->malloc(sizeof(char) * filePathLen);
    snprintf(filePath, filePathLen, "./%s/%s", path, fileName);
    regfree(&regex);
    return filePath;
  }
  regfree(&regex);
  return NULL;
}

middlewareHandler expressStatic(const char *path, const char *fullPath,
                                UNUSED embedded_files_data_t embeddedFiles) {
  return Block_copy(^(request_t *req, response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    if (embeddedFiles.count > 0) {
      const char *reqPath = req->path;
      reqPath++;
      char *data = matchEmbeddedFile(reqPath, embeddedFiles);
      const char *mimetype = getMegaMimeType(req->path);
      if (mimetype != NULL)
        res->set("Content-Type", mimetype);
      if (data != NULL) {
        res->send(data);
        free(data);
      } else {
        next();
      }
      return;
    }

    char *filePath = matchFilepath(req, path);

    char *rPath = realpath(filePath, NULL);
    int isTraversal = rPath && strncmp(rPath, fullPath, strlen(fullPath)) != 0;

    if (rPath)
      free(rPath);

    if (isTraversal) {
      error_t *err = error404(req);
      res->error(err);
      return;
    }

    if (filePath != NULL) {
      res->sendFile(filePath);
      if (res->err)
        next();
    } else {
      next();
    }
  });
}
