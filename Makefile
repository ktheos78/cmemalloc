CC=gcc
CFLAGS=-Wall -Wextra -O2
LDFLAGS=-pthread

SRC_DIR=./src
BUILD_DIR=./build

all: always build

build: $(BUILD_DIR)/cmemalloc.so $(BUILD_DIR)/cmemalloc.o

# shared object file
$(BUILD_DIR)/cmemalloc.so: $(SRC_DIR)/cmemalloc.c
	$(CC) $(CFLAGS) -fPIC -shared $(LDFLAGS) $^ -o $@

# regular object file
$(BUILD_DIR)/cmemalloc.o: $(SRC_DIR)/cmemalloc.c
	$(CC) $(CFLAGS) -fcommon -c $^ -o $@

always:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f build/*