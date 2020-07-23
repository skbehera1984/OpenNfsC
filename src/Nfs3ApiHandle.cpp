#include "NfsConnectionGroup.h"
#include "RpcConnection.h"
#include "ConnectionMgr.h"
#include "Packet.h"
#include "RpcPacket.h"
#include "Portmap.h"
#include "RpcDefs.h"
#include "NfsUtil.h"
#include "NfsCall.h"
#include "Stringf.h"

#include <nfsrpc/pmap.h>
#include <nfsrpc/nfs.h>
#include <nfsrpc/nlm.h>
#include <nfsrpc/mount.h>
#include <NlmCall.h>

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
                         bool        &eof,
                         NfsAttr     &postAttr,
                         NfsError    &err)
{
  READ3args readArg = {};

  readArg.read3_file.fh3_data.fh3_data_len = fileFH.getLength();
  readArg.read3_file.fh3_data.fh3_data_val = (char*)fileFH.getData();
  readArg.read3_offset                     = offset;
  readArg.read3_count                      = length;

  NFSv3::ReadCall nfsReadCall(readArg);
  enum clnt_stat readRet = nfsReadCall.call(m_pConn);
  if (readRet != RPC_SUCCESS)
  {
    return false;
  }

  READ3res &res = nfsReadCall.getResult();
  if (res.status != NFS3_OK)
  {
    err.setError3(res.status, "nfs_v3_read failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::read(): nfs_v3_read error: %d\n", res.status);
    return false;
  }

  // copy the read results
  bytesRead = res.READ3res_u.read3ok.read3_count_res;
  data = std::string(res.READ3res_u.read3ok.read3_data.read3_data_val,
                     res.READ3res_u.read3ok.read3_data.read3_data_len);

  // return the EOF flag
  eof = res.READ3res_u.read3ok.read3_eof;

  // return the post op file attrs
  if (res.READ3res_u.read3ok.read3_file_attributes.attributes_follow)
  {
    postAttr.v3Attr = res.READ3res_u.read3ok.read3_file_attributes.post_op_attr_u.post_op_attr;
  }
  else
  {
    syslog(LOG_DEBUG, "Nfs3ApiHandle::%s() failed no post op attrs returned\n", __func__);
    memset(&postAttr.v3Attr, 0, sizeof(fattr3));
  }

  return true;
}

bool Nfs3ApiHandle::write(NfsFh       &fileFH,
                          uint64_t     offset,
                          uint32_t     length,
                          std::string &data,
                          uint32_t    &bytesWritten,
                          NfsError    &err)
{
  WRITE3args writeArg = {};

  writeArg.write3_file.fh3_data.fh3_data_len = fileFH.getLength();
  writeArg.write3_file.fh3_data.fh3_data_val = (char*)fileFH.getData();
  writeArg.write3_offset                     = (u_quad_t)offset;
  writeArg.write3_count                      = length;
  writeArg.write3_stable                     = STABLE_DATA_SYNC;
  writeArg.write3_data.write3_data_len       = length;
  writeArg.write3_data.write3_data_val       = (char*)data.c_str();

  NFSv3::WriteCall nfsWriteCall(writeArg);
  enum clnt_stat ret = nfsWriteCall.call(m_pConn);
  if ( ret != RPC_SUCCESS )
  {
    //rtNfsErrorMsg = NFS3ERR_SERVERFAULT;
    return false;
  }

  WRITE3res &writeRes = nfsWriteCall.getResult();
  if (writeRes.status != NFS3_OK)
  {
    err.setError3(writeRes.status, "nfs_v3_write failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::write(): nfs_v3_write error: %d\n", writeRes.status);
    return false;
  }

  bytesWritten = writeRes.WRITE3res_u.write3ok.write3_count_res;

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
  COMMIT3args commitArg = {};

  commitArg.commit3_file.fh3_data.fh3_data_len = fh.getLength();
  commitArg.commit3_file.fh3_data.fh3_data_val = (char *) fh.getData();
  commitArg.commit3_offset  = (u_quad_t)offset;
  commitArg.commit3_count   = bytes;

  NFSv3::CommitCall nfsCommitCall(commitArg);
  enum clnt_stat commitRet = nfsCommitCall.call(m_pConn);
  if (commitRet != RPC_SUCCESS)
  {
    //rtNfsErrorMsg = NFS3ERR_SERVERFAULT;
    return false;
  }

  COMMIT3res &res = nfsCommitCall.getResult();
  //rtNfsErrorMsg = res.status;
  if (res.status != NFS3_OK)
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s: nfs_v3_commit error: %d\n", __func__, res.status);
    return false;
  }

  memset(writeverf, 0, NFS3_WRITEVERFSIZE);
  memcpy(writeverf, res.COMMIT3res_u.commit3ok.commit3_verf, NFS3_WRITEVERFSIZE);

  return true;
}

bool Nfs3ApiHandle::lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, bool reclaim)
{
  nlm4_lockargs lockArg = {};

  string cookie("No cookie required");
  lockArg.nlm4_lockargs_cookie.n_len = cookie.size();
  lockArg.nlm4_lockargs_cookie.n_bytes = (char*)cookie.c_str();
  lockArg.nlm4_lockargs_block = false;
  lockArg.nlm4_lockargs_exclusive = true;
  string callerName = m_pConn->getIP();
  lockArg.nlm4_lockargs_alock.nlm4_lock_caller_name = (char*) callerName.c_str();
  lockArg.nlm4_lockargs_alock.nlm4_lock_fh.n_len = fh.getLength();
  lockArg.nlm4_lockargs_alock.nlm4_lock_fh.n_bytes = (char*) fh.getData();
  string lockOwner = stringf("%s:%ld", callerName.c_str(), pthread_self());
  lockArg.nlm4_lockargs_alock.nlm4_lock_oh.n_len = lockOwner.size();
  lockArg.nlm4_lockargs_alock.nlm4_lock_oh.n_bytes = (char*)lockOwner.c_str();
  lockArg.nlm4_lockargs_alock.nlm4_lock_svid = pthread_self();
  lockArg.nlm4_lockargs_alock.nlm4_lock_l_offset = offset;
  lockArg.nlm4_lockargs_alock.nlm4_lock_l_len = length;
  lockArg.nlm4_lockargs_reclaim = reclaim;
  lockArg.nlm4_lockargs_state = 0;

  NLMv4::LockCall nlmLockCall(lockArg);
  enum clnt_stat lockRet = nlmLockCall.call(m_pConn);
  if (lockRet != RPC_SUCCESS)
  {
    return false;
  }

  nlm4_res &res = nlmLockCall.getResult();

  if ( res.nlm4_res_stat.nlm4_stat != NLMSTAT4_GRANTED )
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::nlm_nm_lock(): nlm_v4_nm_lock error: %d\n", res.nlm4_res_stat.nlm4_stat);
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length)
{
  nlm4_unlockargs unLkArg = {};

  string cookie("No cookie required");
  unLkArg.nlm4_unlockargs_cookie.n_len = cookie.size();
  unLkArg.nlm4_unlockargs_cookie.n_bytes = (char*)cookie.c_str();
  string callerName = m_pConn->getIP();
  unLkArg.nlm4_unlockargs_alock.nlm4_lock_caller_name = (char*)callerName.c_str();
  unLkArg.nlm4_unlockargs_alock.nlm4_lock_fh.n_len = fh.getLength();
  unLkArg.nlm4_unlockargs_alock.nlm4_lock_fh.n_bytes = (char*)fh.getData();
  string lockOwner = stringf("%s:%ld", m_pConn->getIP().c_str(), pthread_self());
  unLkArg.nlm4_unlockargs_alock.nlm4_lock_oh.n_len = lockOwner.size();
  unLkArg.nlm4_unlockargs_alock.nlm4_lock_oh.n_bytes = (char*)lockOwner.c_str();
  unLkArg.nlm4_unlockargs_alock.nlm4_lock_svid = pthread_self();
  unLkArg.nlm4_unlockargs_alock.nlm4_lock_l_offset = offset;
  unLkArg.nlm4_unlockargs_alock.nlm4_lock_l_len = length;

  NLMv4::UnlockCall nlmUnlockCall(unLkArg);
  enum clnt_stat tUnlockRet = nlmUnlockCall.call(m_pConn);
  if (tUnlockRet != RPC_SUCCESS)
  {
    return false;
  }

  nlm4_res &res = nlmUnlockCall.getResult();
  if (res.nlm4_res_stat.nlm4_stat != NLMSTAT4_GRANTED)
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::nlm_lock(): nlm_v4_unlock error: %d\n", res.nlm4_res_stat.nlm4_stat);
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::setattr(NfsFh &fh, NfsAttr &attr)
{
  return true;
}

bool Nfs3ApiHandle::getAttr(NfsFh &fh, NfsAttr &attr, NfsError &err)
{
  GETATTR3args getAttrArg = {};

  getAttrArg.getattr3_object.fh3_data.fh3_data_len = fh.getLength();
  getAttrArg.getattr3_object.fh3_data.fh3_data_val = (char*)fh.getData();

  NFSv3::GetAttrCall nfsGetattrCall(getAttrArg);
  enum clnt_stat getAttrRet = nfsGetattrCall.call(m_pConn);
  if (getAttrRet != RPC_SUCCESS)
  {
    return false;
  }

  GETATTR3res &res = nfsGetattrCall.getResult();
  if (res.status != NFS3_OK)
  {
    err.setError3(res.status, "nfs v3 getattr failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::getAttr(): nfs_v3_getattr error: %d\n", res.status);
    return false;
  }

  // copy the object attributes out of the result struct
  memcpy(&attr.v3Attr, &(res.GETATTR3res_u.getattr3ok.getattr3_obj_attributes), sizeof(fattr3));

  return true;
}

bool Nfs3ApiHandle::lookup(const std::string &path, NfsFh &lookup_fh)
{
  return true;
}
