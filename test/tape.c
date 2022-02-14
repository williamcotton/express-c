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

#include "tape.h"
#include <Block.h>
#include <arpa/inet.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"

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
    usleep(100);
    stat_fail_once = 0;
    return -1;
  } else {
    return __real_stat(path, buf);
  }
}

int __real_regcomp(regex_t *preg, const char *regex, int cflags);
int __wrap_regcomp(regex_t *preg, const char *regex, int cflags) {
  if (regcomp_fail_once) {
    usleep(100);
    regcomp_fail_once = 0;
    return -1;
  } else {
    return __real_regcomp(preg, regex, cflags);
  }
}

int __real_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int __wrap_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  if (accept_fail_once) {
    usleep(100);
    accept_fail_once = 0;
    return -1;
  } else {
    return __real_accept(sockfd, addr, addrlen);
  }
}

int __real_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int __wrap_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  if (epoll_ctl_fail_once) {
    usleep(100);
    epoll_ctl_fail_once = 0;
    return -1;
  } else {
    return __real_epoll_ctl(epfd, op, fd, event);
  }
}

int __real_socket(int domain, int type, int protocol);
int __wrap_socket(int domain, int type, int protocol) {
  if (socket_fail_once) {
    usleep(100);
    socket_fail_once = 0;
    return -1;
  } else {
    return __real_socket(domain, type, protocol);
  }
}

int __real_listen(int sockfd, int backlog);
int __wrap_listen(int sockfd, int backlog) {
  if (listen_fail_once) {
    usleep(100);
    listen_fail_once = 0;
    return -1;
  } else {
    return __real_listen(sockfd, backlog);
  }
}
#endif

typedef int (^testHandler)(char *name, void (^block)(tape_t *));
testHandler testHandlerFactory(tape_t *root, int level) {
  return Block_copy(^(char *name, void (^block)(tape_t *)) {
    printf("\n%s\n", name);
    __block tape_t *t = malloc(sizeof(tape_t));
    t->name = name;
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
    t->strEqual = ^(char *name, char *str1, char *str2) {
      int result = strcmp(str1, str2) == 0;
      t->ok(name, result);
      if (!result) {
        printf("\nExpected: \033[32m\n\n%s\n\n\033[0m", str2);
        printf("Received: \n\n\033[31m%s\033[0m\n", str1);
      }
      free(str1);
      return result;
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

    t->test = testHandlerFactory(root, level + 1);
    block(t);
    if (level == 0) {
      if (root->failed == 0)
        printf("\033[32m\n%d tests passed\n\n\033[0m", root->count);
      else
        printf("\033[31m\n%d tests, %d failed\n\n\033[0m", root->count,
               root->failed);
    }
    Block_release(t->test);
    free(t);
    return root->failed > 0;
  });
}

tape_t *tape() {
  tape_t *tape = malloc(sizeof(tape_t));
  tape->count = 0;
  tape->failed = 0;

  tape->test = testHandlerFactory(tape, 0);

  tape->ok = ^(UNUSED char *name, UNUSED int okBool) {
    return 1;
  };

  return tape;
}

#pragma clang diagnostic pop
