#include "express.h"
#include <sys/time.h>

double time_diff(struct timeval x, struct timeval y) {
  double x_ms, y_ms, diff;

  x_ms = (double)x.tv_sec + (double)x.tv_usec;
  y_ms = (double)y.tv_sec + (double)y.tv_usec;

  diff = (double)y_ms - (double)x_ms;

  return diff;
}

void buildResponse(client_t client, request_t *req, response_t *res);
void buildRequest(request_t *req, client_t client, router_t *baseRouter);

void freeRequest(request_t *req);
void freeResponse(response_t *res);

static void closeClientConnection(client_t client) {
  shutdown(client.socket, SHUT_RDWR);
  close(client.socket);
}

static client_t acceptClientConnection(server_t *server) {
  int clientSocket = -1;
  struct sockaddr_in echoClntAddr;
  unsigned int clntLen = sizeof(echoClntAddr);

  check(server->socket >= 0, "server->socket is not valid");

  check_silent(
      (clientSocket = accept(server->socket, (struct sockaddr *)&echoClntAddr,
                             &clntLen)) >= 0,
      "accept() failed");
  check(fcntl(clientSocket, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed");

  char *client_ip = inet_ntoa(echoClntAddr.sin_addr);

  return (client_t){.socket = clientSocket, .ip = client_ip};
error:
  shutdown(clientSocket, SHUT_RDWR);
  if (clientSocket >= 0)
    close(clientSocket);
  return (client_t){.socket = -1, .ip = NULL};
}

#ifdef __linux__
/*

A multi-threaded implementation of the client handler that uses epoll().

*/

#define MAX_EVENTS 64

typedef enum req_status_t { READING, ENDED } req_status_t;

typedef struct http_status_t {
  client_t client;
  req_status_t reqStatus;
  dispatch_source_t timerSource;
} http_status_t;

typedef struct client_thread_args_t {
  int epollFd;
  server_t *server;
  router_t *baseRouter;
} client_thread_args_t;

void *clientAcceptEventHandler(void *args) {
  client_thread_args_t *clientThreadArgs = (client_thread_args_t *)args;

  int epollFd = clientThreadArgs->epollFd;
  server_t *server = clientThreadArgs->server;
  router_t *baseRouter = clientThreadArgs->baseRouter;

  struct epoll_event *events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
  struct epoll_event ev;
  int nfds;

  while (1) {
    nfds = epoll_wait(epollFd, events, MAX_EVENTS, -1);

    if (nfds <= 0) {
      log_err("epoll_wait() failed");
      continue;
    }

    check(server->socket >= 0, "server->socket is not valid");

    for (int n = 0; n < nfds; ++n) {
#ifdef REQUEST_TIMING
      clock_t begin = clock();
      struct timeval before, after;
      gettimeofday(&before, NULL);
#endif // REQUEST_TIMING
      if (events[n].data.fd == server->socket) {
        while (1) {
#ifdef REQUEST_TIMING
          begin = clock();
          gettimeofday(&before, NULL);
#endif // REQUEST_TIMING

          client_t client = acceptClientConnection(server);

          if (client.socket < 0) {
            if (errno == EAGAIN | errno == EWOULDBLOCK) {
              break;
            } else {
              // log_err("accept() failed");
              break;
            }
          }

          ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

          http_status_t *status = malloc(sizeof(http_status_t));
          status->client = client;
          status->reqStatus = READING;

          ev.data.ptr = status;

          dispatch_source_t timerSource = dispatch_source_create(
              DISPATCH_SOURCE_TYPE_TIMER, 0, 0, server->serverQueue);

          status->timerSource = timerSource;

          dispatch_time_t delay = dispatch_time(
              DISPATCH_TIME_NOW, ACCEPT_TIMEOUT_SECS * NSEC_PER_SEC);
          dispatch_source_set_timer(timerSource, delay, delay, 0);
          dispatch_source_set_event_handler(timerSource, ^{
            log_err("timeout");
            dispatch_source_cancel(timerSource);
            dispatch_release(timerSource);
            closeClientConnection(client);
            free(status);
          });
          dispatch_resume(timerSource);

          if (epoll_ctl(epollFd, EPOLL_CTL_ADD, client.socket, &ev) < 0) {
            log_err("epoll_ctl() failed");
            free(status);
            continue;
          }
        }
      } else {
        http_status_t *status = (http_status_t *)events[n].data.ptr;
        if (status->reqStatus == READING) {
          ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
          ev.data.ptr = status;

          dispatch_source_cancel(status->timerSource);
          dispatch_release(status->timerSource);

          client_t client = status->client;

          request_t *req = malloc(sizeof(request_t));
          buildRequest(req, client, baseRouter);

          if (req->method == NULL) {
            free(status);
            free(req);
            closeClientConnection(client);
            continue;
          }

          response_t *res = malloc(sizeof(response_t));
          buildResponse(client, req, res);

          baseRouter->handler(req, res);

#ifdef REQUEST_TIMING
          gettimeofday(&after, NULL);
          clock_t end = clock();
          double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
          printf("%f (%.0lf us)\n", time_spent, time_diff(before, after));
#endif // REQUEST_TIMING

          status->reqStatus = ENDED;

          if (epoll_ctl(epollFd, EPOLL_CTL_MOD, client.socket, &ev) < 0) {
            log_err("epoll_ctl() failed");
            freeResponse(res);
            freeRequest(req);
            closeClientConnection(client);
            continue;
          }

          freeResponse(res);
          freeRequest(req);
          closeClientConnection(client);
        } else if (status->reqStatus == ENDED) {
          free(status);
        }
      }
    }
  }
error:
  free(events);
  return NULL;
}

int initClientAcceptEventHandler(server_t *server, router_t *baseRouter) {

  pthread_t threads[server->threadCount];

  int epollFd = epoll_create1(0);
  check(epollFd >= 0, "epoll_create1() failed");

  client_thread_args_t threadArgs;
  threadArgs.epollFd = epollFd;
  threadArgs.server = server;
  threadArgs.baseRouter = baseRouter;

  struct epoll_event epollEvent;
  epollEvent.events = EPOLLIN | EPOLLET;
  epollEvent.data.fd = server->socket;

  check(epoll_ctl(epollFd, EPOLL_CTL_ADD, server->socket, &epollEvent) >= 0,
        "epoll_ctl() failed");

  for (int i = 0; i < server->threadCount; ++i) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, 1);
    check(pthread_create(&threads[i], NULL, clientAcceptEventHandler,
                         &threadArgs) >= 0,
          "pthread_create() failed");
    pthread_attr_destroy(&attr);
  }

  clientAcceptEventHandler(&threadArgs);

  return 0;
error:
  return -1;
}
#elif __MACH__
/*

A concurrent, multi-threaded implementation of the client handler that uses
dispatch_sources.

For an unknown reason this implementation is not working on linux.

*/
int initClientAcceptEventHandler(server_t *server, router_t *baseRouter) {
  dispatch_source_t acceptSource = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_READ, server->socket, 0, server->serverQueue);

  dispatch_source_set_event_handler(acceptSource, ^{
    const unsigned long numPendingConnections =
        dispatch_source_get_data(acceptSource);
    for (unsigned long i = 0; i < numPendingConnections; i++) {
#ifdef REQUEST_TIMING
      clock_t begin = clock();
      __block struct timeval before, after;
      gettimeofday(&before, NULL);
#endif // REQUEST_TIMING

      client_t client = acceptClientConnection(server);
      if (client.socket < 0)
        continue;

      dispatch_source_t timerSource = dispatch_source_create(
          DISPATCH_SOURCE_TYPE_TIMER, 0, 0, server->serverQueue);
      dispatch_source_t readSource = dispatch_source_create(
          DISPATCH_SOURCE_TYPE_READ, client.socket, 0, server->serverQueue);

      dispatch_time_t delay =
          dispatch_time(DISPATCH_TIME_NOW, ACCEPT_TIMEOUT_SECS * NSEC_PER_SEC);
      dispatch_source_set_timer(timerSource, delay, delay, 0);
      dispatch_source_set_event_handler(timerSource, ^{
        log_err("timeout");
        dispatch_source_cancel(timerSource);
        dispatch_release(timerSource);
        closeClientConnection(client);
        dispatch_source_cancel(readSource);
        dispatch_release(readSource);
      });

      dispatch_source_set_event_handler(readSource, ^{
        dispatch_source_cancel(timerSource);
        dispatch_release(timerSource);

        request_t *req = malloc(sizeof(request_t));
        buildRequest(req, client, baseRouter);

        if (req->method == NULL) {
          free(req);
          closeClientConnection(client);
          dispatch_source_cancel(readSource);
          dispatch_release(readSource);
          return;
        }

        response_t *res = malloc(sizeof(response_t));
        buildResponse(client, req, res);

        baseRouter->handler(req, res);

#ifdef REQUEST_TIMING
        gettimeofday(&after, NULL);
        clock_t end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("%f (%.0lf us)\n", time_spent, time_diff(before, after));
#endif // REQUEST_TIMING

        closeClientConnection(client);
        freeResponse(res);
        freeRequest(req);
        dispatch_source_cancel(readSource);
        dispatch_release(readSource);
      });

      dispatch_resume(timerSource);
      dispatch_resume(readSource);
    }
  });
  dispatch_resume(acceptSource);

  return 1;
}
#endif
