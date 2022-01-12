char *curl(char *cmd);
char *curlGet(char *url);
char *curlPost(char *url, char *data);
int testEq(char *testName, char *response, char *expected);
