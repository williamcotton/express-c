CC = clang
BUILD_DIR = build

CFLAGS = -lexpress -Ofast

string-baseline:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/string-baseline blocks/string-baseline.c $(CFLAGS)
	time $(BUILD_DIR)/string-baseline

string-block:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/string-block blocks/string-block.c $(CFLAGS)
	time $(BUILD_DIR)/string-block

string-block-no-free:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/string-block-no-free blocks/string-block-no-free.c $(CFLAGS)
	time $(BUILD_DIR)/string-block-no-free

string-block-reset-reuse:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/string-block-reset-reuse blocks/string-block-reset-reuse.c $(CFLAGS)
	time $(BUILD_DIR)/string-block-reset-reuse

string-block-pool:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/string-block-pool blocks/string-block-pool.c $(CFLAGS)
	time $(BUILD_DIR)/string-block-pool

string-block-threaded:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/string-block-threaded blocks/string-block-threaded.c $(CFLAGS)
	time $(BUILD_DIR)/string-block-threaded

req-block-helpers:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/req-block-helpers blocks/req-block-helpers.c $(CFLAGS)
	$(BUILD_DIR)/req-block-helpers

req-baseline:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/req-baseline blocks/req-baseline.c $(CFLAGS)
	$(BUILD_DIR)/req-baseline
