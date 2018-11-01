# compiler
CCC = gcc

# C++ compiler flags (-g -O2 -Wall)
CCFLAGS = -g -O3 -Wall

# compile flags
LDFLAGS = -g

# source files
SRC = $(wildcard src/*.c)

OBJ = $(patsubst src%, build%.o, $(SRC))

OUT = app

.SUFFIXES: .c

default: $(OUT)

build/%.o: src/%
	mkdir -p build
	$(CCC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

$(OUT): $(OBJ)
	$(CCC) $(INCLUDES) $(CCFLAGS) $(OBJ) $(LIBS) -o $(OUT)

clean:
	rm -f $(OBJ) $(OUT)

test:
	echo $(SRC)
	echo $(OBJ)

all:
	make