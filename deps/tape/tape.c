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

#include <Block.h>
#include <arpa/inet.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string/string.h>
#include <sys/stat.h>
#include <tape/tape.h>
#include <unistd.h>

#ifdef __linux__
#include <bsd/stdlib.h>
#include <sys/epoll.h>
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"

struct memory_struct {
  char *buffer;
  size_t size;
};

// write response data to the memory buffer.
static size_t mem_write(void *contents, size_t size, size_t nmemb,
                        void *userp) {
  // initialize memory_struct
  size_t realsize = size * nmemb;
  struct memory_struct *mem = (struct memory_struct *)userp;

  char *ptr = realloc(mem->buffer, mem->size + realsize + 1);
  if (ptr == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  // copy the response contents to memory_struct buffer.
  mem->buffer = ptr;
  memcpy(&(mem->buffer[mem->size]), contents, realsize); // NOLINT
  mem->size += realsize;
  mem->buffer[mem->size] = 0;

  // return the size of content that is copied.
  return realsize;
}

static string_t *fetch(char *url, char *method, string_collection_t *headers,
                       char *json) {

  // debug("\n---- %s", url);
  struct memory_struct mem;
  CURL *curl;
  CURLcode res;

  mem.buffer = malloc(1);
  mem.size = 0;

  curl_global_init(CURL_GLOBAL_ALL);

  // initialize curl
  curl = curl_easy_init();

  // headers list
  __block struct curl_slist *headersList = NULL;
  if (headers) {
    headers->each(^(string_t *header) {
      headersList = curl_slist_append(headersList, header->value);
    });
  }

  if (strcmp(method, "POST") == 0) {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
  } else if (strcmp(method, "PUT") == 0) {
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  } else if (strcmp(method, "DELETE") == 0) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  } else {
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  }

  if (json != NULL) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
  }

  // specify url, callback function to receive response, buffer to hold
  // response and lastly user agent for http request.
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&mem);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "tape-test");

  // make the http request.
  res = curl_easy_perform(curl);

  // check for errors.
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    return NULL;
  }

  // cleanup
  curl_slist_free_all(headersList);
  curl_easy_cleanup(curl);
  curl_global_cleanup();

  string_t *response = string(mem.buffer);
  free(mem.buffer);

  return response;
}

static string_t *curl(char *cmd) {
  system(cmd);
  FILE *file = fopen("./test/test-response.html", "r");
  char html[4096];
  size_t bytesRead = fread(html, 1, 4096, file);
  html[bytesRead] = '\0';
  fclose(file);
  system("rm ./test/test-response.html");
  return string(html);
}

static string_t *curlGet(char *url) {
  char *curlCmd =
      "curl -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt"
      " -o ./test/test-response.html http://127.0.0.1:3032";
  char cmd[1024];
  sprintf(cmd, "%s%s", curlCmd, url);
  return curl(cmd);
}

static string_t *curlGetHeaders(char *url) {
  char *curlCmd =
      "curl -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt"
      " -H \"X-Forwarded-For: 1.1.1.1, 2.2.2.2, 3.3.3.3\""
      " -H \"Host: one.two.three.test.com\""
      " -o ./test/test-response.html http://127.0.0.1:3032";
  char cmd[1024];
  sprintf(cmd, "%s%s", curlCmd, url);
  return curl(cmd);
}

static string_t *curlDelete(char *url) {
  char cmd[1024];
  sprintf(
      cmd, "%s%s",
      "curl -X DELETE -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt "
      "-o ./test/test-response.html http://127.0.0.1:3032",
      url);
  return curl(cmd);
}

static string_t *curlPost(char *url, char *data) {
  char cmd[1024];
  sprintf(
      cmd,
      "curl -X POST -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt "
      "-o ./test/test-response.html -H \"Content-Type: "
      "application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s",
      data, url);
  return curl(cmd);
}

static string_t *curlPatch(char *url, char *data) {
  char cmd[1024];
  sprintf(
      cmd,
      "curl -X PATCH -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt "
      "-o ./test/test-response.html -H \"Content-Type: "
      "application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s",
      data, url);
  return curl(cmd);
}

static string_t *curlPut(char *url, char *data) {
  char cmd[1024];
  sprintf(
      cmd,
      "curl -X PUT -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o "
      "./test/test-response.html -H \"Content-Type: "
      "application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s",
      data, url);
  return curl(cmd);
}

static void sendData(char *data) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(3032);
  inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
  connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  send(sock, data, strlen(data), 0);
  shutdown(sock, SHUT_RDWR);
  close(sock);
}

static void randomString(char *str, size_t size) {
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK=;:!@#$%^&*()_+-"
                         "=[]{}|/.,<>?0123456789";
  if (size) {
    --size;
    for (size_t n = 0; n < size; n++) {
      int key = arc4random() % (int)(sizeof charset - 1);
      str[n] = charset[key];
    }
    str[size] = '\0';
  }
}

static void clearState() {
  system("rm -f ./test/test-cookies.txt");
  system("rm -f ./test/test-response.html");
}

/* mocks */

int stat_fail_once = 0;
int regcomp_fail_once = 0;
int accept_fail_once = 0;
int epoll_ctl_fail_once = 0;
int socket_fail_once = 0;
int listen_fail_once = 0;

#ifdef __linux__

int __real_stat(const char *path, struct stat *buf);
int __wrap_stat(const char *path, struct stat *buf) {
  if (stat_fail_once) {
    usleep(1000);
    stat_fail_once = 0;
    return -1;
  } else {
    return __real_stat(path, buf);
  }
}

int __real_regcomp(regex_t *preg, const char *regex, int cflags);
int __wrap_regcomp(regex_t *preg, const char *regex, int cflags) {
  if (regcomp_fail_once) {
    usleep(1000);
    regcomp_fail_once = 0;
    return -1;
  } else {
    return __real_regcomp(preg, regex, cflags);
  }
}

int __real_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int __wrap_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  if (accept_fail_once) {
    usleep(1000);
    accept_fail_once = 0;
    return -1;
  } else {
    return __real_accept(sockfd, addr, addrlen);
  }
}

int __real_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int __wrap_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  if (epoll_ctl_fail_once) {
    usleep(1000);
    epoll_ctl_fail_once = 0;
    return -1;
  } else {
    return __real_epoll_ctl(epfd, op, fd, event);
  }
}

int __real_socket(int domain, int type, int protocol);
int __wrap_socket(int domain, int type, int protocol) {
  if (socket_fail_once) {
    usleep(1000);
    socket_fail_once = 0;
    return -1;
  } else {
    return __real_socket(domain, type, protocol);
  }
}

int __real_listen(int sockfd, int backlog);
int __wrap_listen(int sockfd, int backlog) {
  if (listen_fail_once) {
    usleep(1000);
    listen_fail_once = 0;
    return -1;
  } else {
    return __real_listen(sockfd, backlog);
  }
}
#endif

typedef int (^testHandler)(char *name, void (^block)(tape_t *));
testHandler testHandlerFactory(tape_t *root, int level, int ignore,
                               int feature) {
  return Block_copy(^(char *name, void (^block)(tape_t *)) {
    printf("\n%s\n", name);

    /* Initialize tape */
    __block tape_t *t = malloc(sizeof(tape_t));
    t->name = name;

    t->trashableCount = 0;
    t->trash = Block_copy(^(freeHandler freeFn) {
      t->trashables[t->trashableCount++] = freeFn;
    });

    t->ok = ^(char *okName, int condition) {
      root->count++;
      if (condition) {
        printf("\033[32m✓ %s\n\033[0m", okName);
        return 1;
      } else {
        printf("\033[31m✗ %s\n\033[0m", okName);
        root->failed++;
        return 0;
      }
    };

    t->strEqual = ^(char *name, string_t *str1, char *str2) {
      int isEqual = str1->eql(str2);
      t->ok(name, isEqual);
      if (!isEqual) {
        printf("\nExpected: \033[32m\n\n%s\n\n\033[0m", str2);
        printf("Received: \n\n\033[31m%s\033[0m\n", str1->value);
      }
      return isEqual;
    };

    t->mockFailOnce = Block_copy(^(char *name) {
      if (strcmp(name, "stat") == 0) {
        stat_fail_once = 1;
      } else if (strcmp(name, "regcomp") == 0) {
        regcomp_fail_once = 1;
      } else if (strcmp(name, "accept") == 0) {
        accept_fail_once = 1;
      } else if (strcmp(name, "epoll_ctl") == 0) {
        epoll_ctl_fail_once = 1;
      } else if (strcmp(name, "socket") == 0) {
        socket_fail_once = 1;
      } else if (strcmp(name, "listen") == 0) {
        listen_fail_once = 1;
      }
      return;
    });

    t->clearState = ^() {
      clearState();
      return;
    };

    t->randomString = ^(char *str, size_t size) {
      randomString(str, size);
      return;
    };

    t->string = ^(char *str) {
      string_t *s = string(str);
      t->trash(s->free);
      return s;
    };

    t->get = ^(char *url) {
      string_t *response = curlGet(url);
      t->trash(response->free);
      return response;
    };

    t->post = ^(char *url, char *data) {
      string_t *response = curlPost(url, data);
      t->trash(response->free);
      return response;
    };

    t->put = ^(char *url, char *data) {
      string_t *response = curlPut(url, data);
      t->trash(response->free);
      return response;
    };

    t->patch = ^(char *url, char *data) {
      string_t *response = curlPatch(url, data);
      t->trash(response->free);
      return response;
    };

    t->delete = ^(char *url) {
      string_t *response = curlDelete(url);
      t->trash(response->free);
      return response;
    };

    t->getHeaders = ^(char *url) {
      string_t *response = curlGetHeaders(url);
      t->trash(response->free);
      return response;
    };

    t->sendData = ^(char *data) {
      return sendData(data);
    };

    t->fetch =
        ^(char *path, char *method, string_collection_t *headers, char *json) {
          char *baseUrl = "http://127.0.0.1:3032";
          string_t *url = string(baseUrl);
          url->concat(path);
          string_t *response = fetch(url->value, method, headers, json);
          t->trash(response->free);
          t->trash(url->free);
          return response;
        };

    t->test = testHandlerFactory(root, level + 1, ignore, feature);
    // t->xtest = testHandlerFactory(root, level + 1, 1, feature);
    // t->ftest = testHandlerFactory(root, level + 1, ignore, 1);

    /* Call nested tests */
    if (!ignore) {
      block(t);
    }

    /* Final report */
    if (level == 0) {
      if (root->failed == 0)
        printf("\033[32m\n%d tests passed\n\n\033[0m", root->count);
      else
        printf("\033[31m\n%d tests, %d failed\n\n\033[0m", root->count,
               root->failed);
    }

    /* Cleanup */
    for (int i = 0; i < t->trashableCount; i++) {
      t->trashables[i]();
    }
    Block_release(t->trash);
    Block_release(t->test);
    // Block_release(t->xtest);
    // Block_release(t->ftest);
    free(t);

    return root->failed > 0;
  });
}

tape_t *tape() {
  tape_t *tape = malloc(sizeof(tape_t));
  tape->count = 0;
  tape->failed = 0;

  tape->test = testHandlerFactory(tape, 0, 0, 0);

  tape->ok = ^(UNUSED char *name, UNUSED int okBool) {
    return 1;
  };

  return tape;
}

#pragma clang diagnostic pop
