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
include CMakeFiles/xxhash.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/xxhash.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/xxhash.dir/flags.make

CMakeFiles/xxhash.dir/xxhash/xxhash.c.o: CMakeFiles/xxhash.dir/flags.make
CMakeFiles/xxhash.dir/xxhash/xxhash.c.o: xxhash/xxhash.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/user/Desktop/paper-version-1/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/xxhash.dir/xxhash/xxhash.c.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/xxhash.dir/xxhash/xxhash.c.o   -c /Users/user/Desktop/paper-version-1/xxhash/xxhash.c

CMakeFiles/xxhash.dir/xxhash/xxhash.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/xxhash.dir/xxhash/xxhash.c.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /Users/user/Desktop/paper-version-1/xxhash/xxhash.c > CMakeFiles/xxhash.dir/xxhash/xxhash.c.i

CMakeFiles/xxhash.dir/xxhash/xxhash.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/xxhash.dir/xxhash/xxhash.c.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /Users/user/Desktop/paper-version-1/xxhash/xxhash.c -o CMakeFiles/xxhash.dir/xxhash/xxhash.c.s

CMakeFiles/xxhash.dir/xxhash/xxhash.c.o.requires:

.PHONY : CMakeFiles/xxhash.dir/xxhash/xxhash.c.o.requires

CMakeFiles/xxhash.dir/xxhash/xxhash.c.o.provides: CMakeFiles/xxhash.dir/xxhash/xxhash.c.o.requires
	$(MAKE) -f CMakeFiles/xxhash.dir/build.make CMakeFiles/xxhash.dir/xxhash/xxhash.c.o.provides.build
.PHONY : CMakeFiles/xxhash.dir/xxhash/xxhash.c.o.provides

CMakeFiles/xxhash.dir/xxhash/xxhash.c.o.provides.build: CMakeFiles/xxhash.dir/xxhash/xxhash.c.o


# Object files for target xxhash
xxhash_OBJECTS = \
"CMakeFiles/xxhash.dir/xxhash/xxhash.c.o"

# External object files for target xxhash
xxhash_EXTERNAL_OBJECTS =

libxxhash.a: CMakeFiles/xxhash.dir/xxhash/xxhash.c.o
libxxhash.a: CMakeFiles/xxhash.dir/build.make
libxxhash.a: CMakeFiles/xxhash.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/user/Desktop/paper-version-1/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C static library libxxhash.a"
	$(CMAKE_COMMAND) -P CMakeFiles/xxhash.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/xxhash.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/xxhash.dir/build: libxxhash.a

.PHONY : CMakeFiles/xxhash.dir/build

CMakeFiles/xxhash.dir/requires: CMakeFiles/xxhash.dir/xxhash/xxhash.c.o.requires

.PHONY : CMakeFiles/xxhash.dir/requires

CMakeFiles/xxhash.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/xxhash.dir/cmake_clean.cmake
.PHONY : CMakeFiles/xxhash.dir/clean

CMakeFiles/xxhash.dir/depend:
	cd /Users/user/Desktop/paper-version-1 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1 /Users/user/Desktop/paper-version-1/CMakeFiles/xxhash.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/xxhash.dir/depend

