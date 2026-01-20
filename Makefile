CXX := g++
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wno-deprecated-declarations

BINDIR := bin
SRCDIR := src
EXAMPLE_DIR := example

# Sources
LIB_SRC := $(SRCDIR)/uthread.cpp

# Demos
PHASE1_SRC := $(EXAMPLE_DIR)/phase1_demo.cpp
PHASE1_BIN := $(BINDIR)/phase1_demo

PHASE2_SRC := $(EXAMPLE_DIR)/phase2_demo.cpp
PHASE2_BIN := $(BINDIR)/phase2_demo

MUTEX_SRC := $(EXAMPLE_DIR)/mutex_demo.cpp
MUTEX_BIN := $(BINDIR)/mutex_demo

PRIORITY_SRC := $(EXAMPLE_DIR)/priority_demo.cpp
PRIORITY_BIN := $(BINDIR)/priority_demo

.PHONY: all clean phase1 phase2 mutex

# Build all demos
all:phase1 phase2 mutex priority

$(BINDIR):
	mkdir -p $(BINDIR)

# Individual Targets
phase1: $(PHASE1_BIN)
phase2: $(PHASE2_BIN)
mutex: $(MUTEX_BIN)

$(PHASE1_BIN): $(PHASE1_SRC) $(LIB_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(PHASE2_BIN): $(PHASE2_SRC) $(LIB_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(MUTEX_BIN): $(MUTEX_SRC) $(LIB_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

priority: $(PRIORITY_BIN)

$(PRIORITY_BIN): $(PRIORITY_SRC) $(LIB_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^
	
clean:
	rm -rf $(BINDIR) *.o
