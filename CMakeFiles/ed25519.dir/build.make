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
include CMakeFiles/ed25519.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/ed25519.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/ed25519.dir/flags.make

CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o: CMakeFiles/ed25519.dir/flags.make
CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o: ed25519-donna/ed25519.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/user/Desktop/paper-version-1/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o   -c /Users/user/Desktop/paper-version-1/ed25519-donna/ed25519.c

CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /Users/user/Desktop/paper-version-1/ed25519-donna/ed25519.c > CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.i

CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /Users/user/Desktop/paper-version-1/ed25519-donna/ed25519.c -o CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.s

CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o.requires:

.PHONY : CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o.requires

CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o.provides: CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o.requires
	$(MAKE) -f CMakeFiles/ed25519.dir/build.make CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o.provides.build
.PHONY : CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o.provides

CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o.provides.build: CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o


# Object files for target ed25519
ed25519_OBJECTS = \
"CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o"

# External object files for target ed25519
ed25519_EXTERNAL_OBJECTS =

libed25519.a: CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o
libed25519.a: CMakeFiles/ed25519.dir/build.make
libed25519.a: CMakeFiles/ed25519.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/user/Desktop/paper-version-1/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C static library libed25519.a"
	$(CMAKE_COMMAND) -P CMakeFiles/ed25519.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/ed25519.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/ed25519.dir/build: libed25519.a

.PHONY : CMakeFiles/ed25519.dir/build

CMakeFiles/ed25519.dir/requires: CMakeFiles/ed25519.dir/ed25519-donna/ed25519.c.o.requires

.PHONY : CMakeFiles/ed25519.dir/requires

CMakeFiles/ed25519.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/ed25519.dir/cmake_clean.cmake
.PHONY : CMakeFiles/ed25519.dir/clean

CMakeFiles/ed25519.dir/depend:
	cd /Users/user/Desktop/paper-version-1 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1/CMakeFiles/ed25519.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/ed25519.dir/depend
