#include "express.h"

error_t *error404(request_t *req);

null nullop = ^{
};

static param_match_t *paramMatch(const char *basePath, const char *route) {
  param_match_t *pm = malloc(sizeof(param_match_t));
  pm->keys = malloc(sizeof(char *));
  pm->count = 0;
  pm->regexRoute = NULL;
  char regexRoute[4096];
  regexRoute[0] = '\0';
  size_t basePathRouteLen = strlen(basePath) + strlen(route) + 1;
  char *basePathRoute = malloc(basePathRouteLen);
  snprintf(basePathRoute, basePathRouteLen, "%s%s", basePath, route);
  const char *source = basePathRoute;
  char *regexString = ":([A-Za-z0-9_]*)";
  size_t maxMatches = 100;
  size_t maxGroups = 100;

  regex_t regexCompiled;
  regmatch_t groupArray[maxGroups];
  unsigned int m;
  char *cursor;

  if (regcomp(&regexCompiled, regexString, REG_EXTENDED)) {
    log_err("regcomp() failed");
    free(basePathRoute);
    return pm;
  };

  cursor = (char *)source;
  for (m = 0; m < maxMatches; m++) {
    if (regexec(&regexCompiled, cursor, maxGroups, groupArray, 0))
      break; // No more matches

    unsigned int g = 0;
    unsigned int offset = 0;
    for (g = 0; g < maxGroups; g++) {
      if (groupArray[g].rm_so == (long long)(size_t)-1)
        break; // No more groups

      char cursorCopy[strlen(cursor) + 1];
      strlcpy(cursorCopy, cursor, groupArray[g].rm_eo + 1);

      if (g == 0) {
        offset = groupArray[g].rm_eo;
        sprintf(regexRoute + strlen(regexRoute), "%.*s([^\\/\\s]*)",
                (int)groupArray[g].rm_so, cursorCopy);
      } else {
        pm->keys = realloc(pm->keys, sizeof(char *) * (m + 1));
        pm->count++;
        char *key = malloc(sizeof(char) *
                           (groupArray[g].rm_eo - groupArray[g].rm_so + 1));
        strlcpy(key, cursorCopy + groupArray[g].rm_so,
                groupArray[g].rm_eo - groupArray[g].rm_so + 1);
        pm->keys[m] = key;
      }
    }
    cursor += offset;
  }

  sprintf(regexRoute + strlen(regexRoute), "%s", cursor);

  regfree(&regexCompiled);

  size_t regesRouteLen = strlen(regexRoute) + 3;
  pm->regexRoute = malloc(sizeof(char) * (regesRouteLen));
  snprintf(pm->regexRoute, regesRouteLen, "^%s$", regexRoute);

  free(basePathRoute);

  return pm;
}

static void paramMatchFree(param_match_t *paramMatch) {
  for (int i = 0; i < paramMatch->count; i++) {
    free(paramMatch->keys[i]);
  }
  free(paramMatch->keys);
  if (paramMatch->regexRoute)
    free(paramMatch->regexRoute);
}

static void runErrorHandlers(int index, request_t *req, response_t *res,
                             router_t *router, void (^next)()) {
  if (res->err) {
    if (index < router->errorHandlerCount) {
      router->errorHandlers[index](res->err, req, res, ^{
        runErrorHandlers(index + 1, req, res, router, next);
      });
    } else {
      next();
    }
  }
}

static void runParamHandlers(int index, request_t *req, response_t *res,
                             router_t *router, void (^next)()) {
  const char *paramValue = NULL;
  if (index < router->paramHandlerCount) {
    void (^cleanup)(cleanupHandler) = ^(cleanupHandler cleanupBlock) {
      req->middlewareCleanupBlocks = realloc( // NOLINT
          req->middlewareCleanupBlocks,
          sizeof(cleanupHandler *) * (req->middlewareStackCount + 1));
      req->middlewareCleanupBlocks[req->middlewareStackCount++] =
          (void *)cleanupBlock;
    };
    if (router->paramHandlers[index].paramKey) {
      paramValue = req->params(router->paramHandlers[index].paramKey);
      router->paramHandlers[index].handler(
          req, res, paramValue,
          ^{
            runParamHandlers(index + 1, req, res, router, next);
          },
          cleanup);
      runErrorHandlers(0, req, res, router, nullop);
    } else {
      runParamHandlers(index + 1, req, res, router, next);
    }

  } else {
    next();
  }
}

static void runMiddleware(int index, request_t *req, response_t *res,
                          router_t *router, void (^next)()) {
  if (index < router->middlewareCount) {
    void (^cleanup)(cleanupHandler) = ^(cleanupHandler cleanupBlock) {
      req->middlewareCleanupBlocks = realloc( // NOLINT
          req->middlewareCleanupBlocks,
          sizeof(cleanupHandler *) * (req->middlewareStackCount + 1));
      req->middlewareCleanupBlocks[req->middlewareStackCount++] =
          (void *)cleanupBlock;
    };
    router->middlewares[index].handler(
        req, res,
        ^{
          runMiddleware(index + 1, req, res, router, next);
        },
        cleanup);
    runErrorHandlers(0, req, res, router, nullop);
  } else {
    next();
  }
}

static route_handler_t matchRouteHandler(request_t *req, router_t *router) {
  for (int i = 0; i < router->routeHandlerCount; i++) {
    size_t methodLen = strlen(router->routeHandlers[i].method);
    size_t pathLen = strlen(router->routeHandlers[i].path);
    size_t basePathLen = strlen(router->basePath);

    char *routeHandlerFullPath =
        malloc(sizeof(char) * (basePathLen + pathLen + 1));
    snprintf(routeHandlerFullPath, basePathLen + pathLen + 1, "%s%s",
             router->basePath, router->routeHandlers[i].path);

    if (strncmp(router->routeHandlers[i].method, req->method, methodLen) != 0) {
      free(routeHandlerFullPath);
      continue;
    }

    if (strcmp(routeHandlerFullPath, req->pathMatch) == 0 &&
        router->routeHandlers[i].regex) {
      free(routeHandlerFullPath);
      return router->routeHandlers[i];
    }

    if (strcmp(routeHandlerFullPath, req->path) == 0) {
      free(routeHandlerFullPath);
      return router->routeHandlers[i];
    }

    if (strcmp(routeHandlerFullPath, "") == 0 && strcmp(req->path, "/") == 0) {
      free(routeHandlerFullPath);
      return router->routeHandlers[i];
    }

    free(routeHandlerFullPath);
  }
  return (route_handler_t){.method = NULL, .path = NULL, .handler = NULL};
}

int routerMatchesRequest(router_t *router, request_t *req) {
  if (strchr(router->basePath, ':') != NULL) {
    param_match_t *pm = paramMatch(router->basePath, "");
    regex_t regexCompiled;
    int regexRouteLen = strlen(pm->regexRoute);
    pm->regexRoute[regexRouteLen - 1] = '\0';
    if (regcomp(&regexCompiled, pm->regexRoute, REG_EXTENDED)) {
      log_err("regcomp() failed");
      return 0;
    };
    if (regexec(&regexCompiled, req->path, 0, NULL, 0)) {
      regfree(&regexCompiled);
      paramMatchFree(pm);
      free(pm);
      return 0;
    }
    regfree(&regexCompiled);
    paramMatchFree(pm);
    free(pm);
  } else {
    if (router->basePath && strlen(router->basePath) && req->path) {
      int minLength = min(strlen(router->basePath), strlen(req->path));
      if (strncmp(router->basePath, req->path, minLength) != 0)
        return 0;
    }
  }
  return 1;
}

router_t *expressRouter() {
  __block router_t *router = malloc(sizeof(router_t));

  router->basePath = NULL;
  router->isBaseRouter = 0;
  router->routeHandlers = malloc(sizeof(route_handler_t));
  router->routeHandlerCount = 0;
  router->middlewares = malloc(sizeof(middleware_t));
  router->middlewareCount = 0;
  router->routers = malloc(sizeof(router_t *));
  router->routerCount = 0;
  router->appCleanupBlocks = malloc(sizeof(appCleanupHandler));
  router->appCleanupCount = 0;
  router->paramHandlers = malloc(sizeof(param_handler_t));
  router->paramHandlerCount = 0;
  router->errorHandlers = malloc(sizeof(errorHandler));
  router->errorHandlerCount = 0;

  int (^isBaseRouter)(void) = ^{
    return router->isBaseRouter;
  };

  void (^addRouteHandler)(const char *, const char *, requestHandler) =
      ^(const char *method, const char *path, requestHandler handler) {
        route_handler_t routeHandler = {
            .method = method,
            .path = !isBaseRouter() && strcmp(path, "/") == 0 ? "" : path,
            .regex = strchr(path, ':') != NULL,
            .handler = handler,
        };

        router->routeHandlers =
            realloc(router->routeHandlers,
                    sizeof(route_handler_t) * (router->routeHandlerCount + 1));

        if (isBaseRouter()) {
          routeHandler.basePath = (char *)router->basePath;
          routeHandler.paramMatch =
              routeHandler.regex ? paramMatch(router->basePath, path) : NULL;
        }

        router->routeHandlers[router->routeHandlerCount++] = routeHandler;
      };

  router->get = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("GET", path, handler);
  });

  router->post = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("POST", path, handler);
  });

  router->put = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("PUT", path, handler);
  });

  router->patch = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("PATCH", path, handler);
  });

  router->delete = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("DELETE", path, handler);
  });

  router->all = Block_copy(^(const char *path, requestHandler handler) {
    addRouteHandler("GET", path, handler);
    addRouteHandler("POST", path, handler);
    addRouteHandler("PUT", path, handler);
    addRouteHandler("PATCH", path, handler);
    addRouteHandler("DELETE", path, handler);
  });

  router->error = Block_copy(^(errorHandler handler) {
    router->errorHandlers =
        realloc(router->errorHandlers,
                sizeof(errorHandler) * (router->errorHandlerCount + 1));
    router->errorHandlers[router->errorHandlerCount++] = handler;
  });

  router->param = Block_copy(^(const char *paramKey, paramHandler handler) {
    router->paramHandlers =
        realloc(router->paramHandlers,
                sizeof(param_handler_t) * (router->paramHandlerCount + 1));
    router->paramHandlers[router->paramHandlerCount++] = (param_handler_t){
        .paramKey = paramKey,
        .handler = handler,
    };
  });

  router->use = Block_copy(^(middlewareHandler handler) {
    router->middlewares =
        realloc(router->middlewares,
                sizeof(middleware_t) * (router->middlewareCount + 1));
    router->middlewares[router->middlewareCount++] =
        (middleware_t){.handler = handler};
  });

  router->mountTo = Block_copy(^(router_t *baseRouter) {
    if (!baseRouter->basePath)
      return;

    if (strlen(baseRouter->basePath) == 0 &&
        strcmp(router->mountPath, "/") == 0) {
      router->basePath = strdup("");
    } else {
      size_t basePathLen =
          strlen(baseRouter->basePath) + strlen(router->mountPath) + 1;
      router->basePath = malloc(basePathLen);
      snprintf((char *)router->basePath, basePathLen, "%s%s",
               baseRouter->basePath, router->mountPath);
    }

    for (int i = 0; i < router->routeHandlerCount; i++) {
      router->routeHandlers[i].basePath = (char *)router->basePath;
      router->routeHandlers[i].regex = router->routeHandlers[i].regex ||
                                       strchr(router->basePath, ':') != NULL;
      if (router->routeHandlers[i].regex)
        router->routeHandlers[i].paramMatch =
            paramMatch(router->basePath, router->routeHandlers[i].path);
    }

    for (int i = 0; i < router->routerCount; i++) {
      router->routers[i]->mountTo(router);
    }
  });

  router->useRouter = Block_copy(^(char *mountPath, router_t *routerToMount) {
    routerToMount->mountPath = mountPath;
    router->routers = realloc(router->routers,
                              sizeof(router_t *) * (router->routerCount + 1));
    router->routers[router->routerCount++] = routerToMount;
    routerToMount->mountTo(router);
  });

  router->handler = Block_copy(^(request_t *req, response_t *res) {
    if (!routerMatchesRequest(router, req))
      return;

    runMiddleware(0, req, res, router, ^{
      runParamHandlers(0, req, res, router, ^{
        route_handler_t routeHandler = matchRouteHandler(req, router);
        if (routeHandler.handler != NULL && res->err == NULL) {
          req->baseUrl = routeHandler.basePath;
          req->route = (void *)&routeHandler;
          routeHandler.handler(req, res);
          return;
        }
      });
    });

    for (int i = 0; i < router->routerCount; i++) {
      router->routers[i]->handler(req, res);
    }

    if (isBaseRouter() && res->didSend == 0) {
      if (res->err == NULL) {
        error_t *err = error404(req);
        res->error(err);
      }
    }

    runErrorHandlers(0, req, res, router, nullop);
  });

  router->cleanup = Block_copy(^(appCleanupHandler handler) {
    router->appCleanupBlocks =
        realloc(router->appCleanupBlocks,
                sizeof(appCleanupHandler) * (router->appCleanupCount + 1));
    router->appCleanupBlocks[router->appCleanupCount++] = handler;
  });

  router->free = Block_copy(^(void) {
    /* Free route handlers */
    for (int i = 0; i < router->routeHandlerCount; i++) {
      if (router->routeHandlers[i].regex) {
        paramMatchFree(router->routeHandlers[i].paramMatch);
        free(router->routeHandlers[i].paramMatch);
      }
      Block_release(router->routeHandlers[i].handler);
    }
    free(router->routeHandlers);

    /* Free middleware */
    for (int i = 0; i < router->middlewareCount; i++) {
      Block_release(router->middlewares[i].handler);
    }
    free(router->middlewares);

    /* Free param handlers */
    for (int i = 0; i < router->paramHandlerCount; i++) {
      Block_release(router->paramHandlers[i].handler);
    }
    free(router->paramHandlers);

    /* Free routers */
    for (int i = 0; i < router->routerCount; i++) {
      router->routers[i]->free();
      free((void *)router->routers[i]->basePath);
      free(router->routers[i]);
    }
    free(router->routers);

    /* Free error handlers */
    for (int i = 0; i < router->errorHandlerCount; i++) {
      Block_release(router->errorHandlers[i]);
    }
    free(router->errorHandlers);

    /* Free app cleanup blocks */
    for (int i = 0; i < router->appCleanupCount; i++) {
      router->appCleanupBlocks[i]();
      Block_release(router->appCleanupBlocks[i]);
    }
    free(router->appCleanupBlocks);

    /* Free router */
    Block_release(router->get);
    Block_release(router->post);
    Block_release(router->put);
    Block_release(router->patch);
    Block_release(router->delete);
    Block_release(router->all);
    Block_release(router->use);
    Block_release(router->error);
    Block_release(router->mountTo);
    Block_release(router->useRouter);
    Block_release(router->param);
    Block_release(router->handler);
    Block_release(router->cleanup);
    Block_release(router->free);
  });

  return router;
}
