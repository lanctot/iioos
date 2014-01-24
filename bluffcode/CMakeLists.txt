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

#set (LIBFILES infosetstore.cpp bluff.cpp util.cpp br.cpp)
set (LIBFILES infosetstore.cpp bluff.cpp util.cpp br.cpp abstraction.cpp eval.cpp sampling.cpp fsicommon.cpp seqstore.cpp ismcts.cpp cfroos.cpp search.cpp)

add_executable (stitching stitching.cpp ${LIBFILES})
target_link_libraries(stitching m)

#add_executable (cfroos cfroos.cpp ${LIBFILES})
#target_link_libraries(cfroos m)

add_executable (pbos pbos.cpp ${LIBFILES})
target_link_libraries(pbos m)

#add_executable (cfros cfros.cpp ${LIBFILES})
#target_link_libraries(cfros m)

#add_executable (cfr cfr.cpp ${LIBFILES})
#target_link_libraries(cfr m)

#add_executable (cfres cfres.cpp ${LIBFILES})
#target_link_libraries(cfres m)

#add_executable (cfrbr cfrbr.cpp ${LIBFILES})
#target_link_libraries(cfrbr m)

#add_executable (cfrcs cfrcs.cpp ${LIBFILES})
#target_link_libraries(cfrcs m)

#add_executable (cfrcsmt cfrcsmt.cpp ${LIBFILES})
#target_link_libraries(cfrcsmt m pthread)

#add_executable (cfresmt cfresmt.cpp ${LIBFILES})
#target_link_libraries(cfresmt m pthread)

#add_executable (fsimt fsimt.cpp ${LIBFILES})
#target_link_libraries(fsimt m pthread)

add_executable (pcs pcs.cpp ${LIBFILES})
target_link_libraries(pcs m)

#add_executable (br brmain.cpp ${LIBFILES})
#target_link_libraries(br m)

add_executable (sim sim.cpp ${LIBFILES})
target_link_libraries(sim m)

#add_executable (cfros-orig cfros-orig.cpp ${LIBFILES})
#target_link_libraries(cfros-orig m)

#add_executable (eval evalmain.cpp ${LIBFILES})
#target_link_libraries(eval m)

#add_executable (cfra cfra.cpp ${LIBFILES})
#target_link_libraries(cfra m)

#add_executable (pcsa pcsa.cpp ${LIBFILES})
#target_link_libraries(pcsa m)

#add_executable (bound bound.cpp ${LIBFILES})
#target_link_libraries(bound m)

#add_executable (cfrosa cfrosa.cpp ${LIBFILES})
#target_link_libraries(cfrosa m)

#add_executable (cfrprb cfrprb.cpp ${LIBFILES})
#target_link_libraries(cfrprb m)

#add_executable (cfrosp cfrosp.cpp ${LIBFILES})
#target_link_libraries(cfrosp m)

#add_executable (cfrabm cfrabm.cpp ${LIBFILES})
#target_link_libraries(cfrabm m)

#add_executable (cfrada cfrada.cpp ${LIBFILES})
#target_link_libraries(cfrada m)

#add_executable (cfrvarcounter cfrvarcounter.cpp ${LIBFILES})
#target_link_libraries(cfrvarcounter m)

#add_executable (fsicfr fsicfr.cpp ${LIBFILES})
#target_link_libraries(fsicfr m)



