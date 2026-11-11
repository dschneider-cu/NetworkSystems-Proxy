CC     := gcc
CFLAGS := -Wall -Wextra -O0 -g
LIBS   := -lssl -lcrypto

TARGET      := proxy
TEST_TARGET := test
CACHE_DIR   := cache

# Find all c files in src/
SRCS = $(shell find src -name "*.c")
# Place .o files in obj/
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))

PROXY_OBJS := obj/proxy.o $(filter-out obj/proxy.o obj/test.o, $(OBJS))
TEST_OBJS  := obj/test.o  $(filter-out obj/proxy.o obj/test.o, $(OBJS))

.PHONY: all proxy test clean

all: proxy

# Build proxy binary
$(TARGET): $(PROXY_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile all .c to .o
obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(TEST_TARGET) obj $(CACHE_DIR)/*
