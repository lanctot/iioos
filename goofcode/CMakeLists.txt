project(bluff)

set(CMAKE_CXX_COMPILER "g++")

# find a way to make this OS-dependent
#set(CMAKE_CXX_FLAGS "-g -pg -O0 -Wall -I/sw/include")
#set(CMAKE_CXX_FLAGS "-g -pg -O2 -Wall -I/sw/include")
set(CMAKE_CXX_FLAGS "-g -O2 -Wall -Wno-unused-variable -I/sw/include")
#set(CMAKE_CXX_FLAGS "-O3 -Wall -DNDEBUG -I/sw/include")

# find a way to make this OS-dependent
# replace this path below with your home directory 
# (must be the full path.. e.g. no env. variables as above)
#set(CMAKE_EXE_LINKER_FLAGS "-g -pg -O0 -L/sw/lib")
#set(CMAKE_EXE_LINKER_FLAGS "-g -pg -O2 -L/sw/lib")
set(CMAKE_EXE_LINKER_FLAGS "-g -O2 -Wno-unused-variable -L/sw/lib")
#set(CMAKE_EXE_LINKER_FLAGS "-O3 -DNDEBUG -L/sw/lib")

set (LIBFILES infosetstore.cpp goof.cpp util.cpp)

add_executable (cfr cfr.cpp ${LIBFILES})
target_link_libraries(cfr m)

