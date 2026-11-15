CC     := gcc
CFLAGS := -Wall -Wextra -Wno-unused-parameter -O0 -g
LIBS   := -lssl -lcrypto -pthread

TARGET        := proxy
CLIENT_TARGET := client
TEST_TARGET   := test
CACHE_DIR     := cache

# Find all c files in src/
SRCS = $(shell find src -name "*.c")
# Place .o files in obj/
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))

PROXY_OBJS  := obj/proxy.o $(filter-out obj/proxy.o obj/client.o obj/test.o, $(OBJS))
CLIENT_OBJS := obj/client.o $(filter-out obj/proxy.o obj/client.o obj/test.o, $(OBJS))
TEST_OBJS   := obj/test.o $(filter-out obj/proxy.o obj/client.o obj/test.o, $(OBJS))

.PHONY: all clean

all: $(TARGET) $(CLIENT_TARGET)

# Build proxy binary
$(TARGET): $(PROXY_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile all .c to .o
obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(CLIENT_TARGET) $(TEST_TARGET) obj $(CACHE_DIR)/*
