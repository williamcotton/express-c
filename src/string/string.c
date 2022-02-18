#include "string.h"
#include "../express.h"
#include <math.h>

char *stringErrorMessage(int error) {
  switch (error) {
  case 0:
    return NULL;
  case NUMBER_ERROR_NO_DIGITS:
    return "invalid (no digits found, 0 returned)";
  case NUMBER_ERROR_UNDERFLOW:
    return "invalid (underflow occurred)";
  case NUMBER_ERROR_OVERFLOW:
    return "invalid (overflow occurred)";
  case NUMBER_ERROR_BASE_UNSUPPORTED:
    return "invalid (base contains unsupported value)";
  case NUMBER_ERROR_UNSPECIFIED:
    return "invalid (unspecified error occurred)";
  case NUMBER_ERROR_ADDITIONAL_CHARACTERS:
    return "valid (additional characters found)";
  default:
    return "invalid (unspecified error occurred)";
  }
}

string_collection_t *stringCollection(size_t size, string_t **array) {
  string_collection_t *collection = malloc(sizeof(string_collection_t));
  collection->size = size;
  collection->arr = array;

  collection->mallocCount = 0;
  collection->malloc = Block_copy(^(size_t msize) {
    void *ptr = malloc(msize);
    collection->mallocs[collection->mallocCount++] = (malloc_t){.ptr = ptr};
    return ptr;
  });

  collection->blockCopyCount = 0;
  collection->blockCopy = Block_copy(^(void *block) {
    void *ptr = Block_copy(block);
    collection->blockCopies[collection->blockCopyCount++] =
        (malloc_t){.ptr = ptr};
    return ptr;
  });

  collection->each = collection->blockCopy(^(eachCallback callback) {
    for (size_t i = 0; i < collection->size; i++) {
      callback(collection->arr[i]);
    }
  });

  collection->eachWithIndex =
      collection->blockCopy(^(eachWithIndexCallback callback) {
        for (size_t i = 0; i < collection->size; i++) {
          callback(collection->arr[i], i);
        }
      });

  collection->reduce =
      collection->blockCopy(^(void *accumulator, reducerCallback reducer) {
        for (size_t i = 0; i < collection->size; i++) {
          accumulator = reducer(accumulator, collection->arr[i]);
        }
        return accumulator;
      });

  collection->map = collection->blockCopy(^(mapCallback callback) {
    void **arr = collection->malloc(sizeof(void *) * collection->size);
    for (size_t i = 0; i < collection->size; i++) {
      arr[i] = callback(collection->arr[i]);
    }
    return arr;
  });

  collection->reverse = collection->blockCopy(^(void) {
    for (size_t i = 0; i < collection->size / 2; i++) {
      struct string_t *tmp = collection->arr[i];
      collection->arr[i] = collection->arr[collection->size - i - 1];
      collection->arr[collection->size - i - 1] = tmp;
    }
  });

  collection->join = collection->blockCopy(^(const char *delim) {
    size_t len = 0;
    for (size_t i = 0; i < collection->size; i++) {
      len += collection->arr[i]->size;
    }
    len += collection->size - 1;
    char *str = malloc(len + 1);
    str[0] = '\0';
    for (size_t i = 0; i < collection->size; i++) {
      strcat(str, collection->arr[i]->value);
      if (i < collection->size - 1) {
        strcat(str, delim);
      }
    }
    string_t *temp = string(str);
    free(str);
    return temp;
  });

  collection->indexOf = collection->blockCopy(^(const char *str) {
    for (int i = 0; i < (int)collection->size; i++) {
      if (strcmp(collection->arr[i]->value, str) == 0) {
        return i;
      }
    }
    return -1;
  });

  collection->free = Block_copy(^(void) {
    for (size_t i = 0; i < collection->size; i++) {
      collection->arr[i]->free();
    }
    free(collection->arr);

    for (int i = 0; i < collection->mallocCount; i++) {
      free(collection->mallocs[i].ptr);
    }
    for (int i = 0; i < collection->blockCopyCount; i++) {
      Block_release(collection->blockCopies[i].ptr);
    }
    Block_release(collection->blockCopy);

    dispatch_async(dispatch_get_main_queue(), ^() {
      Block_release(collection->free);
      free(collection);
    });
  });

  return collection;
}

string_t *string(const char *strng) {
  string_t *s = malloc(sizeof(string_t));
  s->value = strdup(strng);
  s->size = strlen(s->value);

  s->blockCopyCount = 0;
  s->blockCopy = Block_copy(^(void *block) {
    void *ptr = Block_copy(block);
    s->blockCopies[s->blockCopyCount++] = (malloc_t){.ptr = ptr};
    return ptr;
  });

  s->print = s->blockCopy(^(void) {
    printf("%s\n", s->value);
  });

  s->concat = s->blockCopy(^(const char *str) {
    size_t size = s->size + strlen(str);
    char *new_str = malloc(size + 1);
    strcpy(new_str, s->value);
    strcat(new_str, str);
    free(s->value);
    s->value = new_str;
    s->size = size;
    return s;
  });

  s->upcase = s->blockCopy(^(void) {
    for (size_t i = 0; i < s->size; i++) {
      s->value[i] = toupper(s->value[i]);
    }
    return s;
  });

  s->downcase = s->blockCopy(^(void) {
    for (size_t i = 0; i < s->size; i++) {
      s->value[i] = tolower(s->value[i]);
    }
    return s;
  });

  s->capitalize = s->blockCopy(^(void) {
    for (size_t i = 0; i < s->size; i++) {
      if (i == 0) {
        s->value[i] = toupper(s->value[i]);
      } else {
        s->value[i] = tolower(s->value[i]);
      }
    }
    return s;
  });

  s->reverse = s->blockCopy(^(void) {
    char *new_str = malloc(s->size + 1);
    for (size_t i = 0; i < s->size; i++) {
      new_str[s->size - i - 1] = s->value[i];
    }
    new_str[s->size] = '\0';
    free(s->value);
    s->value = new_str;
    return s;
  });

  s->trim = s->blockCopy(^(void) {
    size_t start = 0;
    size_t end = s->size - 1;
    while (isspace(s->value[start])) {
      start++;
    }
    while (isspace(s->value[end])) {
      end--;
    }
    size_t size = end - start + 1;
    char *new_str = malloc(size + 1);
    strncpy(new_str, s->value + start, size);
    new_str[size] = '\0';
    free(s->value);
    s->value = new_str;
    s->size = size;
    return s;
  });

  s->split = s->blockCopy(^(const char *delim) {
    string_collection_t *collection = stringCollection(0, NULL);
    collection->size = 0;
    collection->arr = malloc(sizeof(string_t *));
    char *token = strtok(s->value, delim);
    while (token != NULL) {
      collection->size++;
      collection->arr =
          realloc(collection->arr, collection->size * sizeof(string_t *));
      collection->arr[collection->size - 1] = string(token);
      token = strtok(NULL, delim);
    }
    return collection;
  });

  s->toInt = s->blockCopy(^(void) {
    char *nptr = s->value;
    char *endptr = NULL;
    int error = 0;
    long long number;
    errno = 0;
    number = strtoll(nptr, &endptr, 10);
    if (nptr == endptr) {
      error = NUMBER_ERROR_NO_DIGITS;
    } else if (errno == ERANGE && number == LLONG_MIN) {
      error = NUMBER_ERROR_UNDERFLOW;
    } else if (errno == ERANGE && number == LLONG_MAX) {
      error = NUMBER_ERROR_OVERFLOW;
    } else if (errno == EINVAL) { /* not in all c99 implementations - gcc OK */
      error = NUMBER_ERROR_BASE_UNSUPPORTED;
    } else if (errno != 0 && number == 0) {
      error = NUMBER_ERROR_UNSPECIFIED;
    } else if (errno == 0 && nptr && *endptr != 0) {
      error = NUMBER_ERROR_ADDITIONAL_CHARACTERS;
    }
    integer_number_t n;
    if (error == 0 || error == NUMBER_ERROR_ADDITIONAL_CHARACTERS) {
      n.value = number;
    } else {
      n.value = 0;
    }
    n.error = error;
    return n;
  });

  s->toDecimal = s->blockCopy(^(void) {
    char *nptr = s->value;
    char *endptr = NULL;
    int error = 0;
    long double number;
    errno = 0;
    number = strtold(nptr, &endptr);
    if (nptr == endptr) {
      error = NUMBER_ERROR_NO_DIGITS;
    } else if (errno == ERANGE && number == -HUGE_VALL) {
      error = NUMBER_ERROR_UNDERFLOW;
    } else if (errno == ERANGE && number == HUGE_VALL) {
      error = NUMBER_ERROR_OVERFLOW;
    } else if (errno == EINVAL) { /* not in all c99 implementations - gcc OK */
      error = NUMBER_ERROR_BASE_UNSUPPORTED;
    } else if (errno != 0 && number == 0) {
      error = NUMBER_ERROR_UNSPECIFIED;
    } else if (errno == 0 && nptr && *endptr != 0) {
      error = NUMBER_ERROR_ADDITIONAL_CHARACTERS;
    }
    decimal_number_t n;
    if (error == 0 || error == NUMBER_ERROR_ADDITIONAL_CHARACTERS) {
      n.value = number;
    } else {
      n.value = 0;
    }
    n.error = error;
    return n;
  });

  s->replace = s->blockCopy(^(const char *str1, const char *str2) {
    char *new_str = malloc(s->size + 1);
    size_t i = 0;
    size_t j = 0;
    while (i < s->size) {
      if (strncmp(s->value + i, str1, strlen(str1)) == 0) {
        strcpy(new_str + j, str2);
        i += strlen(str1);
        j += strlen(str2);
      } else {
        new_str[j] = s->value[i];
        i++;
        j++;
      }
    }
    new_str[j] = '\0';
    free(s->value);
    s->value = new_str;
    s->size = j;
    return s;
  });

  s->chomp = s->blockCopy(^(void) {
    if (s->value[s->size - 1] == '\n') {
      s->value[s->size - 1] = '\0';
      s->size--;
    }
    return s;
  });

  s->slice = s->blockCopy(^(size_t start, size_t length) {
    if (start >= s->size) {
      return string("");
    }
    if (start + length > s->size) {
      length = s->size - start;
    }
    char *new_str = malloc(length + 1);
    strncpy(new_str, s->value + start, length);
    new_str[length] = '\0';
    string_t *temp = string(new_str);
    free(new_str);
    return temp;
  });

  s->indexOf = s->blockCopy(^(const char *str) {
    for (int i = 0; i < (int)s->size; i++) {
      if (strncmp(s->value + i, str, strlen(str)) == 0) {
        return i;
      }
    }
    return -1;
  });

  s->lastIndexOf = s->blockCopy(^(const char *str) {
    for (int i = s->size - 1; i > 0; i--) {
      if (strncmp(s->value + i, str, strlen(str)) == 0) {
        return i;
      }
    }
    return -1;
  });

  s->eql = s->blockCopy(^(const char *str) {
    return strcmp(s->value, str) == 0;
  });

  s->free = Block_copy(^(void) {
    free(s->value);
    for (int i = 0; i < s->blockCopyCount; i++) {
      Block_release(s->blockCopies[i].ptr);
    }
    Block_release(s->blockCopy);
    dispatch_async(dispatch_get_main_queue(), ^() {
      Block_release(s->free);
      free(s);
    });
  });
  return s;
}
