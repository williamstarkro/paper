# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.10.2/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.10.2/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/user/Desktop/paper-version-1

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/user/Desktop/paper-version-1

# Include any dependencies generated for this target.
include miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/depend.make

# Include the progress variables for this target.
include miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/progress.make

# Include the compile flags for this target's objects.
include miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/flags.make

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/flags.make
miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o: miniupnp/miniupnpc/minixmlvalid.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/user/Desktop/paper-version-1/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o"
	cd /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc && /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o   -c /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc/minixmlvalid.c

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/minixmlvalid.dir/minixmlvalid.c.i"
	cd /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc && /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc/minixmlvalid.c > CMakeFiles/minixmlvalid.dir/minixmlvalid.c.i

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/minixmlvalid.dir/minixmlvalid.c.s"
	cd /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc && /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc/minixmlvalid.c -o CMakeFiles/minixmlvalid.dir/minixmlvalid.c.s

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o.requires:

.PHONY : miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o.requires

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o.provides: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o.requires
	$(MAKE) -f miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/build.make miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o.provides.build
.PHONY : miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o.provides

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o.provides.build: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o


miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/flags.make
miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o: miniupnp/miniupnpc/minixml.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/user/Desktop/paper-version-1/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o"
	cd /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc && /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/minixmlvalid.dir/minixml.c.o   -c /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc/minixml.c

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/minixmlvalid.dir/minixml.c.i"
	cd /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc && /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc/minixml.c > CMakeFiles/minixmlvalid.dir/minixml.c.i

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/minixmlvalid.dir/minixml.c.s"
	cd /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc && /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc/minixml.c -o CMakeFiles/minixmlvalid.dir/minixml.c.s

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o.requires:

.PHONY : miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o.requires

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o.provides: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o.requires
	$(MAKE) -f miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/build.make miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o.provides.build
.PHONY : miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o.provides

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o.provides.build: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o


# Object files for target minixmlvalid
minixmlvalid_OBJECTS = \
"CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o" \
"CMakeFiles/minixmlvalid.dir/minixml.c.o"

# External object files for target minixmlvalid
minixmlvalid_EXTERNAL_OBJECTS =

miniupnp/miniupnpc/minixmlvalid: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o
miniupnp/miniupnpc/minixmlvalid: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o
miniupnp/miniupnpc/minixmlvalid: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/build.make
miniupnp/miniupnpc/minixmlvalid: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/user/Desktop/paper-version-1/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C executable minixmlvalid"
	cd /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/minixmlvalid.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/build: miniupnp/miniupnpc/minixmlvalid

.PHONY : miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/build

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/requires: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixmlvalid.c.o.requires
miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/requires: miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/minixml.c.o.requires

.PHONY : miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/requires

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/clean:
	cd /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc && $(CMAKE_COMMAND) -P CMakeFiles/minixmlvalid.dir/cmake_clean.cmake
.PHONY : miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/clean

miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/depend:
	cd /Users/user/Desktop/paper-version-1 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc /Users/user/Desktop/paper-version-1/miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : miniupnp/miniupnpc/CMakeFiles/minixmlvalid.dir/depend
