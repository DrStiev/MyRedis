# Compiler and flags
CXX =  g++
CXXFLAGS = -g -Wall -Wextra -O2 -I src
LDFLAGS = 

# Directories
SRC_DIR = src
BUILD_DIR = build
COMMON_DIR = $(SRC_DIR)/common
HASHTABLE_DIR = $(SRC_DIR)/hashtable

# Target executables
SERVER = server
CLIENT = client 

# Source files
SERVER_SOURCE = $(SRC_DIR)/server.cpp $(HASHTABLE_DIR)/hashtable.cpp
CLIENT_SOURCE = $(SRC_DIR)/client.cpp 

# Object files
SERVER_OBJECT = $(SERVER_SOURCE:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
CLIENT_OBJECT = $(CLIENT_SOURCE:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Default target - build both programs
all: $(SERVER) $(CLIENT)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/hashtable

# Build server
$(SERVER): $(SERVER_OBJECT)
	$(CXX) $(SERVER_OBJECT) -o $@ $(LDFLAGS) 

# Build client
$(CLIENT): $(CLIENT_OBJECT)
	$(CXX) $(CLIENT_OBJECT) -o $@ $(LDFLAGS) 

# Object files (with automatic directory creation)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ 

# Clean up generated files
clean:
	rm -rf $(BUILD_DIR) $(SERVER) $(CLIENT)

# Rebuild everything from scratch
rebuild: clean all

# Show variables (for debugging)
debug:
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "BUILD_DIR: $(BUILD_DIR)"
	@echo "SERVER_OBJ: $(SERVER_OBJECT)"
	@echo "CLIENT_OBJ: $(CLIENT_OBJECT)"

# Mark targets that don't create files
.PHONY: all clean rebuild

