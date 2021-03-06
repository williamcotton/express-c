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

#include "postgres-middleware.h"
#include <Block.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <stdlib.h>

static void toUpper(char *givenStr) {
  int i;
  for (i = 0; givenStr[i] != '\0'; i++) {
    if (givenStr[i] >= 'a' && givenStr[i] <= 'z') {
      givenStr[i] = givenStr[i] - 32;
    }
  }
}

int pgParamCount(const char *query) {
  int count = 0;
  for (size_t i = 0; i < strlen(query); i++) {
    if (query[i] == '$') {
      count++;
    }
  }
  return count;
}

getPostgresQueryBlock getPostgresDBQuery(memory_manager_t *memoryManager,
                                         database_pool_t *db) {
  return mmBlockCopy(memoryManager, ^(const char *tableName) {
    query_t *query = mmMalloc(memoryManager, sizeof(query_t));

    query->paramValueCount = 0;
    query->whereConditionsCount = 0;
    query->selectConditionsCount = 0;
    query->limitCondition = "";
    query->offsetCondition = "";
    query->orderConditionsCount = 0;
    query->groupConditions = "";
    query->havingConditionsCount = 0;
    query->joinsConditions = "";
    query->distinctCondition = 0;

    query->select = mmBlockCopy(memoryManager, ^(const char *select) {
      query->selectConditions[query->selectConditionsCount++] = select;
      return query;
    });

    query->where = mmBlockCopy(memoryManager, ^(const char *conditions, ...) {
      int nParams = pgParamCount(conditions);

      char varNum[10];
      sprintf(varNum, "$%d", query->paramValueCount + 1);

      // TODO: replace with basic char * functions
      string_t *whereConditions = string(conditions);
      char *sequentialConditions =
          strdup(whereConditions->replace("$", varNum)->value);

      mmCleanup(memoryManager, whereConditions->free);

      query->whereConditions[query->whereConditionsCount++] =
          sequentialConditions;

      va_list args;
      va_start(args, conditions);

      for (int j = 0; j < nParams; j++) {
        query->paramValues[query->paramValueCount++] =
            va_arg(args, const char *);
      }

      va_end(args);

      return query;
    });

    query->whereIn =
        mmBlockCopy(memoryManager, ^(const char *column, int in,
                                     const char **values, int nValues) {
          char varNum[10];
          // TODO: replace with basic char * functions
          string_t *whereConditionsString = string("");
          whereConditionsString->concat(column);
          if (in) {
            whereConditionsString->concat(" IN (");
          } else {
            whereConditionsString->concat(" NOT IN (");
          }
          for (int i = 0; i < nValues; i++) {
            sprintf(varNum, "$%d", query->paramValueCount + 1);
            whereConditionsString->concat(varNum);
            if (i != nValues - 1) {
              whereConditionsString->concat(",");
            }
            query->paramValues[query->paramValueCount++] = values[i];
          }
          whereConditionsString->concat(")");
          query->whereConditions[query->whereConditionsCount++] =
              strdup(whereConditionsString->value);
          mmCleanup(memoryManager, whereConditionsString->free);
          return query;
        });

    query->limit = mmBlockCopy(memoryManager, ^(int limit) {
      char *limitStr = mmMalloc(memoryManager, sizeof(char) * 10);
      sprintf(limitStr, "%d", limit);
      query->limitCondition = limitStr;
      return query;
    });

    query->offset = mmBlockCopy(memoryManager, ^(int offset) {
      char *offsetStr = mmMalloc(memoryManager, sizeof(char) * 10);
      sprintf(offsetStr, "%d", offset);
      query->offsetCondition = offsetStr;
      return query;
    });

    query->order =
        mmBlockCopy(memoryManager, ^(const char *column, char *direction) {
          toUpper(direction);
          if (strcmp(direction, "ASC") != 0 && strcmp(direction, "DESC") != 0) {
            log_err("Invalid order direction: %s", direction);
            return query;
          }

          char *orderCondition =
              mmMalloc(memoryManager, strlen(column) + strlen(direction) + 2);
          sprintf(orderCondition, "%s %s", column, direction);
          query->orderConditions[query->orderConditionsCount++] =
              (char *)orderCondition;
          return query;
        });

    query->joins = mmBlockCopy(memoryManager, ^(const char *joinsCondition) {
      query->joinsConditions = (char *)joinsCondition;
      return query;
    });

    query->group = mmBlockCopy(memoryManager, ^(const char *groupCondition) {
      query->groupConditions = (char *)groupCondition;
      return query;
    });

    query->having = mmBlockCopy(memoryManager, ^(const char *conditions, ...) {
      int nParams = pgParamCount(conditions);

      char varNum[10];
      sprintf(varNum, "$%d", query->paramValueCount + 1);

      // TODO: replace with basic char * functions
      string_t *havingConditions = string(conditions);
      char *sequentialConditions =
          strdup(havingConditions->replace("$", varNum)->value);
      mmCleanup(memoryManager, havingConditions->free);

      query->havingConditions[query->havingConditionsCount++] =
          sequentialConditions;

      va_list args;
      va_start(args, conditions);

      for (int j = 0; j < nParams; j++) {
        query->paramValues[query->paramValueCount++] =
            va_arg(args, const char *);
      }

      va_end(args);

      return query;
    });

    query->distinct = mmBlockCopy(memoryManager, ^() {
      query->distinctCondition = 1;
      return query;
    });

    query->toSql = mmBlockCopy(memoryManager, ^() {
      // SELECT
      char *select = NULL;
      char *selectConditions = NULL;
      for (int i = 0; i < query->selectConditionsCount; i++) {
        size_t selectLen = strlen(query->selectConditions[i]) + 1;
        if (i == 0) {
          selectConditions = mmMalloc(memoryManager, selectLen);
          strlcpy(selectConditions, query->selectConditions[i], selectLen);
        } else {
          size_t selectsLen = strlen(selectConditions);
          selectConditions = mmRealloc(memoryManager, selectConditions,
                                       selectsLen + selectLen + 2);
          strlcat(selectConditions, ", ", selectsLen + 2);
          strlcat(selectConditions, query->selectConditions[i],
                  selectsLen + selectLen + 2);
        }
      }
      if (selectConditions == NULL) {
        selectConditions = "*";
      }
      if (query->distinctCondition) {
        select = mmMalloc(memoryManager, strlen(selectConditions) +
                                             strlen("SELECT DISTINCT") + 2);
        sprintf(select, "SELECT DISTINCT %s", selectConditions);
      } else {
        select = mmMalloc(memoryManager,
                          strlen(selectConditions) + strlen("SELECT ") + 1);
        sprintf(select, "SELECT %s", selectConditions);
      }

      // FROM
      char *from =
          mmMalloc(memoryManager, strlen(tableName) + strlen("FROM ") + 2);
      sprintf(from, " FROM %s", tableName);

      // JOINS
      char *joins = NULL;
      size_t joinsLen = strlen(query->joinsConditions);
      if (joinsLen > 0) {
        joins = mmMalloc(memoryManager, joinsLen + 2);
        sprintf(joins, " %s", query->joinsConditions);
      } else {
        joins = "";
      }

      // WHERE
      char *where = NULL;
      for (int i = 0; i < query->whereConditionsCount; i++) {
        if (where == NULL) {
          where = mmMalloc(memoryManager, strlen(query->whereConditions[i]) +
                                              strlen(" WHERE ") + 2);
          sprintf(where, " WHERE %s", query->whereConditions[i]);
        } else {
          char *tmp = mmMalloc(memoryManager,
                               strlen(where) + strlen(" AND ") +
                                   strlen(query->whereConditions[i]) + 1);
          sprintf(tmp, "%s AND %s", where, query->whereConditions[i]);
          where = tmp;
        }
      }
      if (where == NULL)
        where = "";

      // GROUP BY
      char *group = NULL;
      if (strlen(query->groupConditions) > 0) {
        group = mmMalloc(memoryManager, strlen(query->groupConditions) +
                                            strlen(" GROUP BY ") + 2);
        sprintf(group, " GROUP BY %s", query->groupConditions);
      }
      if (group == NULL)
        group = "";

      // HAVING
      char *having = NULL;
      for (int i = 0; i < query->havingConditionsCount; i++) {
        if (having == NULL) {
          having = mmMalloc(memoryManager, strlen(query->havingConditions[i]) +
                                               strlen(" HAVING ") + 2);
          sprintf(having, " HAVING %s", query->havingConditions[i]);
        } else {
          char *tmp = mmMalloc(memoryManager,
                               strlen(having) + strlen(" AND ") +
                                   strlen(query->havingConditions[i]) + 1);
          sprintf(tmp, "%s AND %s", having, query->havingConditions[i]);
          having = tmp;
        }
      }
      if (having == NULL)
        having = "";

      // LIMIT
      char *limit = NULL;
      if (strlen(query->limitCondition) > 0) {
        limit = mmMalloc(memoryManager,
                         strlen(query->limitCondition) + strlen(" LIMIT ") + 2);
        sprintf(limit, " LIMIT %s", query->limitCondition);
      }
      if (limit == NULL)
        limit = "";

      // OFFSET
      char *offset = NULL;
      if (strlen(query->offsetCondition) > 0) {
        offset = mmMalloc(memoryManager, strlen(query->offsetCondition) +
                                             strlen(" OFFSET ") + 2);
        sprintf(offset, " OFFSET %s", query->offsetCondition);
      }
      if (offset == NULL)
        offset = "";

      // ORDER BY
      char *order = NULL;
      for (int i = 0; i < query->orderConditionsCount; i++) {
        if (order == NULL) {
          order = mmMalloc(memoryManager, strlen(query->orderConditions[i]) +
                                              strlen(" ORDER BY ") + 2);
          sprintf(order, " ORDER BY %s", query->orderConditions[i]);
        } else {
          char *tmp = mmMalloc(memoryManager,
                               strlen(order) + strlen(", ") +
                                   strlen(query->orderConditions[i]) + 1);
          sprintf(tmp, "%s, %s", order, query->orderConditions[i]);
          order = tmp;
        }
      }
      if (order == NULL)
        order = "";

      char *sql =
          mmMalloc(memoryManager,
                   strlen(select) + strlen(from) + strlen(joins) +
                       strlen(where) + strlen(limit) + strlen(offset) +
                       strlen(order) + strlen(group) + strlen(having) + 2);
      sprintf(sql, "%s%s%s%s%s%s%s%s%s", select, from, joins, where, group,
              having, limit, offset, order);

      mmCleanup(memoryManager, mmBlockCopy(memoryManager, ^{
                  for (int i = 0; i < query->whereConditionsCount; i++) {
                    free(query->whereConditions[i]);
                  }
                  query->whereConditionsCount = 0;
                  for (int i = 0; i < query->havingConditionsCount; i++) {
                    free(query->havingConditions[i]);
                  }
                  query->havingConditionsCount = 0;
                }));

      query->sql = sql;

      // debug("\n==SQL: %s", sql);

      return sql;
    });

    query->all = mmBlockCopy(memoryManager, ^() {
      return db->execParams(query->toSql(), query->paramValueCount, NULL,
                            query->paramValues, NULL, NULL, 0);
    });

    query->find = mmBlockCopy(memoryManager, ^(char *id) {
      query->where("id = $", id);
      return db->execParams(query->toSql(), query->paramValueCount, NULL,
                            query->paramValues, NULL, NULL, 0);
    });

    query->count = mmBlockCopy(memoryManager, ^() {
      query->selectConditions[0] = "count(*)";
      query->selectConditionsCount = 1;
      PGresult *pgres = db->execParams(query->toSql(), query->paramValueCount,
                                       NULL, query->paramValues, NULL, NULL, 0);
      if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        log_err("%s", PQresultErrorMessage(pgres));
        return -1;
      }
      int count = atoi(PQgetvalue(pgres, 0, 0));
      PQclear(pgres);
      query->selectConditionsCount = 0;
      return count;
    });

    query->stat = mmBlockCopy(memoryManager, ^(char *attribute, char *stat) {
      query_stat_result_t *result =
          mmMalloc(memoryManager, sizeof(query_stat_result_t));
      result->value = NULL;
      result->stat = stat;
      result->type = NULL;

      if (strcmp(stat, "min") == 0) {
        query->selectConditions[0] =
            mmMalloc(memoryManager,
                     strlen("min(") + strlen(attribute) + strlen(")") + 1);
        sprintf((char *)query->selectConditions[0], "min(%s)", attribute);
        query->selectConditionsCount = 1;
      }
      if (strcmp(stat, "max") == 0) {
        query->selectConditions[0] =
            mmMalloc(memoryManager,
                     strlen("max(") + strlen(attribute) + strlen(")") + 1);
        sprintf((char *)query->selectConditions[0], "max(%s)", attribute);
        query->selectConditionsCount = 1;
      }
      if (strcmp(stat, "average") == 0) {
        query->selectConditions[0] =
            mmMalloc(memoryManager,
                     strlen("avg(") + strlen(attribute) + strlen(")") + 1);
        sprintf((char *)query->selectConditions[0], "avg(%s)", attribute);
        query->selectConditionsCount = 1;
      }
      if (strcmp(stat, "sum") == 0) {
        query->selectConditions[0] =
            mmMalloc(memoryManager,
                     strlen("sum(") + strlen(attribute) + strlen(")") + 1);
        sprintf((char *)query->selectConditions[0], "sum(%s)", attribute);
        query->selectConditionsCount = 1;
      }

      PGresult *pgres = db->execParams(query->toSql(), query->paramValueCount,
                                       NULL, query->paramValues, NULL, NULL, 0);
      if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        log_err("%s", PQresultErrorMessage(pgres));
        return result;
      }
      char *value = PQgetvalue(pgres, 0, 0);

      query->selectConditionsCount = 0;
      result->value = strdup(value);

      PQclear(pgres);
      return result;
    });

    return query;
  });
}

getPostgresQueryBlock getPostgresQuery(memory_manager_t *memoryManager,
                                       pg_t *pg) {
  return mmBlockCopy(memoryManager, ^(const char *tableName) {
    query_t *query = mmMalloc(memoryManager, sizeof(query_t));

    query->paramValueCount = 0;
    query->whereConditionsCount = 0;
    query->selectConditionsCount = 0;
    query->limitCondition = "";
    query->offsetCondition = "";
    query->orderConditionsCount = 0;
    query->groupConditions = "";
    query->havingConditionsCount = 0;
    query->joinsConditions = "";
    query->distinctCondition = 0;

    query->select = mmBlockCopy(memoryManager, ^(const char *select) {
      query->selectConditions[query->selectConditionsCount++] = select;
      return query;
    });

    query->where = mmBlockCopy(memoryManager, ^(const char *conditions, ...) {
      int nParams = pgParamCount(conditions);

      char varNum[10];
      sprintf(varNum, "$%d", query->paramValueCount + 1);

      // TODO: replace with basic char * functions
      string_t *whereConditions = string(conditions);
      char *sequentialConditions =
          strdup(whereConditions->replace("$", varNum)->value);

      mmCleanup(memoryManager, whereConditions->free);

      query->whereConditions[query->whereConditionsCount++] =
          sequentialConditions;

      va_list args;
      va_start(args, conditions);

      for (int j = 0; j < nParams; j++) {
        query->paramValues[query->paramValueCount++] =
            va_arg(args, const char *);
      }

      va_end(args);

      return query;
    });

    query->whereIn =
        mmBlockCopy(memoryManager, ^(const char *column, int in,
                                     const char **values, int nValues) {
          char varNum[10];
          // TODO: replace with basic char * functions
          string_t *whereConditionsString = string("");
          whereConditionsString->concat(column);
          if (in) {
            whereConditionsString->concat(" IN (");
          } else {
            whereConditionsString->concat(" NOT IN (");
          }
          for (int i = 0; i < nValues; i++) {
            sprintf(varNum, "$%d", query->paramValueCount + 1);
            whereConditionsString->concat(varNum);
            if (i != nValues - 1) {
              whereConditionsString->concat(",");
            }
            query->paramValues[query->paramValueCount++] = values[i];
          }
          whereConditionsString->concat(")");
          query->whereConditions[query->whereConditionsCount++] =
              strdup(whereConditionsString->value);
          mmCleanup(memoryManager, whereConditionsString->free);
          return query;
        });

    query->limit = mmBlockCopy(memoryManager, ^(int limit) {
      char *limitStr = mmMalloc(memoryManager, sizeof(char) * 10);
      sprintf(limitStr, "%d", limit);
      query->limitCondition = limitStr;
      return query;
    });

    query->offset = mmBlockCopy(memoryManager, ^(int offset) {
      char *offsetStr = mmMalloc(memoryManager, sizeof(char) * 10);
      sprintf(offsetStr, "%d", offset);
      query->offsetCondition = offsetStr;
      return query;
    });

    query->order =
        mmBlockCopy(memoryManager, ^(const char *column, char *direction) {
          toUpper(direction);
          if (strcmp(direction, "ASC") != 0 && strcmp(direction, "DESC") != 0) {
            log_err("Invalid order direction: %s", direction);
            return query;
          }

          char *orderCondition =
              mmMalloc(memoryManager, strlen(column) + strlen(direction) + 2);
          sprintf(orderCondition, "%s %s", column, direction);
          query->orderConditions[query->orderConditionsCount++] =
              (char *)orderCondition;
          return query;
        });

    query->joins = mmBlockCopy(memoryManager, ^(const char *joinsCondition) {
      query->joinsConditions = (char *)joinsCondition;
      return query;
    });

    query->group = mmBlockCopy(memoryManager, ^(const char *groupCondition) {
      query->groupConditions = (char *)groupCondition;
      return query;
    });

    query->having = mmBlockCopy(memoryManager, ^(const char *conditions, ...) {
      int nParams = pgParamCount(conditions);

      char varNum[10];
      sprintf(varNum, "$%d", query->paramValueCount + 1);

      // TODO: replace with basic char * functions
      string_t *havingConditions = string(conditions);
      char *sequentialConditions =
          strdup(havingConditions->replace("$", varNum)->value);
      mmCleanup(memoryManager, havingConditions->free);

      query->havingConditions[query->havingConditionsCount++] =
          sequentialConditions;

      va_list args;
      va_start(args, conditions);

      for (int j = 0; j < nParams; j++) {
        query->paramValues[query->paramValueCount++] =
            va_arg(args, const char *);
      }

      va_end(args);

      return query;
    });

    query->distinct = mmBlockCopy(memoryManager, ^() {
      query->distinctCondition = 1;
      return query;
    });

    query->toSql = mmBlockCopy(memoryManager, ^() {
      // SELECT
      char *select = NULL;
      char *selectConditions = NULL;
      for (int i = 0; i < query->selectConditionsCount; i++) {
        size_t selectLen = strlen(query->selectConditions[i]) + 1;
        if (i == 0) {
          selectConditions = mmMalloc(memoryManager, selectLen);
          strlcpy(selectConditions, query->selectConditions[i], selectLen);
        } else {
          size_t selectsLen = strlen(selectConditions);
          selectConditions = mmRealloc(memoryManager, selectConditions,
                                       selectsLen + selectLen + 2);
          strlcat(selectConditions, ", ", selectsLen + 2);
          strlcat(selectConditions, query->selectConditions[i],
                  selectsLen + selectLen + 2);
        }
      }
      if (selectConditions == NULL) {
        selectConditions = "*";
      }
      if (query->distinctCondition) {
        select = mmMalloc(memoryManager, strlen(selectConditions) +
                                             strlen("SELECT DISTINCT") + 2);
        sprintf(select, "SELECT DISTINCT %s", selectConditions);
      } else {
        select = mmMalloc(memoryManager,
                          strlen(selectConditions) + strlen("SELECT ") + 1);
        sprintf(select, "SELECT %s", selectConditions);
      }

      // FROM
      char *from =
          mmMalloc(memoryManager, strlen(tableName) + strlen("FROM ") + 2);
      sprintf(from, " FROM %s", tableName);

      // JOINS
      char *joins = NULL;
      size_t joinsLen = strlen(query->joinsConditions);
      if (joinsLen > 0) {
        joins = mmMalloc(memoryManager, joinsLen + 2);
        sprintf(joins, " %s", query->joinsConditions);
      } else {
        joins = "";
      }

      // WHERE
      char *where = NULL;
      for (int i = 0; i < query->whereConditionsCount; i++) {
        if (where == NULL) {
          where = mmMalloc(memoryManager, strlen(query->whereConditions[i]) +
                                              strlen(" WHERE ") + 2);
          sprintf(where, " WHERE %s", query->whereConditions[i]);
        } else {
          char *tmp = mmMalloc(memoryManager,
                               strlen(where) + strlen(" AND ") +
                                   strlen(query->whereConditions[i]) + 1);
          sprintf(tmp, "%s AND %s", where, query->whereConditions[i]);
          where = tmp;
        }
      }
      if (where == NULL)
        where = "";

      // GROUP BY
      char *group = NULL;
      if (strlen(query->groupConditions) > 0) {
        group = mmMalloc(memoryManager, strlen(query->groupConditions) +
                                            strlen(" GROUP BY ") + 2);
        sprintf(group, " GROUP BY %s", query->groupConditions);
      }
      if (group == NULL)
        group = "";

      // HAVING
      char *having = NULL;
      for (int i = 0; i < query->havingConditionsCount; i++) {
        if (having == NULL) {
          having = mmMalloc(memoryManager, strlen(query->havingConditions[i]) +
                                               strlen(" HAVING ") + 2);
          sprintf(having, " HAVING %s", query->havingConditions[i]);
        } else {
          char *tmp = mmMalloc(memoryManager,
                               strlen(having) + strlen(" AND ") +
                                   strlen(query->havingConditions[i]) + 1);
          sprintf(tmp, "%s AND %s", having, query->havingConditions[i]);
          having = tmp;
        }
      }
      if (having == NULL)
        having = "";

      // LIMIT
      char *limit = NULL;
      if (strlen(query->limitCondition) > 0) {
        limit = mmMalloc(memoryManager,
                         strlen(query->limitCondition) + strlen(" LIMIT ") + 2);
        sprintf(limit, " LIMIT %s", query->limitCondition);
      }
      if (limit == NULL)
        limit = "";

      // OFFSET
      char *offset = NULL;
      if (strlen(query->offsetCondition) > 0) {
        offset = mmMalloc(memoryManager, strlen(query->offsetCondition) +
                                             strlen(" OFFSET ") + 2);
        sprintf(offset, " OFFSET %s", query->offsetCondition);
      }
      if (offset == NULL)
        offset = "";

      // ORDER BY
      char *order = NULL;
      for (int i = 0; i < query->orderConditionsCount; i++) {
        if (order == NULL) {
          order = mmMalloc(memoryManager, strlen(query->orderConditions[i]) +
                                              strlen(" ORDER BY ") + 2);
          sprintf(order, " ORDER BY %s", query->orderConditions[i]);
        } else {
          char *tmp = mmMalloc(memoryManager,
                               strlen(order) + strlen(", ") +
                                   strlen(query->orderConditions[i]) + 1);
          sprintf(tmp, "%s, %s", order, query->orderConditions[i]);
          order = tmp;
        }
      }
      if (order == NULL)
        order = "";

      char *sql =
          mmMalloc(memoryManager,
                   strlen(select) + strlen(from) + strlen(joins) +
                       strlen(where) + strlen(limit) + strlen(offset) +
                       strlen(order) + strlen(group) + strlen(having) + 2);
      sprintf(sql, "%s%s%s%s%s%s%s%s%s", select, from, joins, where, group,
              having, limit, offset, order);

      mmCleanup(memoryManager, mmBlockCopy(memoryManager, ^{
                  for (int i = 0; i < query->whereConditionsCount; i++) {
                    free(query->whereConditions[i]);
                  }
                  query->whereConditionsCount = 0;
                  for (int i = 0; i < query->havingConditionsCount; i++) {
                    free(query->havingConditions[i]);
                  }
                  query->havingConditionsCount = 0;
                }));

      query->sql = sql;

      // debug("\n==SQL: %s", sql);

      return sql;
    });

    query->all = mmBlockCopy(memoryManager, ^() {
      return pg->execParams(query->toSql(), query->paramValueCount, NULL,
                            query->paramValues, NULL, NULL, 0);
    });

    query->find = mmBlockCopy(memoryManager, ^(char *id) {
      query->where("id = $", id);
      return pg->execParams(query->toSql(), query->paramValueCount, NULL,
                            query->paramValues, NULL, NULL, 0);
    });

    query->count = mmBlockCopy(memoryManager, ^() {
      query->selectConditions[0] = "count(*)";
      query->selectConditionsCount = 1;
      PGresult *pgres = pg->execParams(query->toSql(), query->paramValueCount,
                                       NULL, query->paramValues, NULL, NULL, 0);
      if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        log_err("%s", PQresultErrorMessage(pgres));
        return -1;
      }
      int count = atoi(PQgetvalue(pgres, 0, 0));
      PQclear(pgres);
      query->selectConditionsCount = 0;
      return count;
    });

    query->stat = mmBlockCopy(memoryManager, ^(char *attribute, char *stat) {
      query_stat_result_t *result =
          mmMalloc(memoryManager, sizeof(query_stat_result_t));
      result->value = NULL;
      result->stat = stat;
      result->type = NULL;

      if (strcmp(stat, "min") == 0) {
        query->selectConditions[0] =
            mmMalloc(memoryManager,
                     strlen("min(") + strlen(attribute) + strlen(")") + 1);
        sprintf((char *)query->selectConditions[0], "min(%s)", attribute);
        query->selectConditionsCount = 1;
      }
      if (strcmp(stat, "max") == 0) {
        query->selectConditions[0] =
            mmMalloc(memoryManager,
                     strlen("max(") + strlen(attribute) + strlen(")") + 1);
        sprintf((char *)query->selectConditions[0], "max(%s)", attribute);
        query->selectConditionsCount = 1;
      }
      if (strcmp(stat, "average") == 0) {
        query->selectConditions[0] =
            mmMalloc(memoryManager,
                     strlen("avg(") + strlen(attribute) + strlen(")") + 1);
        sprintf((char *)query->selectConditions[0], "avg(%s)", attribute);
        query->selectConditionsCount = 1;
      }
      if (strcmp(stat, "sum") == 0) {
        query->selectConditions[0] =
            mmMalloc(memoryManager,
                     strlen("sum(") + strlen(attribute) + strlen(")") + 1);
        sprintf((char *)query->selectConditions[0], "sum(%s)", attribute);
        query->selectConditionsCount = 1;
      }

      PGresult *pgres = pg->execParams(query->toSql(), query->paramValueCount,
                                       NULL, query->paramValues, NULL, NULL, 0);
      if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        log_err("%s", PQresultErrorMessage(pgres));
        return result;
      }
      char *value = PQgetvalue(pgres, 0, 0);

      query->selectConditionsCount = 0;
      result->value = strdup(value);

      PQclear(pgres);
      return result;
    });

    return query;
  });
}

pg_t *initPg(const char *pgUri) {
  pg_t *pg = malloc(sizeof(pg_t));
  pg->connection = PQconnectdb(pgUri);
  pg->used = 0;

  pg->exec = Block_copy(^(const char *sql, ...) {
    int nParams = pgParamCount(sql);
    va_list args;
    va_start(args, sql);

    const char **paramValues =
        nParams == 0 ? NULL : malloc(sizeof(char *) * nParams);
    for (int i = 0; i < nParams; i++) {
      paramValues[i] = va_arg(args, const char *);
      if (i > 1024) {
        log_err("Too many parameters");
        return (PGresult *)NULL;
      }
    }
    va_end(args);
    PGresult *pgres = PQexecParams(pg->connection, sql, nParams, NULL,
                                   paramValues, NULL, NULL, 0);
    free(paramValues);
    return pgres;
  });

  pg->execParams =
      Block_copy(^(const char *sql, int nParams, const Oid *paramTypes,
                   const char *const *paramValues, const int *paramLengths,
                   const int *paramFormats, int resultFormat) {
        PGresult *pgres =
            PQexecParams(pg->connection, sql, nParams, paramTypes, paramValues,
                         paramLengths, paramFormats, resultFormat);
        return pgres;
      });

  pg->close = Block_copy(^{
    PQfinish(pg->connection);
  });

  pg->free = Block_copy(^{
    Block_release(pg->exec);
    Block_release(pg->execParams);
    Block_release(pg->close);
    dispatch_async(dispatch_get_main_queue(), ^() {
      Block_release(pg->free);
      free(pg);
    });
  });

  return pg;
}

postgres_connection_t *initPostgressConnection(const char *pgUri,
                                               int poolSize) {
  postgres_connection_t *postgres = malloc(sizeof(postgres_connection_t));
  check(postgres, "Failed to allocate postgres connection pool");

  postgres->uri = pgUri;

  postgres->poolSize = poolSize;

  postgres->pool = malloc(sizeof(pg_t *) * postgres->poolSize);

  postgres->semaphore = dispatch_semaphore_create(postgres->poolSize);

  postgres->queue = dispatch_queue_create("postgres", NULL);

  for (int i = 0; i < postgres->poolSize; i++) {
    postgres->pool[i] = initPg(postgres->uri);
  }

  postgres->free = Block_copy(^() {
    for (int i = 0; i < postgres->poolSize; i++) {
      postgres->pool[i]->free();
    }
    free(postgres->pool);
    dispatch_release(postgres->semaphore);
    dispatch_release(postgres->queue);
    dispatch_async(dispatch_get_main_queue(), ^() {
      Block_release(postgres->free);
      free(postgres);
    });
  });

  return postgres;
error:
  return NULL;
}

middlewareHandler postgresMiddlewareFactory(postgres_connection_t *postgres) {
  /* Test connection */
  postgres->connection = PQconnectdb(postgres->uri);
  check(PQstatus(postgres->connection) == CONNECTION_OK,
        "Failed to connect to postgres: %s (%s)",
        PQerrorMessage(postgres->connection), postgres->uri);

  /* Create middleware, getting a postgress connection at the beginning of
   * every request and releasing it after the request has finished */
  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    /* Wait for a connection */
    dispatch_semaphore_wait(postgres->semaphore, DISPATCH_TIME_FOREVER);

    __block pg_t *pg = NULL;

    /* Get connection */
    dispatch_sync(postgres->queue, ^{
      for (int i = 0; i < postgres->poolSize; i++) {
        if (postgres->pool[i]->used == 0) {
          postgres->pool[i]->used = 1;
          pg = postgres->pool[i];
          break;
        }
      }
    });

    if (PQstatus(pg->connection) != CONNECTION_OK) {
      PQreset(pg->connection);
    }

    pg->query = getPostgresQuery(req->memoryManager, pg);

    req->mSet("pg", pg);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq) {
      /* Release connection */
      dispatch_sync(postgres->queue, ^{
        pg->used = 0;
      });

      /* Signal a release */
      dispatch_semaphore_signal(postgres->semaphore);
    }));

    next();
  });

error:

  return Block_copy(^(UNUSED request_t *req, UNUSED response_t *res,
                      void (^next)(), void (^cleanup)(cleanupHandler)) {
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
        // PQfinish(postgres->connection);
    }));
    next();
  });
}
