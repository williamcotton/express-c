#include "resource.h"

void addDefaultFiltersToAttribute(resource_t *resource, model_t *model,
                                  memory_manager_t *memoryManager,
                                  char *attributeName, char *attributeType) {
  if (strcmp(attributeType, "integer") == 0 ||
      strcmp(attributeType, "integer_id") == 0 ||
      strcmp(attributeType, "big_decimal") == 0 ||
      strcmp(attributeType, "float") == 0 ||
      strcmp(attributeType, "date") == 0 ||
      strcmp(attributeType, "datetime") == 0) {

    resource->filter(
        attributeName, "gt",
        mmBlockCopy(memoryManager,
                    ^(query_t *scope, const char **values, UNUSED int count) {
                      const char *value = values[0];
                      char *whereCondition =
                          mmMalloc(model->memoryManager,
                                   strlen(attributeName) + strlen(" > $") + 1);
                      sprintf(whereCondition, "%s > $", attributeName);
                      return scope->where(whereCondition, value);
                    }));

    resource->filter(
        attributeName, "gte",
        mmBlockCopy(memoryManager,
                    ^(query_t *scope, const char **values, UNUSED int count) {
                      const char *value = values[0];
                      char *whereCondition =
                          mmMalloc(model->memoryManager,
                                   strlen(attributeName) + strlen(" >= $") + 1);
                      sprintf(whereCondition, "%s >= $", attributeName);
                      return scope->where(whereCondition, value);
                    }));

    resource->filter(
        attributeName, "lt",
        mmBlockCopy(memoryManager,
                    ^(query_t *scope, const char **values, UNUSED int count) {
                      const char *value = values[0];
                      char *whereCondition =
                          mmMalloc(model->memoryManager,
                                   strlen(attributeName) + strlen(" < $") + 1);
                      sprintf(whereCondition, "%s < $", attributeName);
                      return scope->where(whereCondition, value);
                    }));

    resource->filter(
        attributeName, "lte",
        mmBlockCopy(memoryManager,
                    ^(query_t *scope, const char **values, UNUSED int count) {
                      const char *value = values[0];
                      char *whereCondition =
                          mmMalloc(model->memoryManager,
                                   strlen(attributeName) + strlen(" <= $") + 1);
                      sprintf(whereCondition, "%s <= $", attributeName);
                      return scope->where(whereCondition, value);
                    }));
  }

  if (strcmp(attributeType, "enum") == 0 ||
      strcmp(attributeType, "uuid") == 0 ||
      strcmp(attributeType, "integer") == 0 ||
      strcmp(attributeType, "integer_id") == 0 ||
      strcmp(attributeType, "big_decimal") == 0 ||
      strcmp(attributeType, "float") == 0 ||
      strcmp(attributeType, "date") == 0 ||
      strcmp(attributeType, "boolean") == 0 ||
      strcmp(attributeType, "datetime") == 0) {

    resource->filter(
        attributeName, "eq",
        mmBlockCopy(
            memoryManager, ^(query_t *scope, const char **values, int count) {
              if (count == 1) {
                char *whereCondition =
                    mmMalloc(model->memoryManager,
                             strlen(attributeName) + strlen(" = $") + 1);
                sprintf(whereCondition, "%s = $", attributeName);
                return scope->where(whereCondition, values[0]);
              }
              return scope->whereIn(attributeName, true, values, count);
            }));

    resource->filter(
        attributeName, "not_eq",
        mmBlockCopy(
            memoryManager, ^(query_t *scope, const char **values, int count) {
              if (count == 1) {
                char *whereCondition =
                    mmMalloc(model->memoryManager,
                             strlen(attributeName) + strlen(" NOT = $") + 1);
                sprintf(whereCondition, "NOT %s = $", attributeName);
                return scope->where(whereCondition, values[0]);
              }
              return scope->whereIn(attributeName, false, values, count);
            }));
  }

  if (strcmp(attributeType, "string") == 0) {
    resource->filter(
        attributeName, "eq",
        mmBlockCopy(
            memoryManager, ^(query_t *scope, const char **values, int count) {
              if (count == 1) {
                char *whereCondition = mmMalloc(
                    model->memoryManager,
                    strlen(attributeName) + strlen("LOWER() = LOWER($)") + 1);
                sprintf(whereCondition, "LOWER(%s) = LOWER($)", attributeName);
                return scope->where(whereCondition, values[0]);
              }
              // TODO: support LOWER
              return scope->whereIn(attributeName, true, values, count);
            }));

    resource->filter(
        attributeName, "not_eq",
        mmBlockCopy(memoryManager, ^(query_t *scope, const char **values,
                                     int count) {
          if (count == 1) {
            char *whereCondition = mmMalloc(
                model->memoryManager,
                strlen(attributeName) + strlen("NOT LOWER() = LOWER($)") + 1);
            sprintf(whereCondition, "NOT LOWER(%s) = LOWER($)", attributeName);
            return scope->where(whereCondition, values[0]);
          }
          // TODO: support LOWER
          return scope->whereIn(attributeName, false, values, count);
        }));

    resource->filter(
        attributeName, "eql",
        mmBlockCopy(
            memoryManager, ^(query_t *scope, const char **values, int count) {
              if (count == 1) {
                char *whereCondition =
                    mmMalloc(model->memoryManager,
                             strlen(attributeName) + strlen(" = $") + 1);
                sprintf(whereCondition, "%s = $", attributeName);
                return scope->where(whereCondition, values[0]);
              }
              return scope->whereIn(attributeName, true, values, count);
            }));

    resource->filter(
        attributeName, "not_eql",
        mmBlockCopy(
            memoryManager, ^(query_t *scope, const char **values, int count) {
              if (count == 1) {
                char *whereCondition =
                    mmMalloc(model->memoryManager,
                             strlen(attributeName) + strlen(" NOT = $") + 1);
                sprintf(whereCondition, "NOT %s = $", attributeName);
                return scope->where(whereCondition, values[0]);
              }
              return scope->whereIn(attributeName, false, values, count);
            }));

    resource->filter(
        attributeName, "match",
        mmBlockCopy(memoryManager, ^(query_t *scope, const char **values,
                                     UNUSED int count) {
          const char *value = values[0];
          char *whereCondition =
              mmMalloc(model->memoryManager,
                       strlen(attributeName) + strlen(" LIKE $") + 1);
          sprintf(whereCondition, "%s LIKE $", attributeName);
          size_t matchValueLen = strlen(value) + strlen("%%") + 1;
          char *matchValue = mmMalloc(model->memoryManager, matchValueLen);
          snprintf(matchValue, matchValueLen, "%%%s%%", value);
          return scope->where(whereCondition, matchValue);
        }));

    resource->filter(
        attributeName, "not_match",
        mmBlockCopy(memoryManager, ^(query_t *scope, const char **values,
                                     UNUSED int count) {
          const char *value = values[0];
          char *whereCondition =
              mmMalloc(model->memoryManager,
                       strlen(attributeName) + strlen(" NOT LIKE $") + 1);
          sprintf(whereCondition, "%s NOT LIKE $", attributeName);
          size_t matchValueLen = strlen(value) + strlen("%%") + 1;
          char *matchValue = mmMalloc(model->memoryManager, matchValueLen);
          snprintf(matchValue, matchValueLen, "%%%s%%", value);
          return scope->where(whereCondition, matchValue);
        }));

    resource->filter(
        attributeName, "prefix",
        mmBlockCopy(memoryManager, ^(query_t *scope, const char **values,
                                     UNUSED int count) {
          const char *value = values[0];
          char *whereCondition =
              mmMalloc(model->memoryManager,
                       strlen(attributeName) + strlen(" LIKE $") + 1);
          sprintf(whereCondition, "%s LIKE $", attributeName);
          size_t matchValueLen = strlen(value) + strlen("%") + 1;
          char *matchValue = mmMalloc(model->memoryManager, matchValueLen);
          snprintf(matchValue, matchValueLen, "%s%%", value);
          return scope->where(whereCondition, matchValue);
        }));

    resource->filter(
        attributeName, "not_prefix",
        mmBlockCopy(memoryManager, ^(query_t *scope, const char **values,
                                     UNUSED int count) {
          const char *value = values[0];
          char *whereCondition =
              mmMalloc(model->memoryManager,
                       strlen(attributeName) + strlen(" NOT LIKE $") + 1);
          sprintf(whereCondition, "%s NOT LIKE $", attributeName);
          size_t matchValueLen = strlen(value) + strlen("%") + 1;
          char *matchValue = mmMalloc(model->memoryManager, matchValueLen);
          snprintf(matchValue, matchValueLen, "%s%%", value);
          return scope->where(whereCondition, matchValue);
        }));

    resource->filter(
        attributeName, "suffix",
        mmBlockCopy(memoryManager, ^(query_t *scope, const char **values,
                                     UNUSED int count) {
          const char *value = values[0];
          char *whereCondition =
              mmMalloc(model->memoryManager,
                       strlen(attributeName) + strlen(" LIKE $") + 1);
          sprintf(whereCondition, "%s LIKE $", attributeName);
          size_t matchValueLen = strlen(value) + strlen("%") + 1;
          char *matchValue = mmMalloc(model->memoryManager, matchValueLen);
          snprintf(matchValue, matchValueLen, "%%%s", value);
          return scope->where(whereCondition, matchValue);
        }));

    resource->filter(
        attributeName, "not_suffix",
        mmBlockCopy(memoryManager, ^(query_t *scope, const char **values,
                                     UNUSED int count) {
          const char *value = values[0];
          char *whereCondition =
              mmMalloc(model->memoryManager,
                       strlen(attributeName) + strlen(" NOT LIKE $") + 1);
          sprintf(whereCondition, "%s NOT LIKE $", attributeName);
          size_t matchValueLen = strlen(value) + strlen("%") + 1;
          char *matchValue = mmMalloc(model->memoryManager, matchValueLen);
          snprintf(matchValue, matchValueLen, "%%%s", value);
          return scope->where(whereCondition, matchValue);
        }));
  }
}
