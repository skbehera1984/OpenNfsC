set(SOURCES Thread.cpp
            Stringf.cpp
            BasicConnection.cpp
            ConnectionMgr.cpp
            DataTypes.cpp
            MountCall.cpp
            Nfs3ApiHandle.cpp
            Nfs4ApiHandle.cpp
            Nfs4Call.cpp
            NfsCall.cpp
            NfsConnectionGroup.cpp
            NfsUtil.cpp
            NlmCall.cpp
            Packet.cpp
            Portmap.cpp
            Responder.cpp
            RpcCall.cpp
            RpcConnection.cpp
            RpcPacket.cpp)

add_library(OpenNfsC ${SOURCES})
