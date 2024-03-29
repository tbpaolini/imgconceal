SOURCES := $(wildcard src/*.c) $(wildcard lib/*.c)
OBJECTS := $(SOURCES:.c=.o)
CFLAGS := -static -lsodium -ljpeg -lpng -lwebp -lwebpmux -lz

# Output directory and executable's name (depending on the operating system)
# The Windows version is being linked with Microsoft's Universal C Runtime (UCRT)
ifeq ($(OS),Windows_NT)
	SHELL := cmd.exe
    CFLAGS := -D_UCRT -I "lib" -L "lib" -I "\msys64\ucrt64\include" -L "\msys64\ucrt64\lib" $(CFLAGS) -largp -lsharpyuv
    DIR := bin/windows
	OBJECTS := src/resources.o $(OBJECTS)
    EXECUTABLE := imgconceal.exe
else
    DIR := bin/linux
    EXECUTABLE := imgconceal
    CFLAGS += -lm
endif

.PHONY: release debug memcheck all clean clean-all

# Release build (no debug flags, and optimizations enabled)
release: CFLAGS += -O3 -DNDEBUG
release: DIR := $(addsuffix /release,$(DIR))
release: TARGET := release
release: all

# Debug build (for use on a debugger)
debug: CFLAGS += -g
debug: DIR := $(addsuffix /debug,$(DIR))
debug: TARGET := debug
debug: all

# Check whether there are memory leaks or overruns, and also for undefined behavior
# Note: '-static' cannot be used with '-fsanitize=address'
memcheck: CFLAGS += -g -fsanitize=address -fsanitize=leak -fsanitize=undefined
memcheck: CFLAGS := $(patsubst -static,,$(CFLAGS))
memcheck: DIR := $(addsuffix /memcheck,$(DIR))
memcheck: TARGET := memcheck
memcheck: all

# If on Windows, build the Argp library (because the one from MSYS2 just don't work for us)
ifeq ($(OS),Windows_NT)
lib/libargp.a: lib/libargp-20110921
	\msys64\msys2_shell.cmd -defterm -no-start -ucrt64 -where "lib\libargp-20110921" -c "./configure; $(MAKE)"
	copy /Y lib\libargp-20110921\gllib\.libs\libargp.a lib\libargp.a
	copy /Y lib\libargp-20110921\gllib\.libs\libargp.la lib\libargp.la
	copy /Y lib\libargp-20110921\gllib\argp.h lib\argp.h
endif

ifeq ($(OS),Windows_NT)
src/resources.o: src/resources.rc
	-windres -i $< -o $@
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
	    -del "lib\*.o"
    else
	    -rm -rv src/*.o
	    -rm -rv lib/*.o
    endif

# On Windows, also removes the artifacts of the Argp's compilation.
# (On Linux, just does the same as the 'clean' target)
clean-all: clean
ifeq ($(OS),Windows_NT)
clean-all:
	\msys64\msys2_shell.cmd -defterm -no-start -ucrt64 -where "lib\libargp-20110921" -c "$(MAKE) clean"
endif