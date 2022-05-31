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
  int maxMallocCount;
  memory_manager_malloc_t *mallocs;
  int blockCopyCount;
  int maxBlockCopyCount;
  memory_manager_block_copy_t *blockCopies;
  int cleanupHandlersCount;
  int maxCleanupHandlersCount;
  memoryManagerCleanupHandler *cleanupHandlers;
  void * (^blockCopy)(void *);
  void * (^malloc)(size_t size);
  void * (^realloc)(void *ptr, size_t size);
  void (^cleanup)(memoryManagerCleanupHandler);
  void (^free)();
} memory_manager_t;

void *mmMalloc(memory_manager_t *memoryManager, size_t size);
void *mmRealloc(memory_manager_t *memoryManager, void *ptr, size_t size);
void *mmBlockCopy(memory_manager_t *memoryManager, void *block);
void mmCleanup(memory_manager_t *memoryManager,
               memoryManagerCleanupHandler handler);
void mmFree(memory_manager_t *memoryManager);

memory_manager_t *createMemoryManager();

#endif // MEMORY_MANAGER_H
