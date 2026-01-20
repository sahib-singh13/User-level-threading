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

.PHONY: all clean

all: $(DEMO_BIN)

$(BINDIR):
	mkdir -p $(BINDIR)

$(DEMO_BIN): $(DEMO_SRC) $(LIB_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ 

timer_demo: $(TIMER_DEMO_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $(TIMER_DEMO_BIN) $(TIMER_DEMO_SRC)
	
clean:
	rm -rf $(BINDIR) *.o
