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
