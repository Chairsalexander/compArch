# Makefile
# A Makefile for compiling and running the CacheSimulator program

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Target executable
TARGET = cacheSimulator

# Source files
SRC = cache.cpp

# Default target: build the program
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Clean up build artifacts
clean:
	rm -f $(TARGET)
	rm -f "memory_trace.txt"
