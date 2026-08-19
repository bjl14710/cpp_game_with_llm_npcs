# Plain-make test runner for the rendering-free core library.
#
# The dev container has g++/make but no cmake or graphics libs, so this is the
# primary way to build and run unit tests there. It compiles every
# src/core/*.cpp plus every tests/test_*.cpp into one doctest binary.
#
#   make -C tests test         # build + run all tests
#   make -C tests build        # compile without running
#   make -C tests portability  # assert the macOS deployment floor still holds
#   make -C tests clean        # remove build artifacts

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -g -O0 -pthread -Wall -Wextra
INCLUDES := -I../src/core -I../external -I.

# Match the deployment floor CMakeLists.txt sets, so the two builds agree about
# which C++20 library features exist. Without this the cmake build would reject
# a std::format call while this one accepted it, and the disagreement would
# surface as a confusing "works in tests, fails in the app".
#
# Guarded on uname because this Makefile is the primary build in the Linux dev
# container (no cmake, no graphics libs there) and -mmacosx-version-min is an
# Apple-clang flag that g++ rejects outright.
#
# += rather than editing CXXFLAGS directly: a caller who overrides CXXFLAGS on
# the command line is doing so deliberately and should get exactly what they
# asked for. See scripts/check-portability-floor.sh, which relies on that.
ifeq ($(shell uname -s),Darwin)
MACOS_MIN := -mmacosx-version-min=11.0
endif
CXXFLAGS += $(MACOS_MIN)

BUILD  := build
TARGET := $(BUILD)/run_tests

CORE_SRCS := $(wildcard ../src/core/*.cpp)
TEST_SRCS := $(wildcard test_*.cpp)

CORE_OBJS := $(patsubst ../src/core/%.cpp,$(BUILD)/core_%.o,$(CORE_SRCS))
TEST_OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(TEST_SRCS))

# Vendored C sources (SQLite amalgamation) — compiled with the C compiler,
# without the C++ warning flags. It DOES need the deployment floor though: the
# C++ objects carry it and this one did not, so the link warned that
# ext_sqlite3.o "was built for newer macOS version (14.0) than being linked
# (11.0)". Same flag, both languages, or the binary's real floor is a lie.
CC        ?= cc
SQLITE_OBJ := $(BUILD)/ext_sqlite3.o

.PHONY: test build clean portability

test: $(TARGET)
	./$(TARGET)

build: $(TARGET)

# The portability floor is a compile-time property, so no doctest case can
# assert it — a test binary that links has already passed the only check there
# was. This target is the caller for the script that does assert it.
portability:
	@../scripts/check-portability-floor.sh

# Each recipe creates its own output directory with `mkdir -p $(@D)` rather
# than depending order-only on a `$(BUILD):` rule.
#
# There used to be such a rule, and it could never run. `BUILD := build` makes
# `build` a real directory target, but `build` is also the phony alias for
# "compile without running". Asking make for `build` therefore matched the
# phony declaration, which made the order-only prerequisite look circular:
#
#     make: Circular build/core_City.o <- build dependency dropped.
#     error: unable to open output file 'build/core_City.o': No such file or directory
#
# Make dropped the prerequisite, the mkdir never ran, and every compile failed
# on a missing directory. `make -C tests test` sidestepped it by reaching the
# objects through $(TARGET) instead of through the colliding name, which is why
# this survived unnoticed — `build` is also undocumented in the header above.
#
# Removing the directory target removes the collision outright, rather than
# renaming one side of it and leaving `make build` to silently produce an empty
# directory. $(@D) is make's built-in "directory part of the target".
$(TARGET): $(CORE_OBJS) $(TEST_OBJS) $(SQLITE_OBJ)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(SQLITE_OBJ): ../external/sqlite3.c
	@mkdir -p $(@D)
	$(CC) -O1 $(MACOS_MIN) -c $< -o $@

# -MMD -MP emits .d header-dependency files next to each object so editing a
# header (e.g. changing the Npc class layout) recompiles every dependent
# object instead of leaving stale, ABI-mismatched ones behind.
$(BUILD)/core_%.o: ../src/core/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

-include $(wildcard $(BUILD)/*.d)

clean:
	rm -rf $(BUILD)
