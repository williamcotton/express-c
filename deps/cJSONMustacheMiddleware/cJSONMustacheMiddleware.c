#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Block.h>
#include <dirent.h>
#include <cJSON/cJSON.h>
#include <mustach/mustach-cjson.h>
#include "../src/express.h"

static char *mustacheErrorMessage(int result)
{
  switch (result)
  {
  case MUSTACH_ERROR_SYSTEM:
    return "System error";
  case MUSTACH_ERROR_UNEXPECTED_END:
    return "Unexpected end of template";
  case MUSTACH_ERROR_EMPTY_TAG:
    return "Empty tag";
  case MUSTACH_ERROR_TAG_TOO_LONG:
    return "Tag too long";
  case MUSTACH_ERROR_BAD_SEPARATORS:
    return "Bad separators";
  case MUSTACH_ERROR_TOO_DEEP:
    return "Too deep";
  case MUSTACH_ERROR_CLOSING:
    return "Closing tag without opening tag";
  case MUSTACH_ERROR_BAD_UNESCAPE_TAG:
    return "Bad unescape tag";
  case MUSTACH_ERROR_INVALID_ITF:
    return "Invalid item or partial";
  case MUSTACH_ERROR_ITEM_NOT_FOUND:
    return "Item not found";
  case MUSTACH_ERROR_PARTIAL_NOT_FOUND:
    return "Partial not found";
  default:
    return "Unknown error";
  }
}

middlewareHandler cJSONMustacheMiddleware(char *viewsPath)
{
  char * (^loadTemplate)(char *) = ^(char *templateFile) {
    char *template = NULL;
    size_t length;
    char *templatePath = malloc(strlen(viewsPath) + strlen(templateFile) + 3);
    sprintf(templatePath, "%s/%s", viewsPath, (char *)templateFile);
    FILE *templateFd = fopen(templatePath, "r");
    if (templateFd)
    {
      fseek(templateFd, 0, SEEK_END);
      length = ftell(templateFd);
      fseek(templateFd, 0, SEEK_SET);
      template = malloc(length + 1);
      fread(template, 1, length, templateFd);
      fclose(templateFd);
      template[length] = '\0';
    }
    return template;
  };

  void (^loadPartials)(cJSON *data, char *templateFile) = ^(UNUSED cJSON *data, char *templateFile) {
    UNUSED struct dirent *de;
    DIR *dr = opendir(viewsPath);
    while ((de = readdir(dr)) != NULL)
    {
      if (strstr(de->d_name, ".mustache") && strcmp(de->d_name, templateFile))
      {
        char *partial = loadTemplate(de->d_name);
        char *partialName = strdup(de->d_name);
        char *partialNameSplit = strtok(partialName, ".");
        cJSON_AddStringToObject(data, partialNameSplit, partial);
        free(partial);
      }
    }
    closedir(dr);
  };

  return Block_copy(^(UNUSED request_t *req, response_t *res, void (^next)()) {
    res->render = ^(void *templateFile, void *data) {
      cJSON *json = data;
      char *template = loadTemplate(templateFile);
      if (template)
      {
        size_t length;
        char *renderedTemplate;
        loadPartials(json, (char *)templateFile);
        int result = mustach_cJSON_mem(template, 0, json, 0, &renderedTemplate, &length);
        if (result == 0)
        {
          res->send(renderedTemplate);
        }
        else
        {
          res->status = 500;
          res->sendf("Error rendering mustache template: %s", mustacheErrorMessage(result));
        }
      }
      else
      {
        res->status = 500;
        res->send("Template file not found");
      }
    };
    next();
  });
}
