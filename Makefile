# Compiler and flags
CXX =  g++
CXXFLAGS = -g -Wall -Wextra -O2

# Target executables
SERVER = server
CLIENT = client 

# Source files
SERVERSOURCE = server.cpp hashtable.cpp
CLIENTSOURCE = client.cpp 

# Object files
SERVEROBJECT = $(SERVERSOURCE:.cpp=.o)
CLIENTOBJECT = $(CLIENTSOURCE:.cpp=.o)

# Build server
$(SERVER): $(SERVEROBJECT)
	$(CXX) $(CXXFLAGS) -o $(SERVER) $(SERVEROBJECT)

# Build client
$(CLIENT): $(CLIENTOBJECT)
	$(CXX) $(CXXFLAGS) -o $(CLIENT) $(CLIENTOBJECT)

# Compile individual source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Default target - build both programs
all: $(SERVER) $(CLIENT)

# Clean up generated files
clean:
	rm -f $(SERVEROBJECT) $(CLIENTOBJECT) $(SERVER) $(CLIENT)

# Rebuild everything from scratch
rebuild: clean all

# Mark targets that don't create files
.PHONY: all clean rebuild

