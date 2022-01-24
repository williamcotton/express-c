ifneq (,$(wildcard ./.env))
	include .env
	export
endif

TARGETS := $(notdir $(patsubst %.c,%,$(wildcard demo/*.c)))
CFLAGS = $(shell cat compile_flags.txt | tr '\n' ' ')
CFLAGS += -DBUILD_ENV=$(BUILD_ENV)
DEV_CFLAGS = -g -O0
PROD_CFLAGS = -Ofast
TEST_CFLAGS = -Werror
SRC = src/express.c
SRC += $(wildcard deps/*/*.c) $(wildcard demo/*/*.c)
BUILD_DIR = build
PLATFORM := $(shell sh -c 'uname -s 2>/dev/null | tr 'a-z' 'A-Z'')

ifeq ($(PLATFORM),LINUX)
	CFLAGS += -lm -lBlocksRuntime -ldispatch -lbsd -luuid
else ifeq ($(PLATFORM),DARWIN)
	DEV_CFLAGS += -fsanitize=address,undefined
endif

all: $(TARGETS)

.PHONY: $(TARGETS)
$(TARGETS):
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/$@ demo/$@.c $(SRC) $(CFLAGS) $(DEV_CFLAGS)

$(TARGETS)-prod:
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/$(TARGETS) demo/$(TARGETS).c $(SRC) $(CFLAGS) $(PROD_CFLAGS)

.PHONY: test
test:
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/$@ test/test-app.c test/test-harnass.c test/tape.c $(SRC) $(CFLAGS) $(TEST_CFLAGS) $(DEV_CFLAGS)
	$(BUILD_DIR)/$@

clean:
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)

$(TARGETS)-watch: $(TARGETS) $(TARGETS)-run-background
	fswatch --event Updated deps/ src/ demo/$(TARGETS).c | xargs -n1 -I{} ./watch.sh $(TARGETS)

$(TARGETS)-run-background: $(TARGETS)-kill
	$(BUILD_DIR)/$(TARGETS) &

$(TARGETS)-kill:
	./kill.sh $(TARGETS)

test-watch:
	make --no-print-directory test || :
	fswatch --event Updated -o test/*.c test/*.h src/ | xargs -n1 -I{} make --no-print-directory test

build-test-trace:
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/test test/test-app.c test/test-harnass.c test/tape.c $(SRC) $(CFLAGS) -g -O0
ifeq ($(PLATFORM),DARWIN)
	codesign -s - -v -f --entitlements debug.plist $(BUILD_DIR)/test
endif

test-leaks: build-test-trace
ifeq ($(PLATFORM),LINUX)
	valgrind --tool=memcheck --leak-check=full --suppressions=express.supp --gen-suppressions=all --error-exitcode=1 --num-callers=30 -s $(BUILD_DIR)/test
else ifeq ($(PLATFORM),DARWIN)
	leaks --atExit -- $(BUILD_DIR)/test
endif

manual-test-trace: build-test-trace
	SLEEP_TIME=5 RUN_X_TIMES=10 $(BUILD_DIR)/test

.PHONY: test-tape
test-tape:
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/$@ test/test-tape.c test/tape.c $(SRC) $(CFLAGS)
	$(BUILD_DIR)/$@

$(TARGETS)-trace:
	clang -o $(BUILD_DIR)/$(TARGETS) demo/$(TARGETS).c $(SRC) $(CFLAGS) -g -O0
ifeq ($(PLATFORM),DARWIN)
	codesign -s - -v -f --entitlements debug.plist $(BUILD_DIR)/$(TARGETS)
endif

$(TARGETS)-analyze:
	clang --analyze demo/$(TARGETS).c $(SRC) $(CFLAGS) -Xclang -analyzer-output=text
