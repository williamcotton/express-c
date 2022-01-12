char *curl(char *cmd);
char *curlGet(char *url);
char *curlPost(char *url, char *data);
void clearState();
int testEq(char *testName, char *response, char *expected);
