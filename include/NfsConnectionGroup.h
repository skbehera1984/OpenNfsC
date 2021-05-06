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
#include <thread>
#include <mutex>
#include <time.h>
#include <unistd.h>

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
    NFSVersion getNfsVersion() { return m_nfsVersion; }
    void setTransport(TransportType transp) { m_nfsTransp = transp; }
    bool update();
    bool ensureConnection();

    static void init();
    static void fini();
    static NfsConnectionGroupPtr create(std::string   serverIP,
                                        TransportType transp,
                                        NFSVersion    version=NFSV3,
                                        bool          startKeepAlive = false);

    static NfsConnectionGroupPtr create(const char* serverIpStr,
                                        bool        useUdp,
                                        NFSVersion  version=NFSV3,
                                        bool        startKeepAlive = false);

    static NfsConnectionGroupPtr forceCreate(std::string   serverIP,
                                             TransportType transp,
                                             NFSVersion    version=NFSV3,
                                             bool          startKeepAlive=false);

    static NfsConnectionGroupPtr forceCreate(const char* serverIpStr,
                                             bool        useUdp,
                                             NFSVersion  version=NFSV3,
                                             bool        startKeepAlive=false);

    static bool addNfsConnectionGroup(std::string serverIP, TransportType transp);
    static NfsConnectionGroupPtr findNfsConnectionGroup(std::string serverIP);

    static void setUdpTransport() { gTransportConfig = TRANSP_UDP; }
    static void setTcpTransport() { gTransportConfig = TRANSP_TCP; }
    void setConnected() { m_bConnected = true; } // used for nfs v4 keepalive

  private:
    NfsConnectionGroup();        // not implemented
    NfsConnectionGroup(const NfsConnectionGroup&); // not implemented

    NfsConnectionGroup(std::string serverIP, NFSVersion nfsVersion = NFSV3, bool startKeepAlive = false);

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

  // CACHING OF HANDLES
  private:
    std::map<std::string, NfsFh> m_cachedDirHandles;
    std::mutex                   m_cachedDirHandlesMutex;
    std::map<std::string, NfsFh> m_cachedFileHandles;
    std::mutex                   m_cachedFileHandlesMutex;
  public:
    bool findCachedDirHandle(const std::string& path, NfsFh& fh);
    bool findCachedFileHandle(const std::string& path, NfsFh& fh);
    void insertDirHandle(const std::string& path, NfsFh& fh);
    void insertFileHandle(const std::string& path, NfsFh& fh);

  public:
        /* APIs */
    bool setLogLevel(unsigned int level);
    bool connect(std::string serverIP);
    bool getExports(list<string>& Exports);
    bool getRootFH(const std::string &nfs_export, NfsFh &rootFh, NfsError &status);
    bool getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH, NfsError &status);
    bool getDirFh(const std::string &dirPath, NfsFh &dirFH, NfsError &status);
    bool getFileHandle(NfsFh &rootFH, const std::string path, NfsFh &fileFh, NfsAttr &attr, NfsError &status);
    bool create(NfsFh &dirFh, std::string &fileName, NfsAttr *inAttr, NfsFh &fileFh, NfsAttr &outAttr, NfsError &status);
    bool open(const std::string filePath, uint32_t access, uint32_t shareAccess, uint32_t shareDeny, NfsFh &file, NfsError &status);
    bool read(NfsFh &fileFH, uint64_t offset, uint32_t length, std::string &data, uint32_t &bytesRead, bool &eof, NfsAttr &postAttr, NfsError &status);
    bool write(NfsFh &fileFH, uint64_t offset, uint32_t length, std::string &data, uint32_t &bytesWritten, NfsError &status);
    bool write_unstable(NfsFh &fileFH, uint64_t offset,std::string &data,uint32_t &bytesWritten,char *verf, const bool needverify, NfsError &status);
    bool close(NfsFh &fileFh, NfsAttr &postAttr, NfsError &status);
    bool remove(std::string &exp, std::string path, NfsError &status);
    bool remove(const NfsFh &parentFH, const string &name, NfsError &status);
    bool rename(NfsFh &fromDirFh, const std::string &fromName, NfsFh &toDirFh, const std::string toName, NfsError &status);
    bool rename(const std::string &nfs_export, const std::string &fromPath, const std::string &toPath, NfsError &status);
    bool readDir(std::string &exp, const std::string &dirPath, NfsFiles &files, NfsError &status);
    bool readDir(NfsFh &dirFh, NfsFiles &files, NfsError &status);
    bool truncate(NfsFh &fh, uint64_t size, NfsError &status);
    bool truncate(const std::string &path, uint64_t size, NfsError &status);
    bool access(const std::string &filePath, uint32_t accessRequested, NfsAccess &acc, NfsError &status);
    bool mkdir(const NfsFh &parentFH, const std::string dirName, uint32_t mode, NfsFh &dirFH, NfsError &status);
    bool mkdir(const std::string &path, uint32_t mode, NfsError &status, bool createPath = false);
    bool rmdir(std::string &exp, const std::string &path, NfsError &status);
    bool rmdir(const NfsFh &parentFH, const string &name, NfsError &status);
    bool commit(NfsFh &fh, uint64_t offset, uint32_t bytes, char *writeverf, NfsError &status);
    bool lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status, bool reclaim = false);
    bool unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status);
    bool setattr(NfsFh &fh, NfsAttr &attr, NfsError &status);
    bool getAttr(NfsFh &fh, NfsAttr &attr, NfsError &status);
    bool getAttr(const std::string& exp, const std::string& path, NfsAttr& attr, NfsError& status);
    bool fileExists(const std::string& exp, const std::string& path, NfsAttr& attr, NfsError& status);
    bool lookupPath(const std::string &exp_path, const std::string &pathFromRoot, NfsFh &lookup_fh, NfsAttr &lookup_attr, NfsError &status);
    bool lookupPath(NfsFh &rootFh, const std::string &pathFromRoot, NfsFh &lookup_fh, NfsAttr &lookup_attr, NfsError &status);
    bool lookup(const std::string &path, NfsFh &lookup_fh, NfsError &status);
    bool lookup(NfsFh &dirFh, const std::string &file, NfsFh &lookup_fh, NfsAttr &attr, NfsError &status);
    bool fsstat(NfsFh &rootFh, NfsFsStat &stat, uint32 &invarSec, NfsError &status);
    bool link(NfsFh           &tgtFh,
              NfsFh           &parentFh,
              const string    &linkName,
              NfsError        &status);
    bool symlink(const string &tgtPath,
                 NfsFh        &parentFh,
                 const string &linkName,
                 NfsError     &status);

    bool renewCid();

  public:
    char *getInitialClientVerifier() { return m_initialClientVerifier; }
    char *getClientVerifier() { return m_ClientVerifier; }
    void  setClientVerifier(char *vref) { memcpy(m_ClientVerifier, vref, NFS4_VERIFIER_SIZE); }
    const std::string& getClientName() { return m_ClientName; }
    uint64_t getClientId() { return m_ClientId; }
    void     setClientId(uint64_t id) { m_ClientId = id; }
    uint32_t getFileOPSeqId();
    void     incrementFileOPSeqId();

  private:
    /* NFSv4 specific fields */
    // these two verifiers are of size verifier4. 8 bytes
    char        *m_initialClientVerifier;
    char        *m_ClientVerifier; // confirmed by server
    std::string  m_ClientName;
    uint64_t     m_ClientId;
    bool         m_bConnected;
    uint32_t     m_file_op_seqid;

    // keepalive setup
    bool         m_keepalive;
    time_t       m_lastRenewCidTime;
    std::thread  m_nfsKeepAliveThread;
    std::mutex   m_mutex;
    void         do_keepAlive();
};

} //end of namespace

#endif /* _CONNECTION_GROUP_H_ */
