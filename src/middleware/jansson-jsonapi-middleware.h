/*
  Copyright (c) 2022 William Cotton

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef JANSSON_JSONAPI_MIDDLEWARE_H
#define JANSSON_JSONAPI_MIDDLEWARE_H

#include <express.h>
#include <jansson.h>

typedef struct jansson_jsonapi_params_t {
  json_t *body;
  json_t *query;
} jansson_jsonapi_params_t;

typedef jansson_jsonapi_params_t jsonapi_params_t;

typedef struct jansson_jsonapi_middleware_t {
  jansson_jsonapi_params_t *params;
  const char *endpointNamespace;
  void (^send)(json_t *);
} jansson_jsonapi_middleware_t;

typedef jansson_jsonapi_middleware_t jsonapi_t;

middlewareHandler janssonJsonapiMiddleware();

#endif // JANSSON_JSONAPI_MIDDLEWARE_H
