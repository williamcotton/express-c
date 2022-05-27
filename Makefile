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
DEV_CFLAGS = -g -O0
TEST_CFLAGS = -Werror
EXPRESS_SRC = $(wildcard src/*/*.c) $(wildcard src/*.c)
SRC = $(EXPRESS_SRC) $(wildcard deps/*/*.c)
TEST_SRC = $(wildcard test/*.c) $(wildcard test/*/*.c)
BUILD_DIR = build

ifeq ($(BUILD_ENV),development)
	TEST_CFLAGS += -DDEV_ENV
endif

ifeq ($(PLATFORM),LINUX)
	CFLAGS += -lm -lBlocksRuntime -ldispatch -lbsd -luuid -lpthread -ldl
	TEST_CFLAGS += -Wl,--wrap=stat -Wl,--wrap=regcomp -Wl,--wrap=accept -Wl,--wrap=socket -Wl,--wrap=epoll_ctl -Wl,--wrap=listen
	PROD_CFLAGS = -Ofast
else ifeq ($(PLATFORM),DARWIN)
	DEV_CFLAGS += -fsanitize=address,undefined,implicit-conversion,float-divide-by-zero,local-bounds,nullability
	PROD_CFLAGS = -Ofast
endif

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
	$(COV) show $(BUILD_DIR)/$@ -instr-profile=$(BUILD_DIR)/test.profdata -ignore-filename-regex="/deps|demo|test/"

test-coverage-html:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/coverage
	$(CC) -o $(BUILD_DIR)/$@ $(TEST_SRC) $(SRC) $(CFLAGS) $(TEST_CFLAGS) $(DEV_CFLAGS) -fprofile-instr-generate -fcoverage-mapping
	LLVM_PROFILE_FILE="build/test.profraw" $(BUILD_DIR)/$@
	$(PROFDATA) merge -sparse build/test.profraw -o build/test.profdata
	$(COV) show $(BUILD_DIR)/$@ -instr-profile=$(BUILD_DIR)/test.profdata -ignore-filename-regex="/deps|demo|test/" -format=html > $(BUILD_DIR)/code-coverage.html

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

test-analyze:
	clang --analyze $(SRC) $(shell cat compile_flags.txt | tr '\n' ' ') -I$(shell pg_config --includedir) -Xanalyzer -analyzer-output=text -Xanalyzer -analyzer-checker=core,deadcode,nullability,optin,osx,security,unix,valist -Xanalyzer -analyzer-disable-checker -Xanalyzer security.insecureAPI.DeprecatedOrUnsafeBufferHandling

test-threads:
	mkdir -p $(BUILD_DIR)
	$(CC) -o $(BUILD_DIR)/$@ $(TEST_SRC) $(SRC) $(CFLAGS) $(TEST_CFLAGS) -fsanitize=thread
	$(BUILD_DIR)/$@

manual-test-trace: build-test-trace
	SLEEP_TIME=5 RUN_X_TIMES=10 $(BUILD_DIR)/test

.PHONY: $(BUILD_DIR)/libexpress.so
$(BUILD_DIR)/libexpress.so:
	mkdir -p $(BUILD_DIR)
	$(CC) -shared -o $@ $(EXPRESS_SRC) $(wildcard deps/*/*.c) $(CFLAGS) $(PROD_CFLAGS) -fPIC

# TODO: copy express.supp, debug.plist to /usr/local/share

install: $(BUILD_DIR)/libexpress.so
	mkdir -p /usr/local/include
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/bin
	mkdir -p /usr/local/share
	rm -f /usr/local/lib/libexpress.so
ifeq ($(PLATFORM),DARWIN)
	codesign -s - -v -f --entitlements debug.plist $(BUILD_DIR)/libexpress.so
endif
	cp $(BUILD_DIR)/libexpress.so /usr/local/lib/libexpress.so
	cp -Rp src/* /usr/local/include
	cp -Rp deps/* /usr/local/include
	cp -Rp scripts/* /usr/local/bin
	cp express.supp /usr/local/share

.PHONY: $(BUILD_DIR)/libexpress-trace.so
$(BUILD_DIR)/libexpress-trace.so:
	mkdir -p $(BUILD_DIR)
	$(CC) -shared -o $@ $(EXPRESS_SRC) $(wildcard deps/*/*.c) $(CFLAGS) $(TEST_CFLAGS) -g -O0 -fPIC

# TODO: copy express.supp, debug.plist to /usr/local/share

install-trace: $(BUILD_DIR)/libexpress-trace.so
	mkdir -p /usr/local/include
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/bin
	mkdir -p /usr/local/share
	rm -f /usr/local/lib/libexpress-trace.so
ifeq ($(PLATFORM),DARWIN)
	codesign -s - -v -f --entitlements debug.plist $(BUILD_DIR)/libexpress-trace.so
endif
	cp $(BUILD_DIR)/libexpress-trace.so /usr/local/lib/libexpress-trace.so
	cp -Rp src/* /usr/local/include
	cp -Rp deps/* /usr/local/include
	cp -Rp scripts/* /usr/local/bin
	cp express.supp /usr/local/share
	
.PHONY: $(BUILD_DIR)/libexpress-debug.so
$(BUILD_DIR)/libexpress-debug.so:
	mkdir -p $(BUILD_DIR)
	$(CC) -shared -o $@ $(EXPRESS_SRC) $(wildcard deps/*/*.c) $(CFLAGS) $(DEV_CFLAGS) -fPIC

# TODO: copy express.supp, debug.plist to /usr/local/share

install-debug: $(BUILD_DIR)/libexpress-debug.so
	mkdir -p /usr/local/include
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/bin
	mkdir -p /usr/local/share
	rm -f /usr/local/lib/libexpress-debug.so
ifeq ($(PLATFORM),DARWIN)
	codesign -s - -v -f --entitlements debug.plist $(BUILD_DIR)/libexpress-debug.so
endif
	cp $(BUILD_DIR)/libexpress-debug.so /usr/local/lib/libexpress-debug.so
	cp -Rp src/* /usr/local/include
	cp -Rp deps/* /usr/local/include
	cp -Rp scripts/* /usr/local/bin
	cp express.supp /usr/local/share
	