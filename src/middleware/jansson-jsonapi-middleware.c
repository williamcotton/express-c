#include "jansson-jsonapi-middleware.h"

#define JSON_API_MIME_TYPE "application/vnd.api+json"

middlewareHandler janssonJsonapiMiddleware(const char *endpointNamespace) {
  return Block_copy(^(request_t *req, response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    jansson_jsonapi_middleware_t *jsonapi =
        expressReqMalloc(req, sizeof(jansson_jsonapi_middleware_t));

    jsonapi->endpointNamespace = endpointNamespace;

    jsonapi->params = expressReqMalloc(req, sizeof(jansson_jsonapi_params_t));
    jsonapi->params->body = NULL;
    jsonapi->params->query = NULL;

    if (req->bodyString != NULL) {
      jsonapi->params->body = json_loads(req->bodyString, 0, NULL);
    }

    if (req->queryString != NULL) {
      json_t *query = json_object();
      json_t *nested = NULL;
      for (size_t i = 0; i < req->queryKeyValueCount; i++) {
        nested = query;
        char *decodedKey =
            curl_easy_unescape(req->curl, req->queryKeyValues[i].key,
                               req->queryKeyValues[i].keyLen, NULL);
        size_t startOfKey = 0;
        for (size_t j = 0; j < req->queryKeyValues[i].keyLen; j++) {
          if (decodedKey[j] == '[') {
            size_t keyDiff = j - startOfKey;
            if (startOfKey > 0 && keyDiff > 1) {
              keyDiff--;
            }
            char *key = expressReqMalloc(req, keyDiff + 1);
            strncpy(key, decodedKey + startOfKey, keyDiff);
            key[keyDiff] = '\0';
            startOfKey = j + 1;
            json_t *nestedKey = json_object_get(nested, key);
            if (nestedKey == NULL) {
              nestedKey = json_object();
              json_object_set_new(nested, key, nestedKey);
            } else if (!json_is_object(nestedKey)) {
              json_decref(nestedKey);
              nestedKey = json_object();
              json_object_set_new(nested, key, nestedKey);
            }
            nested = nestedKey;
          }
        }
        char *decodedValue =
            curl_easy_unescape(req->curl, req->queryKeyValues[i].value,
                               req->queryKeyValues[i].valueLen, NULL);
        json_t *value = json_array();
        char *tknPtr;
        char *token = strtok_r(decodedValue, ",", &tknPtr);
        while (token != NULL) {
          json_array_append_new(value, json_string(token));
          token = strtok_r(NULL, ",", &tknPtr);
        }
        size_t keyDiff = req->queryKeyValues[i].keyLen - startOfKey;
        if (startOfKey > 0 && keyDiff > 1) {
          keyDiff--;
        }
        char *key = expressReqMalloc(req, keyDiff + 1);
        strncpy(key, decodedKey + startOfKey, keyDiff);
        key[keyDiff] = '\0';
        json_object_set_new(nested, key, value);
        free(decodedKey);
        free(decodedValue);
      }
      jsonapi->params->query = query;
    }

    expressReqMiddlewareSet(req, "jsonapi", jsonapi);

    expressResSetSender(res, "jsonapi", ^(response_t *_res, void *value) {
      char *jsonString = json_dumps((json_t *)value, 0);
      _res->set("Content-Type", JSON_API_MIME_TYPE);
      _res->send(jsonString);
      free(jsonString);
      json_decref((json_t *)value); // CONFLICT
    });

    cleanup(^(request_t *finishedReq) {
      jansson_jsonapi_middleware_t *finishedJsonapi = finishedReq->m("jsonapi");
      if (finishedJsonapi != NULL) {
        if (finishedJsonapi->params->body != NULL) {
          json_decref(finishedJsonapi->params->body);
        }
        if (finishedJsonapi->params->query != NULL) {
          json_decref(finishedJsonapi->params->query); // CONFLICT
        }
      }
    });

    next();
  });
}