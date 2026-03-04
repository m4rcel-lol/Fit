CC = gcc
CFLAGS = -Wall -Wextra -std=c17 -O2 -I./include
LDFLAGS = -lz -lssl -lcrypto
WEB_LDFLAGS = -lsqlite3

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

FIT_SOURCES = $(filter-out $(SRC_DIR)/web.c $(SRC_DIR)/web_enhanced.c, $(wildcard $(SRC_DIR)/*.c))
FIT_OBJECTS = $(FIT_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/fit
WEB_TARGET = $(BIN_DIR)/fitweb
WEB_ENHANCED_TARGET = $(BIN_DIR)/fitweb-enhanced

.PHONY: all clean install test web web-enhanced

all: $(TARGET) $(WEB_TARGET) $(WEB_ENHANCED_TARGET)

$(TARGET): $(FIT_OBJECTS) | $(BIN_DIR)
	$(CC) $(FIT_OBJECTS) -o $@ $(LDFLAGS)

$(WEB_TARGET): $(OBJ_DIR)/web.o | $(BIN_DIR)
	$(CC) $(OBJ_DIR)/web.o -o $@ $(WEB_LDFLAGS)

$(WEB_ENHANCED_TARGET): $(OBJ_DIR)/web_enhanced.o | $(BIN_DIR)
	$(CC) $(OBJ_DIR)/web_enhanced.o -o $@ $(WEB_LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: $(TARGET) $(WEB_TARGET)
	install -m 755 $(TARGET) /usr/local/bin/fit
	install -m 755 $(WEB_TARGET) /usr/local/bin/fitweb

web: $(WEB_TARGET)
	@echo "Starting Fit Web Interface on http://localhost:8080"
	@$(WEB_TARGET)

web-enhanced: $(WEB_ENHANCED_TARGET)
	@echo "Starting Fit Web Interface (Enhanced Edition) on http://localhost:8080"
	@$(WEB_ENHANCED_TARGET)

test: $(TARGET)
	@echo "Running tests..."
	@./tests/run_tests.sh

.PHONY: docker
docker:
	docker build -t fit:latest .

.PHONY: docker-compose
docker-compose:
	docker-compose up -d
