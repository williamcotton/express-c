#include "string.h"
#include "../express.h"

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
      strcat(str, collection->arr[i]->str);
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
      if (strcmp(collection->arr[i]->str, str) == 0) {
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
  s->str = strdup(strng);
  s->size = strlen(s->str);

  s->blockCopyCount = 0;
  s->blockCopy = Block_copy(^(void *block) {
    void *ptr = Block_copy(block);
    s->blockCopies[s->blockCopyCount++] = (malloc_t){.ptr = ptr};
    return ptr;
  });

  s->print = s->blockCopy(^(void) {
    printf("%s\n", s->str);
  });

  s->concat = s->blockCopy(^(const char *str) {
    size_t size = s->size + strlen(str);
    char *new_str = malloc(size + 1);
    strcpy(new_str, s->str);
    strcat(new_str, str);
    free(s->str);
    s->str = new_str;
    s->size = size;
    return s;
  });

  s->upcase = s->blockCopy(^(void) {
    for (size_t i = 0; i < s->size; i++) {
      s->str[i] = toupper(s->str[i]);
    }
    return s;
  });

  s->downcase = s->blockCopy(^(void) {
    for (size_t i = 0; i < s->size; i++) {
      s->str[i] = tolower(s->str[i]);
    }
    return s;
  });

  s->capitalize = s->blockCopy(^(void) {
    for (size_t i = 0; i < s->size; i++) {
      if (i == 0) {
        s->str[i] = toupper(s->str[i]);
      } else {
        s->str[i] = tolower(s->str[i]);
      }
    }
    return s;
  });

  s->reverse = s->blockCopy(^(void) {
    char *new_str = malloc(s->size + 1);
    for (size_t i = 0; i < s->size; i++) {
      new_str[s->size - i - 1] = s->str[i];
    }
    new_str[s->size] = '\0';
    free(s->str);
    s->str = new_str;
    return s;
  });

  s->trim = s->blockCopy(^(void) {
    size_t start = 0;
    size_t end = s->size - 1;
    while (isspace(s->str[start])) {
      start++;
    }
    while (isspace(s->str[end])) {
      end--;
    }
    size_t size = end - start + 1;
    char *new_str = malloc(size + 1);
    strncpy(new_str, s->str + start, size);
    new_str[size] = '\0';
    free(s->str);
    s->str = new_str;
    s->size = size;
    return s;
  });

  s->split = s->blockCopy(^(const char *delim) {
    string_collection_t *collection = stringCollection(0, NULL);
    collection->size = 0;
    collection->arr = malloc(sizeof(string_t *));
    char *token = strtok(s->str, delim);
    while (token != NULL) {
      collection->size++;
      collection->arr =
          realloc(collection->arr, collection->size * sizeof(string_t *));
      collection->arr[collection->size - 1] = string(token);
      token = strtok(NULL, delim);
    }
    return collection;
  });

  s->to_i = s->blockCopy(^(void) {
    return atoi(s->str);
  });

  s->replace = s->blockCopy(^(const char *str1, const char *str2) {
    char *new_str = malloc(s->size + 1);
    size_t i = 0;
    size_t j = 0;
    while (i < s->size) {
      if (strncmp(s->str + i, str1, strlen(str1)) == 0) {
        strcpy(new_str + j, str2);
        i += strlen(str1);
        j += strlen(str2);
      } else {
        new_str[j] = s->str[i];
        i++;
        j++;
      }
    }
    new_str[j] = '\0';
    free(s->str);
    s->str = new_str;
    s->size = j;
    return s;
  });

  s->chomp = s->blockCopy(^(void) {
    if (s->str[s->size - 1] == '\n') {
      s->str[s->size - 1] = '\0';
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
    strncpy(new_str, s->str + start, length);
    new_str[length] = '\0';
    string_t *temp = string(new_str);
    free(new_str);
    return temp;
  });

  s->indexOf = s->blockCopy(^(const char *str) {
    for (int i = 0; i < (int)s->size; i++) {
      if (strncmp(s->str + i, str, strlen(str)) == 0) {
        return i;
      }
    }
    return -1;
  });

  s->lastIndexOf = s->blockCopy(^(const char *str) {
    for (int i = s->size - 1; i > 0; i--) {
      if (strncmp(s->str + i, str, strlen(str)) == 0) {
        return i;
      }
    }
    return -1;
  });

  s->eql = s->blockCopy(^(const char *str) {
    return strcmp(s->str, str) == 0;
  });

  s->free = Block_copy(^(void) {
    free(s->str);
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
