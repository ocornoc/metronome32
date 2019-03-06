CXX_STANDARD = c++14
CXX_STANDARD_OPT = -std=$(CXX_STANDARD)
CXX_OPTIMIZE_OPT = -O2
CXX_ERRORS_OPT = -pedantic-errors
CXX_SUGGEST_OPT = -Wsuggest-attribute=pure -Wsuggest-attribute=const
CXX_WARNINGS_OPT = -Wall -Wextra -Wpedantic -Wshadow
CXX_SYMBOLS_OPT = -g
CXX_COVERAGE_OPT = -coverage

# You can comment out specific portions here.
CXXFLAGS = $(CXX_STANDARD_OPT)
CXXFLAGS += $(CXX_SYMBOLS_OPT)
CXXFLAGS += $(CXX_OPTIMIZE_OPT)
CXXFLAGS += $(CXX_ERRORS_OPT)
CXXFLAGS += $(CXX_SUGGEST_OPT)
CXXFLAGS += $(CXX_WARNINGS_OPT)
#CXXFLAGS += $(CXX_COVERAGE_OPT)

LD = ld

VALG = valgrind
VALGMC = $(VALG) --tool=memcheck
VALGMCFLAGS = --track-origins=yes --expensive-definedness-checks=yes
VALGCLG = $(VALG) --tool=callgrind
VALGCLGFLAGS = --collect-jumps=yes --cache-sim=yes --branch-sim=yes \
	       --simulate-hwpref=yes --simulate-wb=yes

RM_FILE = rm -f
RM_FOLDER = rm -rf
MKDIR = mkdir

THIS_MAKEFILE = $(abspath $(lastword $(MAKEFILE_LIST)))
MY_PATH = $(dir $(THIS_MAKEFILE))
BUILD_PATH = $(MY_PATH)build
SRC_PATH = $(MY_PATH)src

# Compiles the object in $(BUILD_PATH)/metronome32.o.
default: $(BUILD_PATH)/metronome32.o

# Compiles the object in $(BUILD_PATH)/metronome32.o AND compiles a test program.
# Then executes the test program.
test: default $(BUILD_PATH)/test
	@echo Testing test program by itself
	$(BUILD_PATH)/test

# Same as test except executes it in Valgrind's Memcheck.
test_memcheck: test
	@echo Testing test program with Valgrind\'s Memcheck
	$(VALGMC) $(VALGMCFLAGS) $(BUILD_PATH)/test

# Same as test except executes it in Valgrind's Callgrind.
test_callgrind: test
	@echo Testing test program with Valgrind\'s Callgrind.
	$(VALGCLG) $(VALGCLGFLAGS) $(BUILD_PATH)/test

# Calls both test_memcheck and test_callgrind.
test_full: test_memcheck test_callgrind

# Runs test but with gcov coverage enabled.
coverage: CXXFLAGS += $(CXX_COVERAGE_OPT)
coverage: test

# Deletes the build folder.
clean:
	$(RM_FOLDER) $(BUILD_PATH)

.PHONY: default test test_memcheck test_callgrind test_full clean coverage

$(BUILD_PATH):
	$(MKDIR) $(BUILD_PATH)

$(BUILD_PATH)/instruction.o: $(SRC_PATH)/instruction.cpp $(BUILD_PATH)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_PATH)/memory.o: $(SRC_PATH)/memory.cpp $(BUILD_PATH)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_PATH)/vm.o: $(SRC_PATH)/vm.cpp $(BUILD_PATH)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_PATH)/metronome32.o: $(BUILD_PATH)/instruction.o $(BUILD_PATH)/memory.o $(BUILD_PATH)/vm.o
	$(LD) -r $^ -o $@

$(BUILD_PATH)/test: $(SRC_PATH)/test.cpp $(BUILD_PATH)/metronome32.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@
