CC     := gcc
CFLAGS := -Wall -Wextra -O0 -g
LIBS   := -lssl -lcrypto

TARGET      := proxy
CLIENT_TARGET := client
CACHE_DIR   := cache

# Find all c files in src/
SRCS = $(shell find src -name "*.c")
# Place .o files in obj/
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))

PROXY_OBJS := obj/proxy.o $(filter-out obj/proxy.o obj/client.o, $(OBJS))
CLIENT_OBJS  := obj/client.o  $(filter-out obj/proxy.o obj/client.o, $(OBJS))

.PHONY: all proxy client clean

all: proxy

# Build proxy binary
$(TARGET): $(PROXY_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile all .c to .o
obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(client_TARGET) obj $(CACHE_DIR)/*
