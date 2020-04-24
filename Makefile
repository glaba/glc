CXX      := g++
CXXFLAGS := -Iinc -std=c++17 -Wall -O0 -g
all:   CXXFLAGS += -D'DEBUG(body)='
debug: CXXFLAGS += -D'DEBUG(body)=body'
LDFLAGS  := -pthread

SRC_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(patsubst src/%.cpp, obj/%.o, $(SRC_FILES))

.PHONY: clean

all: glc

debug: glc

glc: $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

obj/%.o: src/%.cpp obj/
	$(CXX) $(CXXFLAGS) -c -o $@ $<

obj/:
	mkdir -p obj

clean:
	rm -rf glc obj
