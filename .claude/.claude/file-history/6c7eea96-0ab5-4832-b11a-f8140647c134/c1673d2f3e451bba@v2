# Plain-make test runner for the SFML-free core library.
#
# The dev container has g++/make but no cmake or SFML, so this Makefile is the
# primary way to build and run unit tests there. It compiles every
# src/core/*.cpp plus every tests/test_*.cpp into one doctest binary.
#
#   make -C tests test     # build + run all tests
#   make -C tests clean    # remove build artifacts

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -g -O0 -pthread -Wall -Wextra
INCLUDES := -I../src/core -I../src -I../external -I.

BUILD  := build
TARGET := $(BUILD)/run_tests

CORE_SRCS := $(wildcard ../src/core/*.cpp)
TEST_SRCS := $(wildcard test_*.cpp)

CORE_OBJS := $(patsubst ../src/core/%.cpp,$(BUILD)/core_%.o,$(CORE_SRCS))
TEST_OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(TEST_SRCS))

.PHONY: test build clean

test: $(TARGET)
	./$(TARGET)

build: $(TARGET)

$(TARGET): $(CORE_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/core_%.o: ../src/core/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
