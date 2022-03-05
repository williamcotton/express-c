#include "express.h"

server_t *expressServer() {
  server_t *server = malloc(sizeof(server_t));

  server->socket = -1;
  server->serverQueue =
      dispatch_queue_create("serverQueue", DISPATCH_QUEUE_CONCURRENT);

  server->close = Block_copy(^() {
    close(server->socket);
    server->socket = -1;
  });

  server->listen = Block_copy(^(int port) {
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    check(bind(server->socket, (struct sockaddr *)&servAddr,
               sizeof(servAddr)) >= 0,
          "bind() failed");
    check(fcntl(server->socket, F_SETFL, O_NONBLOCK) >= 0, "fcntl() failed");
    check(listen(server->socket, 10000) >= 0, "listen() failed");

    return 0;
  error:
    shutdown(server->socket, SHUT_RDWR);
    close(server->socket);
    return -1;
  });

  server->initSocket = Block_copy(^() {
    int flag = 1;
    check((server->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) >= 0,
          "socket() failed");
    check(setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &flag,
                     sizeof(flag)) >= 0,
          "setsockopt() failed");

    return 0;
  error:
    return -1;
  });

  server->free = Block_copy(^() {
    dispatch_release(server->serverQueue);
    Block_release(server->close);
    Block_release(server->listen);
    Block_release(server->initSocket);
    Block_release(server->free);
  });

  return server;
}
