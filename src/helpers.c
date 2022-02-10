#include "express.h"

char *matchEmbeddedFile(const char *path, embedded_files_data_t embeddedFiles) {
  size_t pathLen = strlen(path);
  for (int i = 0; i < embeddedFiles.count; i++) {
    int match = 1;
    size_t nameLen = strlen(embeddedFiles.names[i]);
    if (pathLen != nameLen)
      continue;
    for (size_t j = 0; j < nameLen; j++) {
      if (embeddedFiles.names[i][j] != path[j] &&
          embeddedFiles.names[i][j] != '_') {
        match = 0;
      }
    }
    if (match) {
      char *data = malloc(sizeof(char) * (embeddedFiles.lengths[i] + 1));
      memcpy(data, embeddedFiles.data[i], embeddedFiles.lengths[i]);
      data[embeddedFiles.lengths[i]] = '\0';
      return data;
    }
  }
  return (char *)NULL;
};

char *cwdFullPath(const char *path) {
  char cwd[PATH_MAX];
  getcwd(cwd, sizeof(cwd));
  size_t fullPathLen = strlen(cwd) + strlen(path) + 2;
  char *fullPath = malloc(sizeof(char) * fullPathLen);
  snprintf(fullPath, fullPathLen, "%s/%s", cwd, path);
  return fullPath;
}

int writePid(char *pidFile) {
  char buf[100];
  int fd = open(pidFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  snprintf(buf, 100, "%ld\n", (long)getpid());
  return (unsigned long)write(fd, buf, strlen(buf)) == (unsigned long)strlen;
}

unsigned long readPid(char *pidFile) {
  int fd = open(pidFile, O_RDONLY);
  if (fd < 0)
    return -1;
  char buf[100];
  unsigned long pid = 0;
  while (read(fd, buf, 1) > 0) {
    pid = pid * 10 + (unsigned long)(buf[0] - '0');
  }
  return pid;
}

char *generateUuid() {
  char *guid = malloc(sizeof(char) * 37);
  if (guid == NULL)
    return NULL;
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, guid);
  return guid;
}
