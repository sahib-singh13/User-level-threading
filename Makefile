CXX := g++
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wno-deprecated-declarations

BINDIR := bin
SRCDIR := src
EXAMPLE_DIR := example

# Output binary name
DEMO_BIN := $(BINDIR)/phase1_demo

# Source files
LIB_SRC := $(SRCDIR)/uthread.cpp
DEMO_SRC := $(EXAMPLE_DIR)/phase1_demo.cpp
TIMER_DEMO_SRC := example/timer_demo.cpp
TIMER_DEMO_BIN := bin/timer_demo
PHASE2_DEMO_SRC := example/phase2_demo.cpp
PHASE2_DEMO_BIN := bin/phase2_demo

.PHONY: all clean

all: $(DEMO_BIN) $(PHASE2_DEMO_BIN)

$(BINDIR):
	mkdir -p $(BINDIR)

$(DEMO_BIN): $(DEMO_SRC) $(LIB_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ 

timer_demo: $(TIMER_DEMO_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $(TIMER_DEMO_BIN) $(TIMER_DEMO_SRC)
	
# Compile phase 2 demo
$(PHASE2_DEMO_BIN): $(PHASE2_DEMO_SRC) $(LIB_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^
	
clean:
	rm -rf $(BINDIR) *.o
