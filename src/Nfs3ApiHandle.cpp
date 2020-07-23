#include "NfsConnectionGroup.h"
#include "RpcConnection.h"
#include "ConnectionMgr.h"
#include "Packet.h"
#include "RpcPacket.h"
#include "Portmap.h"
#include "RpcDefs.h"
#include "NfsUtil.h"
#include "NfsCall.h"

#include <nfsrpc/pmap.h>
#include <nfsrpc/nfs.h>
#include <nfsrpc/nlm.h>
#include <nfsrpc/mount.h>

#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>

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
                                   char        *verf,
                                   const bool   needverify)
{
  WRITE3args writeArg = {};

  writeArg.write3_file.fh3_data.fh3_data_len = fileFH.getLength();
  writeArg.write3_file.fh3_data.fh3_data_val = (char*)fileFH.getData();
  writeArg.write3_offset                     = (u_quad_t)offset;
  writeArg.write3_count                      = data.length();
  writeArg.write3_stable                     = STABLE_UNSTABLE;
  writeArg.write3_data.write3_data_len       = data.length();
  writeArg.write3_data.write3_data_val       = (char*)data.c_str();

  NFSv3::WriteCall nfsWriteCall(writeArg);
  enum clnt_stat writeRet = nfsWriteCall.call(m_pConn);
  if (writeRet != RPC_SUCCESS)
  {
    return false;
  }

  WRITE3res &res = nfsWriteCall.getResult();
  if (res.status != NFS3_OK)
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s: nfs_v3_write error: %d\n", __func__, res.status);
    return false;
  }

  bytesWritten = res.WRITE3res_u.write3ok.write3_count_res;

  if(verf && needverify)
  {
    memset(verf, 0, NFS3_WRITEVERFSIZE);
    memcpy(verf, res.WRITE3res_u.write3ok.write3_verf, NFS3_WRITEVERFSIZE);
  }

  return true;
}

bool Nfs3ApiHandle::close(NfsFh &fileFh, NfsAttr &postAttr)
{
  return true;
}

bool Nfs3ApiHandle::remove(std::string path, NfsError &status)
{
  return true;
}

bool Nfs3ApiHandle::remove(const NfsFh &parentFH, const string &name, NfsError &status)
{
  REMOVE3args removeArgs = {};

  removeArgs.remove3_object.dirop3_dir.fh3_data.fh3_data_len = parentFH.getLength();
  removeArgs.remove3_object.dirop3_dir.fh3_data.fh3_data_val = (char*)parentFH.getData();
  removeArgs.remove3_object.dirop3_name = (char*)name.c_str();

  NFSv3::RemoveCall nfsRemoveCall(removeArgs);
  enum clnt_stat ret = nfsRemoveCall.call(m_pConn);
  if ( ret != RPC_SUCCESS )
  {
    return false;
  }

  REMOVE3res &res = nfsRemoveCall.getResult();

  if ( res.status != NFS3_OK)
  {
    status.setError3(res.status, string("NFSV3 remove Failed"));
    syslog(LOG_ERR, "Nfs3ApiHandle::remove(): nfs_v3_remove error: %d  <%s>\n", res.status, name.c_str());
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::rename(NfsFh &fromDirFh,
                           const std::string &fromName,
                           NfsFh &toDirFh,
                           const std::string toName,
                           NfsError &sts)
{
  RENAME3args renameArg = {};

  renameArg.rename3_from.dirop3_dir.fh3_data.fh3_data_len = fromDirFh.getLength();
  renameArg.rename3_from.dirop3_dir.fh3_data.fh3_data_val = (char*)fromDirFh.getData();
  string sFromName = fromName;
  renameArg.rename3_from.dirop3_name = (char*)sFromName.c_str();
  renameArg.rename3_to.dirop3_dir.fh3_data.fh3_data_len = toDirFh.getLength();
  renameArg.rename3_to.dirop3_dir.fh3_data.fh3_data_val = (char*)toDirFh.getData();
  string sToName = toName;
  renameArg.rename3_to.dirop3_name = (char*)sToName.c_str();

  NFSv3::RenameCall nfsRenameCall(renameArg);
  enum clnt_stat ret = nfsRenameCall.call(m_pConn);
  if ( ret != RPC_SUCCESS )
  {
    return false;
  }

  RENAME3res &res = nfsRenameCall.getResult();

  if (res.status != NFS3_OK)
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::rename(): error: %d <%s,%s>\n", res.status, fromName.c_str(), toName.c_str());
    return false;
  }

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

bool Nfs3ApiHandle::mkdir(const NfsFh &parentFH,
                          const std::string dirName,
                          uint32_t mode,
                          NfsFh &dirFH)
{
  MKDIR3args mkdirArgs = {};
  MKDIR3res  mkdirRes  = {};

  mkdirArgs.mkdir3_where.dirop3_dir.fh3_data.fh3_data_len = parentFH.getLength();
  mkdirArgs.mkdir3_where.dirop3_dir.fh3_data.fh3_data_val = (char *)parentFH.getData();
  mkdirArgs.mkdir3_attributes.sattr3_mode.set_it             = true;
  mkdirArgs.mkdir3_attributes.sattr3_mode.set_mode3_u.mode   = 0700;
  string tName = dirName;
  mkdirArgs.mkdir3_where.dirop3_name =  (char *) tName.c_str();

  NFSv3::MkdirCall nfsMkdirCall(mkdirArgs);
  enum clnt_stat mkdirRet = nfsMkdirCall.call(m_pConn);
  if ( mkdirRet != RPC_SUCCESS )
  {
    return false;
  }

  if (mkdirRes.status != NFS3_OK)
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::mkdir(): nfs_v3_mkdir error: %d  <%s>\n", mkdirRes.status, dirName.c_str());
    return false;
  }

  // created the directory, but the file handle may not have been return in the result,
  // so we'll call NFS LOOKUP() to get the file handle of the dir we just created
  LOOKUP3args lookupArgs = {};
  lookupArgs.lookup3_what.dirop3_dir.fh3_data.fh3_data_len = parentFH.getLength();
  lookupArgs.lookup3_what.dirop3_dir.fh3_data.fh3_data_val = (char *)parentFH.getData();
  lookupArgs.lookup3_what.dirop3_name = (char *)tName.c_str();

  NFSv3::LookUpCall nfsLookupCall(lookupArgs);
  enum clnt_stat lookupRet = nfsLookupCall.call(m_pConn);
  if ( lookupRet != RPC_SUCCESS )
  {
    return false;
  }

  LOOKUP3res &lookupRes = nfsLookupCall.getResult();
  if ( lookupRes.status != NFS3_OK)
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::mkdir(): nfs_v3_lookup error: %d  <%s>\n", lookupRes.status, dirName.c_str());
    return false;
  }

  nfs_fh3 *fh3 = &(lookupRes.LOOKUP3res_u.lookup3ok.lookup3_object);
  NfsFh fh(fh3->fh3_data.fh3_data_len, fh3->fh3_data.fh3_data_val);
  dirFH = fh;

  return true;
}

bool Nfs3ApiHandle::mkdir(const std::string &path, uint32_t mode, bool createPath)
{
  return true;
}

bool Nfs3ApiHandle::rmdir(const std::string &path, NfsError &status)
{
  return true;
}

bool Nfs3ApiHandle::rmdir(const NfsFh &parentFH, const string &name, NfsError &status)
{
  RMDIR3args rmdirArg = {};

  rmdirArg.rmdir3_object.dirop3_dir.fh3_data.fh3_data_len = parentFH.getLength();
  rmdirArg.rmdir3_object.dirop3_dir.fh3_data.fh3_data_val = (char*)parentFH.getData();
  string tName = name;
  rmdirArg.rmdir3_object.dirop3_name =  (char *) tName.c_str();

  NFSv3::RmdirCall nfsRmdirCall(rmdirArg);
  enum clnt_stat ret = nfsRmdirCall.call(m_pConn);
  if ( ret != RPC_SUCCESS )
  {
    return false;
  }

  RMDIR3res &res = nfsRmdirCall.getResult();

  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, string("NFSV3 rmdir Failed"));
    syslog(LOG_ERR, "Nfs3ApiHandle::rmdir(): nfs_v3_rmdir error: %d  <%s>\n", res.status, name.c_str());
    return false;
  }

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

bool Nfs3ApiHandle::lookup(const std::string &path, NfsFh &lookup_fh)
{
  return true;
}
