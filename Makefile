# Compiler and flags
CXX =  g++
CXXFLAGS = -g -Wall -Wextra -O2 -I src
LDFLAGS = 

# Directories
SRC_DIR = src
BUILD_DIR = build
COMMON_DIR = $(SRC_DIR)/common
HASHTABLE_DIR = $(SRC_DIR)/hashtable
SORTED_SET_DIR = $(SRC_DIR)/sorted_set
TREE_DIR = $(SRC_DIR)/tree
THREAD_POOL_DIR = $(SRC_DIR)/thread
TEST_DIR = tests

# Target executables
SERVER = server
CLIENT = client 
TEST1 = test_avl
TEST2 = test_offset
TEST3 = test_heap

# Source files
SERVER_SOURCE = $(SRC_DIR)/server.cpp \
				$(HASHTABLE_DIR)/hashtable.cpp \
				$(SORTED_SET_DIR)/zset.cpp \
				$(TREE_DIR)/avl.cpp \
				$(TREE_DIR)/heap.cpp \
				$(THREAD_POOL_DIR)/thread_pool.cpp

CLIENT_SOURCE = $(SRC_DIR)/client.cpp 

TEST1_SOURCE = $(TEST_DIR)/test_avl.cpp \
			   $(TREE_DIR)/avl.cpp

TEST2_SOURCE = $(TEST_DIR)/test_offset.cpp \
			   $(TREE_DIR)/avl.cpp
			   
TEST3_SOURCE = $(TEST_DIR)/test_heap.cpp \
			   $(TREE_DIR)/heap.cpp

# Object files
SERVER_OBJECT = $(SERVER_SOURCE:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
CLIENT_OBJECT = $(CLIENT_SOURCE:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TEST1_OBJECT = $(TEST1_SOURCE:$(TEST_DIR)/%.cpp=$(BUILD_DIR)/tests/%.o)
TEST2_OBJECT = $(TEST2_SOURCE:$(TEST_DIR)/%.cpp=$(BUILD_DIR)/tests/%.o)
TEST3_OBJECT = $(TEST3_SOURCE:$(TEST_DIR)/%.cpp=$(BUILD_DIR)/tests/%.o)

# Default target - build both programs
all: $(SERVER) $(CLIENT)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/hashtable
	mkdir -p $(BUILD_DIR)/sorted_set
	mkdir -p $(BUILD_DIR)/tree
	mkdir -p $(BUILD_DIR)/thread
	mkdir -p $(BUILD_DIR)/tests

# Build server
$(SERVER): $(SERVER_OBJECT)
	$(CXX) $(SERVER_OBJECT) -o $@ $(LDFLAGS) 

# Build client
$(CLIENT): $(CLIENT_OBJECT)
	$(CXX) $(CLIENT_OBJECT) -o $@ $(LDFLAGS) 

# Build tests
$(TEST1): $(TEST1_OBJECT)
	$(CXX) $(TEST1_OBJECT) -o $@ $(LDFLAGS)

$(TEST2): $(TEST2_OBJECT)
	$(CXX) $(TEST2_OBJECT) -o $@ $(LDFLAGS)

$(TEST3): $(TEST3_OBJECT)
	$(CXX) $(TEST3_OBJECT) -o $@ $(LDFLAGS)

# Test target to build all tests
test: $(TEST1) $(TEST2) $(TEST3)
	@echo "Tests compiled successfully"

# Object files (with automatic directory creation)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ 

# Test object files pattern rule
$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ 

# Clean up generated files
clean:
	rm -rf $(BUILD_DIR) $(SERVER) $(CLIENT) $(TEST1) $(TEST2) $(TEST3)

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

