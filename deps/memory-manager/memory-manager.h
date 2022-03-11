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

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Block.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>

typedef struct memory_manager_malloc_t {
  void *ptr;
} memory_manager_malloc_t;

typedef struct memory_manager_block_copy_t {
  void *ptr;
} memory_manager_block_copy_t;

typedef void (^memoryManagerCleanupHandler)();

typedef struct memory_manager_t {
  int mallocCount;
  memory_manager_malloc_t mallocs[1024];
  void * (^malloc)(size_t size);
  void * (^realloc)(void *ptr, size_t size);
  int blockCopyCount;
  memory_manager_block_copy_t blockCopies[1024];
  void * (^blockCopy)(void *);
  int cleanupHandlersCount;
  memoryManagerCleanupHandler cleanupHandlers[1024];
  void (^cleanup)(memoryManagerCleanupHandler);
  void (^free)();
} memory_manager_t;

memory_manager_t *createMemoryManager();

#endif // MEMORY_MANAGER_H
