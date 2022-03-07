#include "jansson-jsonapi-middleware.h"

#define JSON_API_MIME_TYPE "application/vnd.api+json"

middlewareHandler janssonJsonapiMiddleware() {
  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    char *contentType = (char *)req->get("Content-Type");
    if (contentType != NULL &&
        strncmp(contentType, JSON_API_MIME_TYPE, 25) == 0) {

      jansson_jsonapi_middleware_t *jsonapi =
          req->malloc(sizeof(jansson_jsonapi_middleware_t));

      jsonapi->body = NULL;
      if (req->bodyString != NULL) {
        jsonapi->body = json_loads(req->bodyString, 0, NULL);
      }

      jsonapi->query = NULL;
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
              if (startOfKey > 0) {
                keyDiff--;
              }
              char *key = req->malloc(keyDiff + 1);
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
          char *token = strtok(decodedValue, ",");
          while (token != NULL) {
            json_array_append_new(value, json_string(token));
            token = strtok(NULL, ",");
          }
          size_t keyDiff = req->queryKeyValues[i].keyLen - startOfKey;
          if (startOfKey > 0) {
            keyDiff--;
          }
          char *key = req->malloc(keyDiff + 1);
          strncpy(key, decodedKey + startOfKey, keyDiff);
          key[keyDiff] = '\0';
          json_object_set_new(nested, key, value);
          free(decodedKey);
          free(decodedValue);
        }
        jsonapi->query = query;
      }

      req->mSet("jsonapi", jsonapi);
    };

    cleanup(Block_copy(^(request_t *finishedReq) {
      jansson_jsonapi_middleware_t *jsonapi = finishedReq->m("jsonapi");
      if (jsonapi != NULL) {
        if (jsonapi->body != NULL) {
          json_decref(jsonapi->body);
        }
        if (jsonapi->query != NULL) {
          json_decref(jsonapi->query);
        }
      }
    }));

    next();
  });
}