#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *curl(char *cmd) {
  system(cmd);
  FILE *file = fopen("./test/test-response.html", "r");
  char html[4096];
  size_t bytesRead = fread(html, 1, 4096, file);
  html[bytesRead] = '\0';
  fclose(file);
  system("rm ./test/test-response.html");
  return strdup(html);
}

// TODO: refactor curlGet and curlDelete
char *curlGet(char *url) {
  char *curlCmd =
      "curl -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt"
      " -o ./test/test-response.html http://127.0.0.1:3032";
  char cmd[1024];
  sprintf(cmd, "%s%s", curlCmd, url);
  return curl(cmd);
}

char *curlGetHeaders(char *url) {
  char *curlCmd =
      "curl -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt"
      " -H \"X-Forwarded-For: 1.1.1.1, 2.2.2.2, 3.3.3.3\""
      " -H \"Host: one.two.three.test.com\""
      " -o ./test/test-response.html http://127.0.0.1:3032";
  char cmd[1024];
  sprintf(cmd, "%s%s", curlCmd, url);
  return curl(cmd);
}

char *curlDelete(char *url) {
  char cmd[1024];
  sprintf(
      cmd, "%s%s",
      "curl -X DELETE -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt "
      "-o ./test/test-response.html http://127.0.0.1:3032",
      url);
  return curl(cmd);
}

// TODO: refactor curlPost, curlPut and curlPatch
char *curlPost(char *url, char *data) {
  char cmd[1024];
  sprintf(
      cmd,
      "curl -X POST -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt "
      "-o ./test/test-response.html -H \"Content-Type: "
      "application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s",
      data, url);
  return curl(cmd);
}

char *curlPatch(char *url, char *data) {
  char cmd[1024];
  sprintf(
      cmd,
      "curl -X PATCH -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt "
      "-o ./test/test-response.html -H \"Content-Type: "
      "application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s",
      data, url);
  return curl(cmd);
}

char *curlPut(char *url, char *data) {
  char cmd[1024];
  sprintf(
      cmd,
      "curl -X PUT -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o "
      "./test/test-response.html -H \"Content-Type: "
      "application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s",
      data, url);
  return curl(cmd);
}

void sendData(char *data) {
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

void randomString(char *str, size_t size) {
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK=;:!@#$%^&*()_+-"
                         "=[]{}|/.,<>?0123456789";
  if (size) {
    --size;
    for (size_t n = 0; n < size; n++) {
      int key = rand() % (int)(sizeof charset - 1); // NOLINT
      str[n] = charset[key];
    }
    str[size] = '\0';
  }
}

void clearState() {
  system("rm -f ./test/test-cookies.txt");
  system("rm -f ./test/test-response.html");
}
