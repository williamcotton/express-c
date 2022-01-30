/*
  Copyright (c) 2022 William Cotton

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

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

middlewareHandler cJSONMustacheMiddleware(char *viewsPath, embedded_files_data_t embeddedFiles)
{
  char * (^loadTemplate)(char *) = ^(char *templateFile) {
    char *template = NULL;
    char *templatePath = malloc(strlen(viewsPath) + strlen(templateFile) + 3);
    sprintf(templatePath, "%s/%s", viewsPath, (char *)templateFile);
    if (embeddedFiles.count > 0)
    {
      template = matchEmbeddedFile(templatePath, embeddedFiles);
      return template;
    }
    FILE *templateFd = fopen(templatePath, "r");
    if (templateFd)
    {
      fseek(templateFd, 0, SEEK_END);
      size_t length = ftell(templateFd);
      fseek(templateFd, 0, SEEK_SET);
      template = malloc(length + 1);
      fread(template, 1, length, templateFd);
      template[length] = '\0';
      fclose(templateFd);
    }
    free(templatePath);
    return template;
  };

  void (^loadPartials)(cJSON *data, char *templateFile) = ^(cJSON *data, char *templateFile) {
    if (embeddedFiles.count > 0)
    {
      char *templateName = strtok(templateFile, ".");
      for (int i = 0; i < embeddedFiles.count; i++)
      {
        if (strstr(embeddedFiles.names[i], "mustache"))
        {
          char *partialName = embeddedFiles.names[i];
          char *partial = (char *)embeddedFiles.data[i];

          char *token = strdup(partialName);
          char *extention = token;
          char *name = token;
          strtok(token, "_");
          while ((token = strtok(NULL, "_")) != NULL)
          {
            name = extention;
            extention = token;
          }

          if (strcmp(name, templateName) == 0)
            continue;

          cJSON_AddStringToObject(data, name, partial);
          free(token);
        }
      }
      return;
    }
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
        free(partialName);
      }
    }
    closedir(dr);
  };

  return Block_copy(^(UNUSED request_t *req, response_t *res, void (^next)(), UNUSED void (^cleanup)(cleanupHandler)) {
    res->render = ^(void *templateName, void *data) {
      cJSON *json = data;
      char *templateFile;
      if (strstr(templateName, ".mustache"))
      {
        templateFile = templateName;
      }
      else
      {
        templateFile = malloc(strlen(templateName) + strlen(".mustache") + 1);
        sprintf(templateFile, "%s.mustache", (char *)templateName);
      }
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
          free(renderedTemplate);
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
      free(templateFile);
      free(template);
      cJSON_Delete(json);
    };
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));
    next();
  });
}
