include_directories(include
                    include/nfsrpc)

#find_library(LIB_OPENNFSC OpenNfsC)
add_executable(nfs4Client client.cpp)

target_link_libraries(nfs4Client OpenNfsC pthread)
