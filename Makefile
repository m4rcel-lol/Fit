CC = gcc
CFLAGS = -Wall -Wextra -std=c17 -O2 -I./include
LDFLAGS = -lz -lssl -lcrypto

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

FIT_SOURCES = $(wildcard $(SRC_DIR)/*.c)
FIT_OBJECTS = $(FIT_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/fit

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(FIT_OBJECTS) | $(BIN_DIR)
	$(CC) $(FIT_OBJECTS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/fit

test: $(TARGET)
	@echo "Running tests..."
	@./tests/run_tests.sh

.PHONY: docker
docker:
	docker build -t fit:latest .

.PHONY: docker-compose
docker-compose:
	docker-compose up -d
