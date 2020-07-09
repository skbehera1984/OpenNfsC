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

#include <Nfs3ApiHandle.h>

using namespace OpenNfsC;

Nfs3ApiHandle::Nfs3ApiHandle(NfsConnectionGroup *ptr) : NfsApiHandle(ptr)
{
}

bool Nfs3ApiHandle::connect(std::string &serverIP)
{
  return true;
}

bool Nfs3ApiHandle::getRootFH(const std::string &nfs_export)
{
  return true;
}

bool Nfs3ApiHandle::getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH)
{
  return true;
}

bool Nfs3ApiHandle::getDirFh(const std::string &dirPath, NfsFh &dirFH)
{
  return true;
}

bool Nfs3ApiHandle::open(const std::string filePath,
                         uint32_t          access,
                         uint32_t          shareAccess,
                         uint32_t          shareDeny,
                         NfsFh             &file)
{
  return true;
}

bool Nfs3ApiHandle::read(NfsFh       &fileFH,
                         uint64_t     offset,
                         uint32_t     length,
                         std::string &data,
                         uint32_t    &bytesRead,
                         bool        &eof)
{
  return true;
}

bool Nfs3ApiHandle::write(NfsFh       &fileFH,
                          uint64_t     offset,
                          std::string &data,
                          uint32_t    &bytesWritten,
                          NfsAttr     &postAttr)
{
  return true;
}

bool Nfs3ApiHandle::write_unstable(NfsFh       &fileFH,
                                   uint64_t     offset,
                                   std::string &data,
                                   uint32_t    &bytesWritten,
                                   NfsAttr     &postAttr)
{
  return true;
}

bool Nfs3ApiHandle::close(NfsFh &fileFh, NfsAttr &postAttr)
{
  return true;
}

bool Nfs3ApiHandle::remove(std::string path)
{
  return true;
}

bool Nfs3ApiHandle::rename(const std::string &nfs_export,
                           const std::string &fromPath,
                           const std::string &toPath)
{
  return true;
}

bool Nfs3ApiHandle::readDir(const std::string &dirPath, NfsFiles &files)
{
  return true;
}

bool Nfs3ApiHandle::truncate(NfsFh &fh, uint64_t size)
{
  return true;
}

bool Nfs3ApiHandle::truncate(const std::string &path, uint64_t size)
{
  return true;
}

bool Nfs3ApiHandle::access(const std::string &filePath, uint32_t accessRequested, NfsAccess &acc)
{
  return true;
}

bool Nfs3ApiHandle::mkdir(const NfsFh &parentFH, const std::string dirName, uint32_t mode)
{
  return true;
}

bool Nfs3ApiHandle::mkdir(const std::string &path, uint32_t mode, bool createPath)
{
  return true;
}

bool Nfs3ApiHandle::rmdir(const std::string &path)
{
  return true;
}

bool Nfs3ApiHandle::commit(NfsFh &fh, uint64_t offset, uint32_t bytes, char *writeverf)
{
  return true;
}

bool Nfs3ApiHandle::lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, bool reclaim)
{
  return true;
}

bool Nfs3ApiHandle::unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length)
{
  return true;
}

bool Nfs3ApiHandle::setattr( NfsFh &fh, NfsAttr &attr)
{
  return true;
}
