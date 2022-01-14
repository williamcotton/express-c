char *curl(char *cmd);
char *curlGet(char *url);
char *curlDelete(char *url);
char *curlPost(char *url, char *data);
char *curlPut(char *url, char *data);
char *curlPatch(char *url, char *data);
void clearState();
