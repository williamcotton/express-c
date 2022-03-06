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

#include "memory-manager.h"

memory_manager_t *createMemoryManager() {
  memory_manager_t *memoryManager = malloc(sizeof(memory_manager_t));
  memoryManager->mallocCount = 0;
  memoryManager->blockCopyCount = 0;
  memoryManager->cleanupHandlersCount = 0;

  memoryManager->malloc = Block_copy(^(size_t size) {
    void *ptr = malloc(size);
    memoryManager->mallocs[memoryManager->mallocCount++] =
        (memory_manager_malloc_t){.ptr = ptr};
    return ptr;
  });

  memoryManager->blockCopy = Block_copy(^(void *block) {
    void *ptr = Block_copy(block);
    memoryManager->blockCopies[memoryManager->blockCopyCount++] =
        (memory_manager_block_copy_t){.ptr = ptr};
    return ptr;
  });

  memoryManager->cleanup = Block_copy(^(memoryManagerCleanupHandler handler) {
    memoryManager->cleanupHandlers[memoryManager->cleanupHandlersCount++] =
        handler;
  });

  memoryManager->free = Block_copy(^() {
    for (int i = 0; i < memoryManager->mallocCount; i++) {
      free(memoryManager->mallocs[i].ptr);
    }

    for (int i = 0; i < memoryManager->blockCopyCount; i++) {
      Block_release(memoryManager->blockCopies[i].ptr);
    }

    for (int i = 0; i < memoryManager->cleanupHandlersCount; i++) {
      memoryManager->cleanupHandlers[i]();
    }

    Block_release(memoryManager->malloc);
    Block_release(memoryManager->blockCopy);
    Block_release(memoryManager->cleanup);

    dispatch_async(dispatch_get_main_queue(), ^() {
      Block_release(memoryManager->free);
      free(memoryManager);
    });
  });

  return memoryManager;
}