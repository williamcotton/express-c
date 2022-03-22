ifneq (,$(wildcard ./.env))
	include .env
	export
endif

PLATFORM := $(shell sh -c 'uname -s 2>/dev/null | tr 'a-z' 'A-Z'')

CC = clang
PROFDATA = llvm-profdata
COV = llvm-cov
TIDY = clang-tidy
FORMAT = clang-format

ifeq ($(PLATFORM),DARWIN)
	CC = $(shell brew --prefix llvm)/bin/clang
	PROFDATA = $(shell brew --prefix llvm)/bin/profdata
	COV = $(shell brew --prefix llvm)/bin/llvm-cov
	TIDY = $(shell brew --prefix llvm)/bin/clang-tidy
	FORMAT = $(shell brew --prefix llvm)/bin/clang-format
endif

CFLAGS = $(shell cat compile_flags.txt | tr '\n' ' ')

CFLAGS += -DBUILD_ENV=$(BUILD_ENV) -lcurl $(shell pkg-config --libs libpq libjwt jansson) -I$(shell pg_config --includedir) -ljansson -ljwt
EXPRESS_SRC = $(wildcard ../src/*/*.c) $(wildcard ../src/*.c) $(wildcard ../deps/*/*.c) $(wildcard ../deps/*.c)
SRC = $(EXPRESS_SRC) $(wildcard */*.c) $(wildcard */*/*.c)

# CFLAGS += -DBUILD_ENV=$(BUILD_ENV) -lcurl $(shell pkg-config --libs libpq) -I$(shell pg_config --includedir) -I/usr/local/include -L/usr/local/lib -lexpress
# SRC = $(wildcard */*.c) $(wildcard */*/*.c)

TARGETS := $(notdir $(patsubst %.c,%,$(wildcard *.c)))
DEV_CFLAGS = -g -O0
TEST_CFLAGS = -Werror
TEST_SRC = $(wildcard test/*.c) $(wildcard test/*/*.c)mak
BUILD_DIR = build

ifeq ($(PLATFORM),LINUX)
	CFLAGS += -lm -lBlocksRuntime -ldispatch -lbsd -luuid -lpthread
	TEST_CFLAGS += -Wl,--wrap=stat -Wl,--wrap=regcomp -Wl,--wrap=accept -Wl,--wrap=socket -Wl,--wrap=epoll_ctl, -Wl,--wrap=listen
	PROD_CFLAGS = -Ofast
else ifeq ($(PLATFORM),DARWIN)
	DEV_CFLAGS += -fsanitize=address,undefined,implicit-conversion,float-divide-by-zero,local-bounds,nullability
	PROD_CFLAGS = -Ofast
endif

all: $(TARGETS)

.PHONY: $(TARGETS)
$(TARGETS):
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/$@ $@.c $(SRC) $(CFLAGS) $(DEV_CFLAGS) -DERR_STACKTRACE

$(TARGETS)-prod: embeddedFiles.h
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/$(TARGETS) $(TARGETS).c $(SRC) $(CFLAGS) $(PROD_CFLAGS) -DEMBEDDED_FILES=1

.PHONY: test
test: test-database-create
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/$@ $(TEST_SRC) $(SRC) $(CFLAGS) $(TEST_CFLAGS) $(DEV_CFLAGS)
	$(BUILD_DIR)/$@

test-database-create:
	-dbmate -e TEST_DATABASE_URL create

test-coverage-output:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/coverage
	$(CC) -o $(BUILD_DIR)/$@ $(TEST_SRC) $(SRC) $(CFLAGS) $(TEST_CFLAGS) $(DEV_CFLAGS) -fprofile-instr-generate -fcoverage-mapping
	LLVM_PROFILE_FILE="build/test.profraw" $(BUILD_DIR)/$@
	$(PROFDATA) merge -sparse build/test.profraw -o build/test.profdata
	$(COV) show $(BUILD_DIR)/$@ -instr-profile=$(BUILD_DIR)/test.profdata

test-coverage-html:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/coverage
	$(CC) -o $(BUILD_DIR)/$@ $(TEST_SRC) $(SRC) $(CFLAGS) $(TEST_CFLAGS) $(DEV_CFLAGS) -fprofile-instr-generate -fcoverage-mapping
	LLVM_PROFILE_FILE="build/test.profraw" $(BUILD_DIR)/$@
	$(PROFDATA) merge -sparse build/test.profraw -o build/test.profdata
	$(COV) show $(BUILD_DIR)/$@ -instr-profile=$(BUILD_DIR)/test.profdata -format=html > $(BUILD_DIR)/code-coverage.html

test-coverage:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/coverage
	$(CC) -o $(BUILD_DIR)/$@ $(TEST_SRC) $(SRC) $(CFLAGS) $(TEST_CFLAGS) $(DEV_CFLAGS) -fprofile-instr-generate -fcoverage-mapping
	LLVM_PROFILE_FILE="build/test.profraw" $(BUILD_DIR)/$@
	$(PROFDATA) merge -sparse build/test.profraw -o build/test.profdata
	$(COV) report $(BUILD_DIR)/$@ -instr-profile=$(BUILD_DIR)/test.profdata -ignore-filename-regex="/deps|demo|test/"

lint:
ifeq ($(PLATFORM),LINUX)
	$(TIDY) --checks=-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling -warnings-as-errors=* src/express.c
else ifeq ($(PLATFORM),DARWIN)
	$(TIDY) -warnings-as-errors=* src/express.c
endif

format:
	$(FORMAT) --dry-run --Werror $(SRC)

clean:
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)

$(TARGETS)-watch: $(TARGETS) $(TARGETS)-run-background
	fswatch --event Updated TodoMVC.c .env models/ controllers/ | xargs -n1 -I{} watch.sh $(TARGETS)

$(TARGETS)-run-background: $(TARGETS)-kill
	$(BUILD_DIR)/$(TARGETS) &

$(TARGETS)-kill:
	kill.sh $(TARGETS)

test-watch:
	make --no-print-directory test || :
	fswatch --event Updated -o test/*.c test/*.h src/ | xargs -n1 -I{} make --no-print-directory test

build-test-trace:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/test $(TEST_SRC) $(SRC) $(TEST_CFLAGS) $(CFLAGS) -g -O0
ifeq ($(PLATFORM),DARWIN)
	codesign -s - -v -f --entitlements debug.plist $(BUILD_DIR)/test
endif

test-leaks: build-test-trace test-database-create
ifeq ($(PLATFORM),LINUX)
	valgrind --tool=memcheck --leak-check=full --suppressions=express.supp --gen-suppressions=all --error-exitcode=1 --num-callers=30 -s $(BUILD_DIR)/test
else ifeq ($(PLATFORM),DARWIN)
	leaks --atExit -- $(BUILD_DIR)/test
endif

test-threads:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/$@ $(TEST_SRC) $(SRC) $(CFLAGS) $(TEST_CFLAGS) -fsanitize=thread
	$(BUILD_DIR)/$@

manual-test-trace: build-test-trace
	SLEEP_TIME=5 RUN_X_TIMES=10 $(BUILD_DIR)/test

$(TARGETS)-trace:
	$(CC) -o $(BUILD_DIR)/$(TARGETS) $(TARGETS).c $(SRC) $(CFLAGS) -g -O0
ifeq ($(PLATFORM),DARWIN)
	codesign -s - -v -f --entitlements debug.plist $(BUILD_DIR)/$(TARGETS)
endif

$(TARGETS)-prod-trace: embeddedFiles.h
	$(CC) -o $(BUILD_DIR)/$(TARGETS) $(TARGETS).c $(SRC) $(CFLAGS) $(PROD_CFLAGS) -DEMBEDDED_FILES=1
ifeq ($(PLATFORM),DARWIN)
	codesign -s - -v -f --entitlements debug.plist $(BUILD_DIR)/$(TARGETS)
endif

$(TARGETS)-analyze:
	$(CC) --analyze $(TARGETS).c $(SRC) $(CFLAGS) -Xclang -analyzer-output=text

.PHONY: embeddedFiles.h
embeddedFiles.h:
	embed.sh $@ >> $@

