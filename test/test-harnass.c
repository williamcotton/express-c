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

char *curlGet(char *url)
{
  char *curlCmd = "curl -s -o ./test/test-response.html http://127.0.0.1:3032";
  char *cmd = malloc(strlen(curlCmd) + strlen(url) + 1);
  sprintf(cmd, "%s%s", curlCmd, url);
  return curl(cmd);
}

char *curlPost(char *url, char *data)
{
  char *curlCmd = "curl -X POST -s -o ./test/test-response.html -H \"Content-Type: application/x-www-form-urlencoded\" -d \"\" http://127.0.0.1:3032";
  char *cmd = malloc(strlen(curlCmd) + strlen(url) + strlen(data) + 1);
  sprintf(cmd, "curl -X POST -s -o ./test/test-response.html -H \"Content-Type: application/x-www-form-urlencoded\" -d \"%s\" http://127.0.0.1:3032%s", data, url);
  return curl(cmd);
}

int testEq(char *testName, char *response, char *expected)
{
  int status = 0;
  if (strcmp(response, expected) != 0)
  {
    printf("%s: FAIL\n", testName);
    printf("\tExpected: %s\n", expected);
    printf("\tReceived: %s\n", response);
    status = 1;
  }
  else
  {
    printf("%s: PASS\n", testName);
  }
  free(response);
  return status;
}
