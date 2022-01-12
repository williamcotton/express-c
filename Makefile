TARGETS := $(notdir $(patsubst %.c,%,$(wildcard demo/*.c)))
CFLAGS = $(shell cat compile_flags.txt | tr '\n' ' ')
SRC = src/express.c
SRC += $(wildcard deps/*/*.c)
BUILD_DIR = build
PLATFORM := $(shell sh -c 'uname -s 2>/dev/null | tr 'a-z' 'A-Z'')

ifeq ($(PLATFORM),LINUX)
	CFLAGS += -lBlocksRuntime -ldispatch -lbsd
else ifeq ($(PLATFORM),DARWIN)
	CFLAGS += -fsanitize=address
endif

all: $(TARGETS)

.PHONY: $(TARGETS)
$(TARGETS):
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/$@ demo/$@.c $(SRC) $(CFLAGS)

.PHONY: test
test:
	mkdir -p $(BUILD_DIR)
	clang -o $(BUILD_DIR)/$@ test/test-app.c test/test-harnass.c $(SRC) $(CFLAGS)
	$(BUILD_DIR)/$@

clean:
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)
