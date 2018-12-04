CCC = gcc
CCFLAGS = -g -O3 -Wall
LDFLAGS = -g

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src%, build%.o, $(SRC))
OUT = download

.SUFFIXES: .c

build/%.o: src/%
	mkdir -p build
	$(CCC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

$(OUT): $(OBJ)
	$(CCC) $(INCLUDES) $(CCFLAGS) $(OBJ) $(LIBS) -o $(OUT)

clean:
	rm -f $(OBJ) $(OUT)
