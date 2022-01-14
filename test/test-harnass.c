#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *curl(char *cmd)
{
  system(cmd);
  FILE *file = fopen("./test/test-response.html", "r");
  char *html = malloc(4096);
  size_t bytesRead = fread(html, 1, 4096, file);
  html[bytesRead] = '\0';
  fclose(file);
  system("rm ./test/test-response.html");
  return html;
}

// TODO: refactor curlGet and curlDelete
char *curlGet(char *url)
{
  char *curlCmd = "curl -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o ./test/test-response.html http://127.0.0.1:3032";
  char *cmd = malloc(strlen(curlCmd) + strlen(url) + 1);
  sprintf(cmd, "%s%s", curlCmd, url);
  return curl(cmd);
}

char *curlDelete(char *url)
{
  char *curlCmd = "curl -X DELETE -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o ./test/test-response.html http://127.0.0.1:3032";
  char *cmd = malloc(strlen(curlCmd) + strlen(url) + 1);
  sprintf(cmd, "%s%s", curlCmd, url);
  return curl(cmd);
}

// TODO: refactor curlPost, curlPut and curlPatch
char *curlPost(char *url, char *data)
{
  char *curlCmd = "curl -X POST -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o ./test/test-response.html -H \"Content-Type: application/x-www-form-urlencoded\" -d \"\" http://127.0.0.1:3032";
  char *cmd = malloc(strlen(curlCmd) + strlen(url) + strlen(data) + 1);
  sprintf(cmd, "curl -X POST -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o ./test/test-response.html -H \"Content-Type: application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s", data, url);
  return curl(cmd);
}

char *curlPatch(char *url, char *data)
{
  char *curlCmd = "curl -X PATCH -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o ./test/test-response.html -H \"Content-Type: application/x-www-form-urlencoded\" -d \"\" http://127.0.0.1:3032";
  char *cmd = malloc(strlen(curlCmd) + strlen(url) + strlen(data) + 1);
  sprintf(cmd, "curl -X PATCH -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o ./test/test-response.html -H \"Content-Type: application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s", data, url);
  return curl(cmd);
}

char *curlPut(char *url, char *data)
{
  char *curlCmd = "curl -X PUT -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o ./test/test-response.html -H \"Content-Type: application/x-www-form-urlencoded\" -d \"\" http://127.0.0.1:3032";
  char *cmd = malloc(strlen(curlCmd) + strlen(url) + strlen(data) + 1);
  sprintf(cmd, "curl -X PUT -s -c ./test/test-cookies.txt -b ./test/test-cookies.txt -o ./test/test-response.html -H \"Content-Type: application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s", data, url);
  return curl(cmd);
}

void clearState()
{
  system("rm -f ./test/test-cookies.txt");
  system("rm -f ./test/test-response.html");
}
