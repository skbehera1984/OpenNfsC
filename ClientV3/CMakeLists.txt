include_directories(include
                    include/nfsrpc)

#find_library(LIB_OPENNFSC OpenNfsC)
add_executable(nfs3Client client.cpp)

target_link_libraries(nfs3Client OpenNfsC pthread)
