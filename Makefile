CXX := g++
# -Wno-deprecated-declarations suppresses warnings for ucontext on newer Linux
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wno-deprecated-declarations

# Directories
BINDIR := bin
EXAMPLE_DIR := example

# Target Binary
DEMO_BIN := $(BINDIR)/context_demo
# Source File
DEMO_SRC := $(EXAMPLE_DIR)/context_switch_demo.cpp

.PHONY: all clean demo

all: demo

# Ensure bin directory exists
$(BINDIR):
	mkdir -p $(BINDIR)

# Compile the demo
demo: $(DEMO_SRC) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $(DEMO_BIN) $(DEMO_SRC)

clean:
	rm -rf $(BINDIR) *.o
