ifneq (,$(wildcard ./.env))
	include .env
	export
endif

TARGETS := $(notdir $(patsubst %.c,%,$(wildcard demo/*.c)))
CFLAGS = $(shell cat compile_flags.txt | tr '\n' ' ')
DEV_CFLAGS = -g -O0 -fdiagnostics-color=always -fno-omit-frame-pointer -fno-optimize-sibling-calls -DDEPLOY_ENV=$(DEPLOY_ENV)
TEST_CFLAGS = -Werror -DTEST_ENV=$(TEST_ENV)
SRC = src/express.c
SRC += $(wildcard deps/*/*.c) $(wildcard demo/*/*.c)
BUILD_DIR = build
PLATFORM := $(shell sh -c 'uname -s 2>/dev/null | tr 'a-z' 'A-Z'')

ifeq ($(PLATFORM),LINUX)
	CFLAGS += -lm -lBlocksRuntime -ldispatch -lbsd -luuid
else ifeq ($(PLATFORM),DARWIN)
	DEV_CFLAGS += -fsanitize=address,undefined
endif

CFLAGS += $(DEV_CFLAGS)

all: $(TARGETS)

.PHONY: $(TARGETS)
$(TARGETS):
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/$@ demo/$@.c $(SRC) $(CFLAGS)

.PHONY: test
test:
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/$@ test/test-app.c test/test-harnass.c test/tape.c $(SRC) $(CFLAGS) $(TEST_CFLAGS)
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

.PHONY: test-tape
test-tape:
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/$@ test/test-tape.c test/tape.c $(SRC) $(CFLAGS)
	$(BUILD_DIR)/$@
