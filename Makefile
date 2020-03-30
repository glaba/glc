CXX      := g++
CXXFLAGS := -Iinc -std=c++17 -Wall -O0 -g
LDFLAGS  := -pthread

SRC_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(patsubst src/%.cpp, obj/%.o, $(SRC_FILES))

.PHONY: all clean

all: glc

glc: $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

obj/%.o: src/%.cpp obj/
	$(CXX) $(CXXFLAGS) -c -o $@ $<

obj/:
	mkdir -p obj

clean:
	rm -rf glc obj
