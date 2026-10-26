CC     := gcc
CFLAGS := -Wall -Wextra -O2
LIBS   := -lssl -lcrypto

TARGET      := proxy
CACHE_DIR   := cache

# Find all c files in src/
SRCS = $(shell find src -name "*.c")
# Place .o files in obj/
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))

.PHONY: all proxy test clean

all: proxy

# Build proxy binary
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Build client binary
$(CLIENT_BIN): $(CLIENT_OBJS)
	mkdir -p $(CLIENT_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Compile all .c to .o
obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) obj $(CACHE_DIR)/*
