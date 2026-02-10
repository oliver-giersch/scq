CXX := c++

INC := -I./include

CXXFLAGS := -std=c++20 -O2

all: test_scq test_simple_scq2

test_scq: test/test_scq.cpp
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

test_simple_scq2: test/test_simple_scq2.cpp
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

clean:
	$(RM) test_scq test_simple_scq2

.PHONY: all clean
