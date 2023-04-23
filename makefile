SOURCES := $(wildcard src/*.c) $(wildcard lib/*.c)
OBJECTS := $(SOURCES:.c=.o)
CFLAGS := -static -lsodium -ljpeg -lpng -lz

# Output directory and executable's name (depending on the operating system)
ifeq ($(OS),Windows_NT)
	SHELL := cmd.exe
    CFLAGS := -I "lib" -L "lib" -I "\msys64\ucrt64\include" -I "\msys64\usr\include" -L "\ucrt64\mingw64\lib" -L "\msys64\usr\lib" $(CFLAGS) -largp -lmsys-2.0
    DIR := bin/windows
    EXECUTABLE := imgconceal.exe
else
    DIR := bin/linux
    EXECUTABLE := imgconceal
    CFLAGS += -lm
endif

.PHONY: release debug memcheck all clean

# Release build (no debug flags, and otimizations enabled)
release: CFLAGS += -O3 -DNDEBUG
release: DIR := $(addsuffix /release,$(DIR))
release: TARGET := release
release: all

# Debug build (for use on a debugger)
debug: CFLAGS += -g
debug: DIR := $(addsuffix /debug,$(DIR))
debug: TARGET := debug
debug: all

# Check whether there are memory leaks or overruns
# Note: '-static' cannot be used with '-fsanitize=address'
memcheck: CFLAGS += -g -fsanitize=address -fsanitize=leak
memcheck: CFLAGS := $(patsubst -static,,$(CFLAGS))
memcheck: DIR := $(addsuffix /memcheck,$(DIR))
memcheck: TARGET := memcheck
memcheck: all

# If on Windows, build the Argp library (because the one from MSYS2 just don't work for us)
ifeq ($(OS),Windows_NT)
lib/libargp.a: lib/libargp-20110921
	\msys64\msys2_shell.cmd -defterm -no-start -ucrt64 -where "lib\libargp-20110921" -c "./configure; make"
	copy /Y lib\libargp-20110921\gllib\.libs\libargp.a lib\libargp.a
	copy /Y lib\libargp-20110921\gllib\.libs\libargp.la lib\libargp.la
	copy /Y lib\libargp-20110921\gllib\argp.h lib\argp.h
endif

# Build the executable
ifeq ($(OS),Windows_NT)
all: lib/libargp.a $(DIR)/$(EXECUTABLE)
else
all: $(DIR)/$(EXECUTABLE)
endif

# Create the output folder and link the objects together
$(DIR)/$(EXECUTABLE): $(OBJECTS)
    ifeq ($(OS),Windows_NT)
	    -mkdir $(subst /,\,$(DIR))
    else
	    mkdir -p $(DIR)
    endif
	gcc $(OBJECTS) -o $(DIR)/$(EXECUTABLE) $(CFLAGS)

# Compile the objects
%.o: %.c
	gcc -c $< -o $@ $(CFLAGS)

# Delete the build artifacts
clean:
    ifeq ($(OS),Windows_NT)
	    -del /S "src\*.o"
	    -del /S "lib\*.o"
		-del /S "lib\*.lo"
		-del /S "lib\*.Plo"
    else
	    -rm -rv src/*.o
	    -rm -rv lib/*.o
    endif