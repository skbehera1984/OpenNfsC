/*
   Copyright (C) 2020 by Sarat Kumar Behera <beherasaratkumar@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _CONNECTION_GROUP_H_
#define _CONNECTION_GROUP_H_

#include "SmartPtr.h"
#include "RpcDefs.h"
#include "RpcConnection.h"
#include "DataTypes.h"
#include "Nfs3ApiHandle.h"
#include "Nfs4ApiHandle.h"
#include <nfsrpc/nfs4.h>
#include <Thread.h>
#include <map>

namespace OpenNfsC {

//forward declaration
class RpcConnection;

enum NFSVersion
{
    NFS_UNKNOWN = 0,
    NFSV1,
    NFSV2,
    NFSV3,
    NFSV4
};

class NfsConnectionGroup;
typedef SmartPtr<NfsConnectionGroup> NfsConnectionGroupPtr;

class NfsConnectionGroup : public SmartRef
{
  public:
    virtual ~NfsConnectionGroup();
    bool removeFromPool();

    BasicConnectionPtr getNfsConnection(ServiceType type) const
    {
        return m_connections[type][m_nfsTransp];
    }

    std::string getIP() { return m_serverIP; }
    const char* getServerIpStr() { return m_serverIPStr; }
    void setTransport(TransportType transp) { m_nfsTransp = transp; }
    bool update();
    bool ensureConnection();

    static void init();
    static void fini();
    static NfsConnectionGroupPtr create(std::string   serverIP,
                                        TransportType transp,
                                        NFSVersion    version=NFSV3);

    static NfsConnectionGroupPtr create(const char* serverIpStr,
                                        bool        useUdp,
                                        NFSVersion  version=NFSV3);

    static bool addNfsConnectionGroup(std::string serverIP, TransportType transp);
    static NfsConnectionGroupPtr findNfsConnectionGroup(std::string serverIP);

    static void setUdpTransport() { gTransportConfig = TRANSP_UDP; }
    static void setTcpTransport() { gTransportConfig = TRANSP_TCP; }

  private:
    NfsConnectionGroup();        // not implemented
    NfsConnectionGroup(const NfsConnectionGroup&); // not implemented


    NfsConnectionGroup(std::string serverIP, NFSVersion nfsVersion = NFSV3);

    bool initNfs();
    bool updateRpcPorts(TransportType transp);
    bool initRpcService(ServiceType   progType,
                        uint32        version,
                        TransportType transpType,
                        const bool    bReset);

    bool ensurePortMapperConnection(bool& bForceRecreate);


  private:

    static OpenNfsC::Thread::Mutex serverTableMutex;
    static std::map<std::string, NfsConnectionGroupPtr> serverTable;

    static enum TransportType gTransportConfig;

    std:: string m_serverIP;
    enum TransportType m_nfsTransp;

    OpenNfsC::Thread::Mutex m_connMutex;
    BasicConnectionPtr m_connections[MAX_SERVICE][MAX_TRANSP];

    uint16 m_rpcPorts[MAX_SERVICE][4][MAX_TRANSP];
    char m_serverIPStr[46];
    enum NFSVersion    m_nfsVersion;

    NfsApiHandlePtr    m_NfsApiHandle = nullptr;;

  public:
        /* APIs */
    bool setLogLevel(unsigned int level);
    bool connect(std::string serverIP);
    bool getRootFH(const std::string &nfs_export);
    bool getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH);
    bool getDirFh(const std::string &dirPath, NfsFh &dirFH);
    bool open(const std::string filePath, uint32_t access, uint32_t shareAccess, uint32_t shareDeny, NfsFh &file);
    bool read(NfsFh &fileFH, uint64_t offset, uint32_t length, std::string &data, uint32_t &bytesRead, bool &eof);
    bool write(NfsFh &fileFH, uint64_t offset, uint32_t length, std::string &data, uint32_t &bytesWritten, NfsError &err);
    bool write_unstable(NfsFh &fileFH, uint64_t offset,std::string &data,uint32_t &bytesWritten,char *verf, const bool needverify);
    bool close(NfsFh &fileFh, NfsAttr &postAttr);
    bool remove(std::string path, NfsError &status);
    bool remove(const NfsFh &parentFH, const string &name, NfsError &status);
    bool rename(NfsFh &fromDirFh, const std::string &fromName, NfsFh &toDirFh, const std::string toName, NfsError &sts);
    bool rename(const std::string &nfs_export, const std::string &fromPath, const std::string &toPath);
    bool readDir(const std::string &dirPath, NfsFiles &files);
    bool truncate(NfsFh &fh, uint64_t size);
    bool truncate(const std::string &path, uint64_t size);
    bool access(const std::string &filePath, uint32_t accessRequested, NfsAccess &acc);
    bool mkdir(const NfsFh &parentFH, const std::string dirName, uint32_t mode, NfsFh &dirFH);
    bool mkdir(const std::string &path, uint32_t mode, bool createPath = false);
    bool rmdir(const std::string &path, NfsError &status);
    bool rmdir(const NfsFh &parentFH, const string &name, NfsError &status);
    bool commit(NfsFh &fh, uint64_t offset, uint32_t bytes, char *writeverf);
    bool lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, bool reclaim = false);
    bool unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length);
    bool setattr( NfsFh &fh, NfsAttr &attr);
    bool lookup(const std::string &path, NfsFh &lookup_fh);

  public:
    char *getInitialClientVerifier() { return m_initialClientVerifier; }
    char *getClientVerifier() { return m_ClientVerifier; }
    void  setClientVerifier(char *vref) { memcpy(m_ClientVerifier, vref, NFS4_VERIFIER_SIZE); }
    const std::string& getClientName() { return m_ClientName; }
    uint64_t getClientId() { return m_ClientId; }
    void     setClientId(uint64_t id) { m_ClientId = id; }
    uint32_t getFileOPSeqId();
    uint32_t getFileLockSeqId();

  private:
    /* NFSv4 specific fields */
    // these two verifiers are of size verifier4. 8 bytes
    char        *m_initialClientVerifier;
    char        *m_ClientVerifier; // confirmed by server
    std::string  m_ClientName;
    uint64_t     m_ClientId;
    std::mutex   m_seqid_mutex;
    uint32_t     m_file_op_seqid;
    uint32_t     m_file_lock_seqid;
};

} //end of namespace

#endif /* _CONNECTION_GROUP_H_ */
