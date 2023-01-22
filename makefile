SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:.c=.o)
CFLAGS := -static -lsodium -ljpeg -lz

# Output directory and executable's name (depending on the operating system)
ifeq ($(OS),Windows_NT)
    DIR := bin/windows
    EXECUTABLE := imgconceal.exe
else
    DIR := bin/linux
    EXECUTABLE := imgconceal
    CFLAGS += -lm
endif

.PHONY: release debug memcheck all clean

# Release build (no debug flags, and otimizations enabled)
release: CFLAGS += -O3
release: DIR := $(addsuffix /release,$(DIR))
release: all

# Debug build (for use on a debugger)
debug: CFLAGS += -g
debug: DIR := $(addsuffix /debug,$(DIR))
debug: all

# Check whether there are memory leaks or overruns
# Note: '-static' cannot be used with '-fsanitize=address'
memcheck: CFLAGS += -g -fsanitize=address -fsanitize=leak
memcheck: CFLAGS := $(patsubst -static,,$(CFLAGS))
memcheck: DIR := $(addsuffix /memcheck,$(DIR))
memcheck: all

# Build the executable
all: $(EXECUTABLE)

# Create the output folder and link the objects together
$(EXECUTABLE): $(OBJECTS)
    ifeq ($(OS),Windows_NT)
	    -mkdir $(DIR)
    else
	    mkdir -p $(DIR)
    endif
	gcc $(OBJECTS) -o $(DIR)/$(EXECUTABLE) $(CFLAGS)

# Compile the objects
src/%.o: src/%.c
	gcc -c $< -o $@ $(CFLAGS)

# Delete the build artifacts
clean:
    ifeq ($(OS),Windows_NT)
	    -del /S src/*.o
    else
	    -rm -rv src/*.o
    endif