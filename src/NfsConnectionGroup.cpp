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

#include "NfsConnectionGroup.h"
#include "RpcConnection.h"
#include "ConnectionMgr.h"
#include "Packet.h"
#include "RpcPacket.h"
#include "Portmap.h"
#include "RpcDefs.h"
#include "NfsUtil.h"
#include "Nfs4Call.h"

#include <nfsrpc/pmap.h>
#include <nfsrpc/nfs.h>
#include <nfsrpc/nfs4.h>
#include <nfsrpc/nlm.h>
#include <nfsrpc/mount.h>

#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

namespace OpenNfsC {

using Thread::Mutex;
using Thread::MutexGuard;

const uint16 portmap_port = 111;

Mutex NfsConnectionGroup::serverTableMutex;
std::map<std::string, NfsConnectionGroupPtr> NfsConnectionGroup::serverTable;
TransportType NfsConnectionGroup::gTransportConfig = TRANSP_TCP;


NfsConnectionGroup::NfsConnectionGroup(std::string serverIP, NFSVersion nfsVersion):m_serverIP(serverIP),m_nfsTransp(TRANSP_TCP)
{
  // Open the syslog
  setlogmask(LOG_UPTO(LOG_NOTICE));
  openlog("OpenNfsC", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

  // Properly zero-out all the members
  memset(m_serverIPStr, 0, sizeof(m_serverIPStr));
  memset(m_rpcPorts, 0, sizeof(m_rpcPorts));

  m_nfsVersion = nfsVersion;

  strcpy(m_serverIPStr,serverIP.c_str());
  syslog(LOG_INFO, "NfsConnectionGroup constructor %s\n", getServerIpStr());

  for (int i = 0; i < MAX_SERVICE; ++i)
    for (int j = 0; j < MAX_TRANSP; ++j)
      m_connections[i][j] = NULL;

  if (m_nfsVersion == NFSV3)
  {
    m_NfsApiHandle = new Nfs3ApiHandle(this);
  }
  else if (m_nfsVersion == NFSV4)
  {
    m_rpcPorts[NFS][4-1][TRANSP_TCP] = 2049;  // set default NFSV4 port

    /* Create a random initial verifier */
    uint64_t v;
    verifier4 verifier;

    m_initialClientVerifier = (char *) malloc(8);
    if (m_initialClientVerifier == NULL)
      throw std::string("Failed to allocate verifier");

    m_ClientVerifier = (char *) malloc(8);
    if (m_ClientVerifier == NULL)
      throw std::string("Failed to allocate verifier confirm");

    v = ((uint64_t)time(NULL) * 1000) << 32 | getpid();
    for (int i = 0; i < NFS4_VERIFIER_SIZE; i++)
    {
      verifier[i] = v & 0xff;
      v >>= 8;
    }
    memcpy(m_initialClientVerifier, verifier, NFS4_VERIFIER_SIZE);

    m_ClientName = std::string("fma");
    m_file_op_seqid = 0;
    m_file_lock_seqid = 0;

    m_NfsApiHandle = new Nfs4ApiHandle(this);
  }
}

NfsConnectionGroup::~NfsConnectionGroup()
{
  syslog(LOG_INFO, "NfsConnectionGroup destructor %s\n", getServerIpStr());

  // close syslog
  closelog();

  for (int i = 0; i < MAX_SERVICE; ++i)
    for (int j = 0; j < MAX_TRANSP; ++j)
      RpcConnection::clear(m_connections[i][j]);

  if (m_initialClientVerifier)
  {
    free(m_initialClientVerifier);
    m_initialClientVerifier = NULL;
  }
  if (m_ClientVerifier)
  {
    free(m_ClientVerifier);
    m_ClientVerifier = NULL;
  }
}

bool NfsConnectionGroup::update()
{
  return initNfs();
}


bool NfsConnectionGroup::initNfs()
{
  MutexGuard lock(m_connMutex);
  srandom(time(NULL)); // set seed to generate random initial xid

  TransportType transp = gTransportConfig;

  if ( MAX_TRANSP == transp )
    transp = TRANSP_TCP;

  if ( m_nfsTransp != transp )
    m_nfsTransp = transp;

  bool bForceRecreate = false;

  if (m_nfsVersion == NFSV3)
  {
    if ( !ensurePortMapperConnection(/*out*/ bForceRecreate) )
      return false;

    // Update port mapper
    if ( !updateRpcPorts(m_nfsTransp) )
    {
      syslog(LOG_ERR, "NfsConnectionGroup::initNfs failed to updateRpcPorts for server %s\n", getServerIpStr());
      return false;
    }

    // mount version 3
    initRpcService(MOUNT, 3, m_nfsTransp, bForceRecreate);

    // NFS v3
    initRpcService(NFS, 3, m_nfsTransp, bForceRecreate);

    // NLM v4
    initRpcService(NLM, 4, m_nfsTransp, bForceRecreate);
  }
  else if (m_nfsVersion == NFSV4)
  {
    // NFS v4
    initRpcService(NFS, 4, m_nfsTransp, bForceRecreate);
  }
  else
    throw std::string("Unknown NFS version");

  return true;
}

bool NfsConnectionGroup::initRpcService(ServiceType progType, uint32 version, TransportType transpType, const bool bReset)
{
  if ( version > 4 || version < 1 )
    return false;

  BasicConnectionPtr conn = m_connections[progType][transpType];
  if ( !conn.empty() )
  {
    if ( !bReset && conn->getConnKey().getServerPort() == m_rpcPorts[progType][version-1][transpType]
         && conn->isAlive() )
      return true;

    RpcConnection::clear(m_connections[progType][transpType]);
  }

  ConnKey portmapKey(m_serverIP, m_rpcPorts[progType][version-1][transpType], transpType);
  bool useResvPort = false;
  if ( progType == MOUNT )
    useResvPort = true;

  uint32 progNumber = 0;
  switch ( progType )
  {
  case MOUNT: progNumber = RPCPROG_MOUNT; break;
  case NFS:   progNumber = RPCPROG_NFS;   break; //RPCPROG_NFS is same as NFS4_PROGRAM
  case NLM:   progNumber = RPCPROG_NLM;   break;
  default:    break;
  }

  // If the port number is 0, do not attempt to create it
  // Found this behavior with Data Domain for NLM
  if ( portmapKey.getServerPort() == 0 )
  {
    std::ostringstream out;
    switch ( progNumber )
    {
    case RPCPROG_NFS:   out << "NFS";   break;
    case RPCPROG_MOUNT: out << "MOUNT"; break;
    case RPCPROG_NLM:   out << "NLM";   break;
    }
    out << "_V" << version << "@";

    switch ( portmapKey.getTransport() )
    {
    case TRANSP_UDP: out << "UDP"; break;
    case TRANSP_TCP: out << "TCP"; break;
    default: break;
    }
    out << ":" << portmapKey.getServerPort();
    syslog(LOG_DEBUG, "NfsConnectionGroup::%s: Not creating connection for %s as it is not availables\n",
           __func__, out.str().c_str());
    return false;
  }

  m_connections[progType][transpType] = RpcConnection::create(portmapKey, progNumber, version, useResvPort);

  return true;
}

bool NfsConnectionGroup::updateRpcPorts(TransportType transp)
{
  Portmap::DumpCall dumpCall;
  enum clnt_stat callState = dumpCall.call(this, 30);
  if ( callState != RPC_SUCCESS )
  {
    syslog(LOG_ERR, "NfsConnectionGroup::%s: Port mapper DumpCall failed with error %d\n", __func__, callState);
    return false;
  }

  Portmap::PortmapDumpData* dumplist = NULL;
  const Portmap::PortmapDumpList& dlist = dumpCall.getResult();
  dumplist = dlist.data;

  // Declare local RPC port array and initialize it to 0
  uint16 rpcPorts[MAX_SERVICE][4][MAX_TRANSP];
  memset(rpcPorts, 0, sizeof(rpcPorts));

  std::ostringstream out;
  out << getServerIpStr() << ":";
  for ( int i = 0; i < dlist.len; i++ )
  {
    ServiceType progType;
    switch ( dumplist[i].program )
    {
    case RPCPROG_NFS:   progType = NFS;   out << " NFS";   break;
    case RPCPROG_MOUNT: progType = MOUNT; out << " MOUNT"; break;
    case RPCPROG_NLM:   progType = NLM;   out << " NLM";   break;
    default: continue;
    }
    out << "_V" << dumplist[i].version << "@";

    TransportType transpType;
    switch ( dumplist[i].transp )
    {
    case IPPROTO_UDP: transpType = TRANSP_UDP; out << "UDP"; break;
    case IPPROTO_TCP: transpType = TRANSP_TCP; out << "TCP"; break;
    default: continue;
    }
    out << ":" << dumplist[i].port;

    uint32 version = dumplist[i].version;
    if ( version > 4 || version < 1 )
      continue;

    // set RPC port array
    rpcPorts[progType][version-1][transpType] = dumplist[i].port;
  }

  // Copy local port array to member port array
  memcpy(m_rpcPorts, rpcPorts, sizeof(rpcPorts));

  syslog(LOG_DEBUG, "NfsConnectionGroup::%s: %s\n", __func__, out.str().c_str());

  return true;
}

bool NfsConnectionGroup::ensurePortMapperConnection(bool& bForceRecreate)
{
  TransportType transp = m_nfsTransp;
  bForceRecreate = false;

  if ( !m_connections[PORTMAP][transp].empty() )
  {
    bool isAlive = m_connections[PORTMAP][transp]->isAlive();
    syslog(LOG_DEBUG, "NfsConnectionGroup::%s: Connection to %s is %s\n",
           __func__, m_connections[PORTMAP][transp]->toString().c_str(), isAlive? "ALIVE":"DEAD");
    if ( !isAlive )
    {
      syslog(LOG_DEBUG, "NfsConnectionGroup::%s: Force recreating RPC connections as connection to %s is DEAD.\n",
             __func__, m_connections[PORTMAP][transp]->toString().c_str());
      RpcConnection::clear(m_connections[PORTMAP][transp]);
    }
  }

  if ( m_connections[PORTMAP][transp].empty() )
  {
    bForceRecreate = true;
    ConnKey portmapKey(m_serverIP, portmap_port, transp);
    m_connections[PORTMAP][transp] = RpcConnection::create(portmapKey, RPCPROG_PMAP, 2);
  }

  if ( m_connections[PORTMAP][transp].empty() )
    syslog(LOG_ERR, "NfsConnectionGroup::%s: Creation of RPC connection failed for server %s.\n", __func__, getServerIpStr());

  return !m_connections[PORTMAP][transp].empty();
}

bool NfsConnectionGroup::ensureConnection()
{
  MutexGuard lock(m_connMutex);

  bool bForceRecreate = false;

  if (m_nfsVersion == NFSV3)
  {
    if ( !ensurePortMapperConnection(/*out*/ bForceRecreate) )
      return false;

    // Update port mapper
    if ( !updateRpcPorts(m_nfsTransp) )
    {
      syslog(LOG_ERR, "NfsConnectionGroup::%s: failed to updateRpcPorts for server %s\n", __func__, getServerIpStr());
      return false;
    }

    // mount version 3
    initRpcService(MOUNT, 3, m_nfsTransp, bForceRecreate);

    // NFS v3
    initRpcService(NFS, 3, m_nfsTransp, bForceRecreate);

    // NLM v4
    initRpcService(NLM, 4, m_nfsTransp, bForceRecreate);
  }
  else if (m_nfsVersion == NFSV4)
  {
    // NFS v4
    initRpcService(NFS, 4, m_nfsTransp, bForceRecreate);
  }

  return true;
}

NfsConnectionGroupPtr NfsConnectionGroup::findNfsConnectionGroup(std::string serverIp)
{
  NfsConnectionGroupPtr svr;

  MutexGuard lock(serverTableMutex);
  std::map<std::string, NfsConnectionGroupPtr>::iterator iter = serverTable.find(serverIp);
  if ( iter != serverTable.end() )
    svr = iter->second;

  return svr;
}

NfsConnectionGroupPtr NfsConnectionGroup::create(std::string serverIp, TransportType proto, NFSVersion version)
{
  NfsConnectionGroupPtr svr;
  bool bNewConnection = false;
  {
    MutexGuard lock(serverTableMutex);

    std::map<std::string, NfsConnectionGroupPtr>::iterator iter = serverTable.find(serverIp);
    if ( iter != serverTable.end() )
    {
      svr = iter->second;
      if ( svr.empty() )
        serverTable.erase(iter);
    }

    if ( svr.empty() )
    {
      svr = new NfsConnectionGroup(serverIp, version);
      serverTable[serverIp] = svr;
      bNewConnection = true;
    }
  }

  if ( bNewConnection )
    svr->initNfs();

  return svr;
}

NfsConnectionGroupPtr NfsConnectionGroup::create(const char* serverIpStr, bool useUdp, NFSVersion version)
{
  NfsConnectionGroupPtr svr;

  TransportType transp = useUdp ? TRANSP_UDP : TRANSP_TCP;
  std::string srvIP=serverIpStr;
  svr = create(srvIP, transp, version);

  return svr;
}


bool NfsConnectionGroup::addNfsConnectionGroup(std::string  serverIp, TransportType proto)
{
  NfsConnectionGroupPtr svr;
  {
    MutexGuard lock(serverTableMutex);

    std::map<std::string, NfsConnectionGroupPtr>::iterator iter = serverTable.find(serverIp);
    if ( iter != serverTable.end() )
    {
      svr = iter->second;
      if ( !svr.empty() )
        return true;
    }

    svr = new NfsConnectionGroup(serverIp);
    serverTable[serverIp] = svr;
  }

  if ( !svr.empty() )
    svr->initNfs();

  return !svr.empty();
}

bool NfsConnectionGroup::removeFromPool()
{
  MutexGuard lock(serverTableMutex);
  std::map<std::string, NfsConnectionGroupPtr>::iterator it = serverTable.find(m_serverIP);
  if ( it != serverTable.end() )
  {
    int refCount = it->second.refCount();
    if ( it->second.refCount() > 2 )
    {
      syslog(LOG_DEBUG, "NfsConnectionGroup::%s: RefCount=%d. Not removing from the pool.\n", __func__, refCount);
      return false;
    }
    serverTable.erase(it);
    syslog(LOG_DEBUG, "NfsConnectionGroup::%s: Removed from pool\n", __func__);
  }
  return true;
}

void NfsConnectionGroup::init()
{
}

void NfsConnectionGroup::fini()
{
  MutexGuard lock(serverTableMutex);
  std::map<std::string, NfsConnectionGroupPtr>::iterator iter = serverTable.begin();
  while ( iter != serverTable.end() )
  {
    NfsConnectionGroupPtr svr = iter->second;
    serverTable.erase(iter);
    iter = serverTable.begin();
    syslog(LOG_DEBUG, "NfsConnectionGroup::%s: remove server %s\n", __func__, svr->getServerIpStr());
    svr.clear();
  }
}

bool NfsConnectionGroup::setLogLevel(unsigned int level)
{
  if (level < LOG_EMERG || level > LOG_DEBUG)
    return false;
  setlogmask(LOG_UPTO(level));
}

uint32_t NfsConnectionGroup::getFileOPSeqId()
{
  std::lock_guard<std::mutex> guard(m_seqid_mutex);
  uint32_t seqid = m_file_op_seqid;
  ++m_file_op_seqid;
  return seqid;
}

uint32_t NfsConnectionGroup::getFileLockSeqId()
{
  std::lock_guard<std::mutex> guard(m_seqid_mutex);
  uint32_t seqid = m_file_lock_seqid;
  ++m_file_lock_seqid;
  return seqid;
}

bool NfsConnectionGroup::connect(std::string serverIP, NfsError &status)
{
  return m_NfsApiHandle->connect(serverIP, status);
}

bool NfsConnectionGroup::getRootFH(const std::string &nfs_export, NfsFh &rootFh, NfsError &status)
{
  return m_NfsApiHandle->getRootFH(nfs_export, rootFh, status);
}

bool NfsConnectionGroup::getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH, NfsError &status)
{
  return m_NfsApiHandle->getDirFh(rootFH, dirPath, dirFH, status);
}

bool NfsConnectionGroup::getDirFh(const std::string &dirPath, NfsFh &dirFH, NfsError &status)
{
  return m_NfsApiHandle->getDirFh(dirPath, dirFH, status);
}

bool NfsConnectionGroup::getFileHandle(NfsFh &rootFH, const std::string path, NfsFh &fileFh, NfsAttr &attr, NfsError &status)
{
  return m_NfsApiHandle->getFileHandle(rootFH, path, fileFh, attr, status);
}

bool NfsConnectionGroup::create(NfsFh &dirFh, std::string &fileName, NfsAttr *inAttr, NfsFh &fileFh, NfsAttr &outAttr, NfsError &status)
{
  return m_NfsApiHandle->create(dirFh, fileName, inAttr, fileFh, outAttr, status);
}

bool NfsConnectionGroup::open(const std::string filePath,
                              uint32_t          access,
                              uint32_t          shareAccess,
                              uint32_t          shareDeny,
                              NfsFh             &fileFh,
                              NfsError          &status)
{
  return m_NfsApiHandle->open(filePath, access, shareAccess, shareDeny, fileFh, status);
}

bool NfsConnectionGroup::read(NfsFh        &fileFH,
                              uint64_t     offset,
                              uint32_t     length,
                              std::string  &data,
                              uint32_t     &bytesRead,
                              bool         &eof,
                              NfsAttr      &postAttr,
                              NfsError     &status)
{
  return m_NfsApiHandle->read(fileFH, offset, length, data, bytesRead, eof, postAttr, status);
}

bool NfsConnectionGroup::write(NfsFh       &fileFH,
                               uint64_t     offset,
                               uint32_t     length,
                               std::string &data,
                               uint32_t    &bytesWritten,
                               NfsError    &status)
{
  return m_NfsApiHandle->write(fileFH, offset, length, data, bytesWritten, status);
}

bool NfsConnectionGroup::write_unstable(NfsFh       &fileFH,
                                        uint64_t     offset,
                                        std::string &data,
                                        uint32_t    &bytesWritten,
                                        char        *verf,
                                        const bool   needverify,
                                        NfsError     &status)
{
  return m_NfsApiHandle->write_unstable(fileFH, offset, data, bytesWritten, verf, needverify, status);
}

bool NfsConnectionGroup::close(NfsFh &fileFH, NfsAttr &postAttr, NfsError &status)
{
  return m_NfsApiHandle->close(fileFH, postAttr, status);
}

bool NfsConnectionGroup::remove(std::string path, NfsError &status)
{
  return m_NfsApiHandle->remove(path, status);
}

bool NfsConnectionGroup::remove(const NfsFh &parentFH, const string &name, NfsError &status)
{
  return m_NfsApiHandle->remove(parentFH, name, status);
}

bool NfsConnectionGroup::rename(NfsFh &fromDirFh,
                                const std::string &fromName,
                                NfsFh &toDirFh,
                                const std::string toName,
                                NfsError &status)
{
  return m_NfsApiHandle->rename(fromDirFh, fromName, toDirFh, toName, status);
}

bool NfsConnectionGroup::rename(const std::string &nfs_export,
                                const std::string &fromPath,
                                const std::string &toPath,
                                NfsError          &status)
{
  return m_NfsApiHandle->rename(nfs_export, fromPath, toPath, status);
}

bool NfsConnectionGroup::readDir(const std::string &dirPath, NfsFiles &files, NfsError &status)
{
  return m_NfsApiHandle->readDir(dirPath, files, status);
}

bool NfsConnectionGroup::readDir(NfsFh &dirFh, NfsFiles &files, NfsError &status)
{
  return m_NfsApiHandle->readDir(dirFh, files, status);
}

bool NfsConnectionGroup::truncate(NfsFh &fh, uint64_t size, NfsError &status)
{
  return m_NfsApiHandle->truncate(fh, size, status);
}

bool NfsConnectionGroup::truncate(const std::string &path, uint64_t size, NfsError &status)
{
  return m_NfsApiHandle->truncate(path, size, status);
}

bool NfsConnectionGroup::access(const std::string &path, uint32_t accessRequested, NfsAccess &acc, NfsError &status)
{
  return m_NfsApiHandle->access(path, accessRequested, acc, status);
}

bool NfsConnectionGroup::mkdir(const NfsFh &parentFH, const std::string dirName, uint32_t mode, NfsFh &dirFH, NfsError &status)
{
  return m_NfsApiHandle->mkdir(parentFH, dirName, mode, dirFH, status);
}

bool NfsConnectionGroup::mkdir(const std::string &path, uint32_t mode, NfsError &status, bool createPath)
{
  return m_NfsApiHandle->mkdir(path, mode, status, createPath);
}

bool NfsConnectionGroup::rmdir(const std::string &path, NfsError &status)
{
  return m_NfsApiHandle->rmdir(path, status);
}

bool NfsConnectionGroup::rmdir(const NfsFh &parentFH, const string &name, NfsError &status)
{
  return m_NfsApiHandle->rmdir(parentFH, name, status);
}

bool NfsConnectionGroup::commit(NfsFh &fh, uint64_t offset, uint32_t bytes, char *writeverf, NfsError &status)
{
  return m_NfsApiHandle->commit(fh, offset, bytes, writeverf, status);
}

bool NfsConnectionGroup::lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status, bool reclaim)
{
  return m_NfsApiHandle->lock(fh, lockType, offset, length, status, reclaim);
}

bool NfsConnectionGroup::unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status)
{
  return m_NfsApiHandle->unlock(fh, lockType, offset, length, status);
}

bool NfsConnectionGroup::setattr( NfsFh &fh, NfsAttr &attr, NfsError &status)
{
  return m_NfsApiHandle->setattr(fh, attr, status);
}

bool NfsConnectionGroup::getAttr(NfsFh &fh, NfsAttr &attr, NfsError &status)
{
  return m_NfsApiHandle->getAttr(fh, attr, status);
}

bool NfsConnectionGroup::lookup(const std::string &path, NfsFh &lookup_fh, NfsError &status)
{
  return m_NfsApiHandle->lookup(path, lookup_fh, status);
}

bool NfsConnectionGroup::lookup(NfsFh &dirFh, const std::string &file, NfsFh &lookup_fh, NfsAttr &attr, NfsError &status)
{
  return m_NfsApiHandle->lookup(dirFh, file, lookup_fh, attr, status);
}

bool NfsConnectionGroup::fsstat(NfsFh &rootFh, NfsFsStat &stat, uint32 &invarSec, NfsError &status)
{
  return m_NfsApiHandle->fsstat(rootFh, stat, invarSec, status);
}

} //end of namespace
