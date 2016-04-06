BIN := carpc-controller
CC := gcc
CPP := g++
LD := g++

CPP_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))
INC_DIRS := -Iinc \
			-I/usr/local/include
LIB_DIRS := -L/usr/local/lib
LIBS := -lwiringPi -lpthread -lrt

LD_FLAGS := $(LIB_DIRS) $(LIBS)
CC_FLAGS := $(INC_DIRS)

$(BIN): $(OBJ_FILES)
	@echo $@
	@$(CPP) $(LD_FLAGS) -o $@ $^

obj/%.o: src/%.cpp
	@echo $<
	@$(LD) $(CC_FLAGS) -c -o $@ $<

clean:
	@rm -rf obj/* carpc-controller