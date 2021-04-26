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
#include "MountCall.h"

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

bool Nfs3ApiHandle::connect(std::string &serverIP, NfsError &status)
{
  return true;
}

bool Nfs3ApiHandle::getExports(list<string>& Exports)
{
  bool sts = false;

  Exports.clear();

  Mount::ExportCall tExportCall;
  enum clnt_stat tClientState = tExportCall.call(m_pConn);
  if ( tClientState == ::RPC_SUCCESS )
  {
    exports ExportList = tExportCall.getResult();
    while (ExportList != NULL)
    {
      string exPath(ExportList->ex_dir);
      Exports.push_back(exPath);
      ExportList = ExportList->ex_next;
    }
    sts = true;
  }
  else
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() - Failed to get exports from server '%s'\n", __func__, m_pConn->getServerIpStr());
  }
  return sts;
}

bool Nfs3ApiHandle::getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH, NfsError &status)
{
  if (dirPath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::getDirFh directory path empty");
    return false;
  }

  NfsFh currentFH;
  currentFH = rootFH;

  vector<string> segments;
  NfsUtil::splitNfsPath(dirPath, segments);

  for (string CurrSegment : segments)
  {
    LOOKUP3args lkpArg = {};
    lkpArg.lookup3_what.dirop3_dir.fh3_data.fh3_data_len = currentFH.getLength();
    lkpArg.lookup3_what.dirop3_dir.fh3_data.fh3_data_val = (char*)currentFH.getData();
    lkpArg.lookup3_what.dirop3_name = (char*)CurrSegment.c_str();

    NFSv3::LookUpCall nfsLookupCall(lkpArg);
    enum clnt_stat tLookupRet = nfsLookupCall.call(m_pConn);
    if (tLookupRet != RPC_SUCCESS)
    {
      status.setRpcError(tLookupRet, "NFS V3 lookup rpc error");
      return false;
    }

    LOOKUP3res &res = nfsLookupCall.getResult();
    if (res.status != NFS3_OK)
    {
      status.setError3(res.status, "NFS V3 lookup failed");
      if (res.status == NFS3ERR_NOENT)
      {
        syslog(LOG_DEBUG, "Nfs3ApiHandle::getDirFh(): nfs_v3_lookup didn't find the file %s %s\n", dirPath.c_str(), CurrSegment.c_str());
      }
      else
      {
        syslog(LOG_ERR, "Nfs3ApiHandle::getDirFh(): nfs_v3_lookup error: %d  <%s %s>\n", res.status, dirPath.c_str(), CurrSegment.c_str());
      }
      return false;
    }

    nfs_fh3 *fh3 = &(res.LOOKUP3res_u.lookup3ok.lookup3_object);
    NfsFh fh(fh3->fh3_data.fh3_data_len, fh3->fh3_data.fh3_data_val);
    currentFH = fh;
  }

  //currentFH has the file handle for the file we're looking for, save the value to return
  dirFH = currentFH;

  return true;
}

/* API to get the file handle to a path:-
 * path may be export or any absolute path to a directory or file
 * in nfs v3 we will get fh using mount
 */
bool Nfs3ApiHandle::getDirFh(const std::string &dirPath, NfsFh &dirFH, NfsError &status)
{
  if (dirPath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::getDirFh directory path is empty");
    return false;
  }

  enum clnt_stat retval;
  const char* pszPath = dirPath.c_str();

  dirpath pszMountPoint = (dirpath) pszPath;
  Mount::MntCall mountCall(pszMountPoint);
  retval = mountCall.call(m_pConn);
  if ( retval != RPC_SUCCESS )
  {
    status.setRpcError(retval, "Nfs3ApiHandle::getDirFh mount call rpc error");
    return false;
  }

  mountres3& mount_res = mountCall.getResult();
  if ( mount_res.fhs_status != MNT3_OK )
  {
    status.setMntError(mount_res.fhs_status, "Nfs3ApiHandle::getDirFh mount failed for - " + dirPath);
    return false;
  }

  // Copy the starting file handle
  nfs_fh3 *fh3 = &(mount_res.mountres3_u.mount3_mountinfo.mount3_fhandle);
  NfsFh fh(fh3->fh3_data.fh3_data_len, fh3->fh3_data.fh3_data_val);
  dirFH = fh;

  Mount::UMountCall umountCall(pszMountPoint);
  retval = umountCall.call(m_pConn);
  if ( retval != RPC_SUCCESS )
  {
    status.setRpcError(retval, "Nfs3ApiHandle::getDirFh umount call rpc error");
    return false;
  }

  // always returns success, because failure always throws an exception

  return true;
}

bool Nfs3ApiHandle::create(NfsFh             &dirFh,
                           std::string       &fileName,
                           NfsAttr           *inAttr,
                           NfsFh             &fileFh,
                           NfsAttr           &outAttr,
                           NfsError          &status)
{
  if (fileName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::create name can not be empty");
    return false;
  }

  CREATE3args createArg = {};

  createArg.create3_where.dirop3_dir.fh3_data.fh3_data_len = dirFh.getLength();
  createArg.create3_where.dirop3_dir.fh3_data.fh3_data_val = (char*)dirFh.getData();
  createArg.create3_where.dirop3_name = (char*)fileName.c_str();
  createArg.create3_how.mode = CREATE_GUARDED;

  if (inAttr)
  {
    sattr3 attr3;
    inAttr->NfsAttrToSattr3(&attr3);
    memcpy(&(createArg.create3_how.createhow3_u.create3_obj_attributes),
           &attr3, sizeof(sattr3));
  }
  else
  {
    // We have issues with Isilon if mode is not set
    memset(&(createArg.create3_how.createhow3_u.create3_obj_attributes),(int)0, sizeof(sattr3));
    createArg.create3_how.createhow3_u.create3_obj_attributes.sattr3_mode.set_it = 1;
    createArg.create3_how.createhow3_u.create3_obj_attributes.sattr3_mode.set_mode3_u.mode = 0777;
    //  createArg.create3_how.createhow3_u.create3_obj_attributes.sattr3_size.set_it = 1;
    //  createArg.create3_how.createhow3_u.create3_obj_attributes.sattr3_size.set_size3_u.size = 0;
  }

  NFSv3::CreateCall nfsCreateCall(createArg);
  enum clnt_stat createRet = nfsCreateCall.call(m_pConn);
  if (createRet != RPC_SUCCESS)
  {
    status.setRpcError(createRet, "Nfs3ApiHandle::create(): rpc error");
    return false;
  }

  CREATE3res &res = nfsCreateCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::create failed");
    if ( res.status != NFS3ERR_EXIST )
    {
      syslog(LOG_ERR, "Nfs3ApiHandle::create(): nfs_v3_create error: %d  <%s>\n", res.status, fileName.c_str());
    }
    return false;
  }

  // copy the CREATE3 results if they were returned
  if (res.CREATE3res_u.create3_ok.create3_obj_attributes.attributes_follow &&
      res.CREATE3res_u.create3_ok.create3_obj.handle_follows)
  {
    // return the fsid and fileid(inode) in the attr
    outAttr.Fattr3ToNfsAttr(&(res.CREATE3res_u.create3_ok.create3_obj_attributes.post_op_attr_u.post_op_attr));
    nfs_fh3 &fh3 = res.CREATE3res_u.create3_ok.create3_obj.post_op_fh3_u.post_op_fh3;
    NfsFh fh(fh3.fh3_data.fh3_data_len, fh3.fh3_data.fh3_data_val);
    fileFh = fh;
    return true;
  }

  // create didn't return the file handle and attributes we do a lookup to get them
  if (lookup(dirFh, fileName, fileFh, outAttr, status))
  {
    // fail if the attributes aren't available
    if (outAttr.empty())
      return false;
  }
  else
  {
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::getRootFH(const string &nfs_export, NfsFh &rootFh, NfsError &status)
{
  if (nfs_export.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::getRootFH export path can not be empty");
    return false;
  }

  enum clnt_stat retval;
  dirpath pszMountPoint = (dirpath) nfs_export.c_str();
  string serverIP = m_pConn->getIP();

  Mount::MntCall mountMntCall(pszMountPoint);
  retval = mountMntCall.call(m_pConn);
  if ( retval != RPC_SUCCESS )
  {
    status.setRpcError(retval, "Nfs3ApiHandle::getRootFH(): Mount Failed RPC ERROR");
    return false;
  }

  mountres3 mntRes = mountMntCall.getResult();
  if ( mntRes.fhs_status != MNT3_OK )
  {
    status.setMntError(mntRes.fhs_status, "Nfs3ApiHandle::getRootFH(): mount failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::getRootFH(): mount failed to %s:%s: %d\n", serverIP.c_str(), nfs_export.c_str(), mntRes.fhs_status);
    return false;
  }

  nfs_fh3 *fh3 = &(mntRes.mountres3_u.mount3_mountinfo.mount3_fhandle);
  NfsFh fh(fh3->fh3_data.fh3_data_len, fh3->fh3_data.fh3_data_val);
  rootFh = fh;

  // if mount point is "/", then dont call unmount
  if (nfs_export != "/")
  {
    Mount::UMountCall mountUmntCall(pszMountPoint);
    retval = mountUmntCall.call(m_pConn);
    if (retval != RPC_SUCCESS)
    {
      syslog(LOG_ERR, "Nfs3ApiHandle::getRootFH(): umount error to %s:%s:\n", serverIP.c_str(), nfs_export.c_str());
      // Ignore umount failure
    }
  }
  return true;
}

bool Nfs3ApiHandle::getFileHandle(NfsFh &rootFH, const std::string path, NfsFh &fileFh, NfsAttr &attr, NfsError &status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::getFileHandle path can not be empty");
    return false;
  }

  NfsFh currentFH;
  currentFH = rootFH;

  vector<string> segments;
  NfsUtil::splitNfsPath(path, segments);

  for (string CurrSegment : segments)
  {
    LOOKUP3args lkpArg = {};
    lkpArg.lookup3_what.dirop3_dir.fh3_data.fh3_data_len = currentFH.getLength();
    lkpArg.lookup3_what.dirop3_dir.fh3_data.fh3_data_val = (char*)currentFH.getData();
    lkpArg.lookup3_what.dirop3_name = (char*)CurrSegment.c_str();

    NFSv3::LookUpCall nfsLookupCall(lkpArg);
    enum clnt_stat tLookupRet = nfsLookupCall.call(m_pConn);
    if (tLookupRet != RPC_SUCCESS)
    {
      status.setRpcError(tLookupRet, "Nfs3ApiHandle::getFileHandle(): lookup rpc error");
      return false;
    }

    LOOKUP3res &res = nfsLookupCall.getResult();
    if (res.status != NFS3_OK)
    {
      status.setError3(res.status, "Nfs3ApiHandle::getFileHandle(): lookup failed");
      if (res.status == NFS3ERR_NOENT)
      {
        syslog(LOG_DEBUG, "Nfs3ApiHandle::getFileHandle(): nfs_v3_lookup didn't find the file %s %s\n", path.c_str(), CurrSegment.c_str());
      }
      else
      {
        syslog(LOG_ERR, "Nfs3ApiHandle::getFileHandle(): nfs_v3_lookup error: %d  <%s %s>\n", res.status, path.c_str(), CurrSegment.c_str());
      }
      return false;
    }

    nfs_fh3 *fh3 = &(res.LOOKUP3res_u.lookup3ok.lookup3_object);
    NfsFh fh(fh3->fh3_data.fh3_data_len, fh3->fh3_data.fh3_data_val);
    currentFH = fh;
  }

  //currentFH has the file handle for the file we're looking for, save the value to return
  fileFh = currentFH;

  //now get the attributes for the file handle
  GETATTR3args getAttrArg = {};
  getAttrArg.getattr3_object.fh3_data.fh3_data_len = fileFh.getLength();
  getAttrArg.getattr3_object.fh3_data.fh3_data_val = (char*)fileFh.getData();

  NFSv3::GetAttrCall nfsGetattrCall(getAttrArg);
  enum clnt_stat GetAttrRet = nfsGetattrCall.call(m_pConn);
  if (GetAttrRet != RPC_SUCCESS)
  {
    status.setRpcError(GetAttrRet, "Nfs3ApiHandle::getFileHandle(): getattr rpc error");
    return false;
  }

  GETATTR3res &res = nfsGetattrCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::getFileHandle(): getattr failed");
    syslog(LOG_ERR, "SL_NASFileServer::nfs_getFileHandle(): nfs_v3_getattr error: %d\n", res.status);
    return false;
  }

  // copy the object attributes out of the result struct
  attr.Fattr3ToNfsAttr(&(res.GETATTR3res_u.getattr3ok.getattr3_obj_attributes));
  return true;
}

bool Nfs3ApiHandle::open(NfsFh           &rootFh,
                         const std::string  filePath,
                         NfsFh             &fileFh,
                         NfsAttr           &fileAttr,
                         NfsError          &err)
{
  return false;
}

bool Nfs3ApiHandle::open(const std::string filePath,
                         uint32_t          access,
                         uint32_t          shareAccess,
                         uint32_t          shareDeny,
                         NfsFh             &file,
                         NfsError          &status)
{
  return false;
}

bool Nfs3ApiHandle::read(NfsFh       &fileFH,
                         uint64_t     offset,
                         uint32_t     length,
                         std::string &data,
                         uint32_t    &bytesRead,
                         bool        &eof,
                         NfsAttr     &postAttr,
                         NfsError    &status)
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
    status.setRpcError(readRet, "Nfs3ApiHandle::read(): read() rpc error");
    return false;
  }

  READ3res &res = nfsReadCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::read() failed");
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
    postAttr.Fattr3ToNfsAttr(&(res.READ3res_u.read3ok.read3_file_attributes.post_op_attr_u.post_op_attr));
  }
  else
  {
    syslog(LOG_DEBUG, "Nfs3ApiHandle::%s() failed no post op attrs returned\n", __func__);
    postAttr.clear();
  }

  return true;
}

bool Nfs3ApiHandle::write(NfsFh       &fileFH,
                          uint64_t     offset,
                          uint32_t     length,
                          std::string &data,
                          uint32_t    &bytesWritten,
                          NfsError    &status)
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
    status.setRpcError(ret, "Nfs3ApiHandle::write(): write() rpc error");
    return false;
  }

  WRITE3res &writeRes = nfsWriteCall.getResult();
  if (writeRes.status != NFS3_OK)
  {
    status.setError3(writeRes.status, "Nfs3ApiHandle::write() failed");
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
                                   const bool   needverify,
                                   NfsError    &status)
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
    status.setRpcError(writeRet, "Nfs3ApiHandle::write_unstable(): rpc error");
    return false;
  }

  WRITE3res &res = nfsWriteCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::write_unstable(): failed");
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

bool Nfs3ApiHandle::close(NfsFh &fileFh, NfsAttr &postAttr, NfsError &status)
{
  return false;
}

bool Nfs3ApiHandle::remove(std::string &exp, std::string path, NfsError &status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::remove path can not be empty");
    return false;
  }

  NfsFh rootFh;
  NfsFh parentFH;

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(path, path_components);

  std::string name = path_components.back();
  path_components.pop_back();

  if(!getRootFH(exp, rootFh, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getRootFH using export\n", __func__);
    return false;
  }

  if (path_components.size() > 0)
  {
    std::string dirPath;
    NfsUtil::buildNfsPath(dirPath, path_components);

    if(!getDirFh(rootFh, dirPath, parentFH, status))
    {
      syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getFileHandle using rootFh\n", __func__);
      return false;
    }
  }
  else
  {
    parentFH = rootFh;
  }

  if(!remove(parentFH, name, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for remove using parentFH\n", __func__);
    return false;
  }

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
    status.setRpcError(ret, "Nfs3ApiHandle::remove(): rpc error");
    return false;
  }

  REMOVE3res &res = nfsRemoveCall.getResult();

  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, string("Nfs3ApiHandle::remove() Failed"));
    syslog(LOG_ERR, "Nfs3ApiHandle::remove(): nfs_v3_remove error: %d  <%s>\n", res.status, name.c_str());
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::rename(NfsFh &fromDirFh,
                           const std::string &fromName,
                           NfsFh &toDirFh,
                           const std::string toName,
                           NfsError &status)
{
  if (fromName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::rename fromName can not be empty");
    return false;
  }
  if (toName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::rename toName can not be empty");
    return false;
  }

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
    status.setRpcError(ret, "Nfs3ApiHandle::rename(): rpc error");
    return false;
  }

  RENAME3res &res = nfsRenameCall.getResult();

  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::rename(): failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::rename(): error: %d <%s,%s>\n", res.status, fromName.c_str(), toName.c_str());
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::rename(const std::string &nfs_export,
                           const std::string &fromPath,
                           const std::string &toPath,
                           NfsError          &status)
{
  if (fromPath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::rename fromPath can not be empty");
    return false;
  }
  if (toPath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::rename toPath can not be empty");
    return false;
  }

  NfsFh rootFh;
  NfsFh fromDirFH;
  NfsFh toDirFH;

  std::vector<std::string> fromPathSegments;
  NfsUtil::splitNfsPath(fromPath, fromPathSegments);

  std::string fromName = fromPathSegments.back();
  fromPathSegments.pop_back();

  std::vector<std::string> toPathSegments;
  NfsUtil::splitNfsPath(toPath, toPathSegments);

  std::string toName = toPathSegments.back();
  toPathSegments.pop_back();

  if(!getRootFH(nfs_export, rootFh, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getRootFH using export\n", __func__);
    return false;
  }

  if (fromPathSegments.size())
  {
    std::string fromDirPath;
    NfsUtil::buildNfsPath(fromDirPath, fromPathSegments);
    if(!getDirFh(rootFh, fromDirPath, fromDirFH, status))
    {
      syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getFileHandle using rootFh for fromDirFH\n", __func__);
      return false;
    }
  }
  else
    fromDirFH = rootFh;

  if (toPathSegments.size())
  {
    std::string toDirPath;
    NfsUtil::buildNfsPath(toDirPath, toPathSegments);
    if(!getDirFh(rootFh, toDirPath, toDirFH, status))
    {
      syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getFileHandle using rootFh for toDirFH\n", __func__);
      return false;
    }
  }
  else
    toDirFH = rootFh;

  if(!rename(fromDirFH, fromName, toDirFH, toName, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for rename\n", __func__);
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::readDir(std::string &exp, const std::string &dirPath, NfsFiles &files, NfsError &status)
{
  NfsFh rootFh;
  NfsFh parentFH;

  if(!getRootFH(exp, rootFh, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getRootFH using export\n", __func__);
    return false;
  }

  if (dirPath.size())
  {
    if(!getDirFh(rootFh, dirPath, parentFH, status))
    {
      syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getFileHandle using rootFh\n", __func__);
      return false;
    }
  }
  else
    parentFH = rootFh;

  if(!readDir(parentFH, files, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for readDir using parentFH\n", __func__);
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::readDir(NfsFh &dirFh, NfsFiles &files, NfsError &status)
{
  bool sts = false;
  bool ReadDirError = false;
  bool eof = false;
  cookie3 Cookie = 0;
  cookieverf3 CookieVerf;
  memset(CookieVerf, 0, sizeof(CookieVerf));

  while (!ReadDirError && !eof)
  {
    if (!readDirPlus(dirFh, Cookie, CookieVerf, files, eof, status))
    {
      ReadDirError = true;
    }
  }

  if (!ReadDirError)
    sts = true;

  return sts;
}

bool Nfs3ApiHandle::getAttrForDirEntry(const entryplus3* pEntry,
                                       NfsFh&            fh,
                                       std::string       name,
                                       fattr3&           attr)
{
  const nfs_fh3     *pHandle = NULL;

  if (pEntry->entryplus3_name_attributes.attributes_follow)
  {
    attr = pEntry->entryplus3_name_attributes.post_op_attr_u.post_op_attr;
    return true;
  }

  if (pEntry->entryplus3_name_handle.handle_follows)
  {
    pHandle = &pEntry->entryplus3_name_handle.post_op_fh3_u.post_op_fh3;
    if (pHandle)
    {
      GETATTR3args arg = {};
      arg.getattr3_object = *pHandle;

      NFSv3::GetAttrCall getattrCall(arg);
      enum clnt_stat ret = getattrCall.call(m_pConn);
      if ( ret != RPC_SUCCESS )
      {
        return false;
      }

      GETATTR3res& getattrres = getattrCall.getResult();
      if ( getattrres.status != NFS3_OK )
      {
        return false;
      }
      attr = getattrres.GETATTR3res_u.getattr3ok.getattr3_obj_attributes;
      return true;
    }
  }
  else
  {
    LOOKUP3args arg = {};
    arg.lookup3_what.dirop3_dir.fh3_data.fh3_data_len = fh.getLength();
    arg.lookup3_what.dirop3_dir.fh3_data.fh3_data_val = fh.getData();
    arg.lookup3_what.dirop3_name =  pEntry->entryplus3_name;

    NFSv3::LookUpCall lookupCall(arg);
    enum clnt_stat ret = lookupCall.call(m_pConn);
    if ( ret != RPC_SUCCESS )
    {
      return false;
    }

    LOOKUP3res& lookupres = lookupCall.getResult();
    if ( lookupres.status != NFS3_OK )
    {
      return false;
    }

    pHandle = &lookupres.LOOKUP3res_u.lookup3ok.lookup3_object;
    if ( lookupres.LOOKUP3res_u.lookup3ok.lookup3_obj_attributes.attributes_follow )
    {
      attr = lookupres.LOOKUP3res_u.lookup3ok.lookup3_obj_attributes.post_op_attr_u.post_op_attr;
      return true;
    }
  }
  return false;
}

bool Nfs3ApiHandle::readDirPlus(NfsFh       &dirFh,
                                cookie3     &cookie,
                                cookieverf3 &cookieVref,
                                NfsFiles    &files,
                                bool        &eof,
                                NfsError    &status)
{
  READDIRPLUS3args readDirPlusArg = {};
  readDirPlusArg.readdirplus3_dir.fh3_data.fh3_data_len = dirFh.getLength();
  readDirPlusArg.readdirplus3_dir.fh3_data.fh3_data_val = (char*)dirFh.getData();
  readDirPlusArg.readdirplus3_cookie = cookie;
  memcpy(&(readDirPlusArg.readdirplus3_cookieverf), &(cookieVref), sizeof(cookieVref));
  readDirPlusArg.readdirplus3_dircount = 1048;
  readDirPlusArg.readdirplus3_maxcount = 8192;

  NFSv3::ReaddirPlusCall nfsReaddirPlusCall(readDirPlusArg);
  enum clnt_stat readDirPlusRet = nfsReaddirPlusCall.call(m_pConn);
  if (readDirPlusRet != RPC_SUCCESS)
  {
    status.setRpcError(readDirPlusRet, "Nfs3ApiHandle::readDirPlus(): rpc error");
    return false;
  }

  READDIRPLUS3res &res = nfsReaddirPlusCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::readDirPlus() failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::readDirPlus(): nfs_v3_readdirplus error: %d\n", res.status);
    return false;
  }

  // copy the cookie verifier from the readdirplus results
  memcpy(&(cookieVref),
         &(res.READDIRPLUS3res_u.readdirplus3ok.readdirplus3_cookieverf_res),
         sizeof( cookieverf3 ));

  // copy the EOF from the readdirplus results
  if (res.READDIRPLUS3res_u.readdirplus3ok.readdirplus3_reply.dirlistplus3_eof)
  {
    eof = true;
  }
  else
  {
    eof = false;
  }

  // copy the entries to rtDirEntries
  entryplus3 *ptCurr = res.READDIRPLUS3res_u.readdirplus3ok.readdirplus3_reply.dirlistplus3_entries;

  while( ptCurr )
  {
    NfsFile file;
    file.cookie = ptCurr->entryplus3_cookie;
    file.name = string( ptCurr->entryplus3_name );
    file.path = "";
    fattr3 dattr;
    bool excludedDir = false;

    /* can't get attributes for . and .. */
    if ( ::strcmp(ptCurr->entryplus3_name, ".") == 0 || ::strcmp(ptCurr->entryplus3_name, "..") == 0 )
      excludedDir = true;

    if (!excludedDir)
    {
      if (!getAttrForDirEntry(ptCurr, dirFh, file.name, dattr))
      {
        status.setError3(NFS3ERR_INVAL, "readDirPlus:failed to get attrib for entry");
        return false;
      }
      file.attr.Fattr3ToNfsAttr(&dattr);
      file.type = file.attr.fileType;
    }
    else
    {
      file.type = FILE_TYPE_DIR;
    }
    files.push_back(file);
    cookie = ptCurr -> entryplus3_cookie;
    ptCurr = ptCurr -> entryplus3_nextentry;
  }

  return true;
}

bool Nfs3ApiHandle::truncate(NfsFh &fh, uint64_t size, NfsError &status)
{
  return false;
}

bool Nfs3ApiHandle::truncate(const std::string &path, uint64_t size, NfsError &status)
{
  return false;
}

bool Nfs3ApiHandle::access(const std::string &filePath, uint32_t accessRequested, NfsAccess &acc, NfsError &status)
{
  return false;
}

bool Nfs3ApiHandle::mkdir(const NfsFh       &parentFH,
                          const std::string  dirName,
                          uint32_t           mode,
                          NfsFh             &dirFH,
                          NfsError          &status)
{
  if (dirName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::mkdir dirName can not be empty");
    return false;
  }

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
    status.setRpcError(mkdirRet, "Nfs3ApiHandle::mkdir(): rpc error");
    return false;
  }

  if (mkdirRes.status != NFS3_OK)
  {
    status.setError3(mkdirRes.status, "Nfs3ApiHandle::mkdir() failed");
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
    status.setRpcError(lookupRet, "Nfs3ApiHandle::mkdir(): lookup call rpc error");
    return false;
  }

  LOOKUP3res &lookupRes = nfsLookupCall.getResult();
  if (lookupRes.status != NFS3_OK)
  {
    status.setError3(lookupRes.status, "Nfs3ApiHandle::mkdir() lookup call failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::mkdir(): nfs_v3_lookup error: %d  <%s>\n", lookupRes.status, dirName.c_str());
    return false;
  }

  nfs_fh3 *fh3 = &(lookupRes.LOOKUP3res_u.lookup3ok.lookup3_object);
  NfsFh fh(fh3->fh3_data.fh3_data_len, fh3->fh3_data.fh3_data_val);
  dirFH = fh;

  return true;
}

bool Nfs3ApiHandle::mkdir(const std::string &path, uint32_t mode, NfsError &status, bool createPath)
{
  return false;
}

bool Nfs3ApiHandle::rmdir(std::string &exp, const std::string &path, NfsError &status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::rmdir dir path can not be empty");
    return false;
  }

  NfsFh rootFh;
  NfsFh parentFH;

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(path, path_components);

  std::string name = path_components.back();
  path_components.pop_back();

  if(!getRootFH(exp, rootFh, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getRootFH using export\n", __func__);
    return false;
  }

  if (path_components.size())
  {
    std::string parentPath;
    NfsUtil::buildNfsPath(parentPath, path_components);
    if(!getDirFh(rootFh, parentPath, parentFH, status))
    {
      syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getFileHandle using rootFh\n", __func__);
      return false;
    }
  }
  else
    parentFH = rootFh;

  if (!rmdir(parentFH, name, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for rmdir using parentFH\n", __func__);
  }

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
    status.setRpcError(ret, "Nfs3ApiHandle::rmdir(): rpc error");
    return false;
  }

  RMDIR3res &res = nfsRmdirCall.getResult();

  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, string("Nfs3ApiHandle::rmdir() Failed"));
    syslog(LOG_ERR, "Nfs3ApiHandle::rmdir(): nfs_v3_rmdir error: %d  <%s>\n", res.status, name.c_str());
    return false;
  }

  return true;
}
bool Nfs3ApiHandle::commit(NfsFh &fh, uint64_t offset, uint32_t bytes, char *writeverf, NfsError &status)
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
    status.setRpcError(commitRet, "Nfs3ApiHandle::commit(): rpc error");
    return false;
  }

  COMMIT3res &res = nfsCommitCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::comit() failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::%s: nfs_v3_commit error: %d\n", __func__, res.status);
    return false;
  }

  memset(writeverf, 0, NFS3_WRITEVERFSIZE);
  memcpy(writeverf, res.COMMIT3res_u.commit3ok.commit3_verf, NFS3_WRITEVERFSIZE);

  return true;
}

bool Nfs3ApiHandle::lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status, bool reclaim)
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
    status.setRpcError(lockRet, "Nfs3ApiHandle::lock(): rpc error");
    return false;
  }

  nlm4_res &res = nlmLockCall.getResult();

  if ( res.nlm4_res_stat.nlm4_stat != NLMSTAT4_GRANTED )
  {
    status.setNlmError(res.nlm4_res_stat.nlm4_stat, "Nfs3ApiHandle::lock() NLM lock failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::nlm_nm_lock(): nlm_v4_nm_lock error: %d\n", res.nlm4_res_stat.nlm4_stat);
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status)
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
    status.setRpcError(tUnlockRet, "Nfs3ApiHandle::unlock(): rpc error");
    return false;
  }

  nlm4_res &res = nlmUnlockCall.getResult();
  if (res.nlm4_res_stat.nlm4_stat != NLMSTAT4_GRANTED)
  {
    status.setNlmError(res.nlm4_res_stat.nlm4_stat, "Nfs3ApiHandle::unlock() NLM unlock failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::nlm_lock(): nlm_v4_unlock error: %d\n", res.nlm4_res_stat.nlm4_stat);
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::setattr(NfsFh &fh, NfsAttr &attr, NfsError &status)
{
  SETATTR3args sattrArg = {};

  sattrArg.setattr3_object.fh3_data.fh3_data_len = fh.getLength();
  sattrArg.setattr3_object.fh3_data.fh3_data_val = (char*)fh.getData();
  sattr3 sattr;
  attr.NfsAttrToSattr3(&sattr);
  memcpy(&(sattrArg.setattr3_new_attributes), &sattr, sizeof(sattr3));
  sattrArg.setattr3_guard.check = 0; // don't check for a ctime match

  NFSv3::SetAttrCall nfsSetattrCall(sattrArg);
  enum clnt_stat sattrRet = nfsSetattrCall.call(m_pConn);

  if (sattrRet != RPC_SUCCESS)
  {
    status.setRpcError(sattrRet, "Nfs3ApiHandle::setattr(): rpc error");
    return false;
  }

  SETATTR3res &res = nfsSetattrCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::setattr() failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::setAttr(): nfs_v3_setattr error: %d\n", res.status);
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::getAttr(NfsFh &fh, NfsAttr &attr, NfsError &status)
{
  GETATTR3args getAttrArg = {};

  getAttrArg.getattr3_object.fh3_data.fh3_data_len = fh.getLength();
  getAttrArg.getattr3_object.fh3_data.fh3_data_val = (char*)fh.getData();

  NFSv3::GetAttrCall nfsGetattrCall(getAttrArg);
  enum clnt_stat getAttrRet = nfsGetattrCall.call(m_pConn);
  if (getAttrRet != RPC_SUCCESS)
  {
    status.setRpcError(getAttrRet, "Nfs3ApiHandle::getAttr(): rpc error");
    return false;
  }

  GETATTR3res &res = nfsGetattrCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::getAttr() failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::getAttr(): nfs_v3_getattr error: %d\n", res.status);
    return false;
  }

  // copy the object attributes out of the result struct
  attr.Fattr3ToNfsAttr(&(res.GETATTR3res_u.getattr3ok.getattr3_obj_attributes));

  return true;
}

bool Nfs3ApiHandle::getAttr(const std::string& exp, const std::string& path, NfsAttr& attr, NfsError& status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::getAttr path can not be empty");
    return false;
  }

  NfsFh tmpFh;
  return lookupPath(exp, path, tmpFh, attr, status);
}

bool Nfs3ApiHandle::lookup(const std::string &path, NfsFh &lookup_fh, NfsError &status)
{
  return false;
}

bool Nfs3ApiHandle::lookup(NfsFh             &dirFh,
                           const std::string &file,
                           NfsFh             &lookup_fh,
                           NfsAttr           &attr,
                           NfsError          &status)
{
  if (file.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::lookup file name can not be empty");
    return false;
  }

  LOOKUP3args lkpArg = {};

  lkpArg.lookup3_what.dirop3_dir.fh3_data.fh3_data_len = dirFh.getLength();
  lkpArg.lookup3_what.dirop3_dir.fh3_data.fh3_data_val = (char*)dirFh.getData();
  string name = file;
  lkpArg.lookup3_what.dirop3_name = (char*)name.c_str();

  NFSv3::LookUpCall nfsLookupCall(lkpArg);
  enum clnt_stat lkpRet = nfsLookupCall.call(m_pConn);
  if (lkpRet != RPC_SUCCESS)
  {
    status.setRpcError(lkpRet, "Nfs3ApiHandle::lookup(): rpc error");
    return false;
  }

  LOOKUP3res res = nfsLookupCall.getResult();
  if (res.status != NFS3_OK)
  {
    if (res.status != NFS3ERR_NOENT)
    {
      status.setError3(res.status, "Nfs3ApiHandle::lookup(): failed");
      syslog(LOG_ERR, "Nfs3ApiHandle::lookup(): nfs_v3_lookup error: %d  <%s>\n", res.status, name.c_str());
    }
    return false;
  }

  nfs_fh3 *fh3 = &(res.LOOKUP3res_u.lookup3ok.lookup3_object);
  NfsFh fh(fh3->fh3_data.fh3_data_len, fh3->fh3_data.fh3_data_val);
  lookup_fh = fh;

  bool gotAttributes = false;

  if (res.LOOKUP3res_u.lookup3ok.lookup3_obj_attributes.attributes_follow)
  {
    attr.Fattr3ToNfsAttr(&(res.LOOKUP3res_u.lookup3ok.lookup3_obj_attributes.post_op_attr_u.post_op_attr));
    gotAttributes = true;
  }
  if (res.LOOKUP3res_u.lookup3ok.lookup3_dir_attributes.attributes_follow)
  {
    // TODO sarat nfs - do we want dir attr??
    //= res.LOOKUP3res_u.lookup3ok.lookup3_dir_attributes.post_op_attr_u.post_op_attr;
  }

  if (gotAttributes)
    return true;

  //now get the attributes for the file handle
  GETATTR3args getAttrArg = {};
  getAttrArg.getattr3_object.fh3_data.fh3_data_len = lookup_fh.getLength();
  getAttrArg.getattr3_object.fh3_data.fh3_data_val = (char*)lookup_fh.getData();

  NFSv3::GetAttrCall nfsGetattrCall(getAttrArg);
  enum clnt_stat GetAttrRet = nfsGetattrCall.call(m_pConn);
  if (GetAttrRet != RPC_SUCCESS)
  {
    status.setRpcError(GetAttrRet, "Nfs3ApiHandle::lookup(): getattr rpc error");
    return false;
  }

  GETATTR3res &attrres = nfsGetattrCall.getResult();
  if (attrres.status != NFS3_OK)
  {
    status.setError3(attrres.status, "Nfs3ApiHandle::lookup(): getattr failed");
    syslog(LOG_ERR, "SL_NASFileServer::lookup(): getattr error: %d\n", attrres.status);
    return false;
  }

  // copy the object attributes out of the result struct
  attr.Fattr3ToNfsAttr(&(attrres.GETATTR3res_u.getattr3ok.getattr3_obj_attributes));

  return true;
}

bool Nfs3ApiHandle::lookupPath(const std::string &exp_path,
                               const std::string &pathFromRoot,
                               NfsFh             &lookup_fh,
                               NfsAttr           &lookup_attr,
                               NfsError          &status)
{
  if (pathFromRoot.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::lookupPath path can not be empty");
    return false;
  }

  NfsFh rootFh;
  if (!getRootFH(exp_path, rootFh, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getRootFH\n", __func__);
    return false;
  }

  return lookupPath(rootFh, pathFromRoot, lookup_fh, lookup_attr, status);
}

bool Nfs3ApiHandle::lookupPath(NfsFh             &rootFh,
                               const std::string &pathFromRoot,
                               NfsFh             &lookup_fh,
                               NfsAttr           &lookup_attr,
                               NfsError          &status)
{
  if (pathFromRoot.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::lookupPath path can not be empty");
    return false;
  }

  return getFileHandle(rootFh, pathFromRoot, lookup_fh, lookup_attr, status);
}

bool Nfs3ApiHandle::fileExists(const std::string& exp, const std::string& path, NfsAttr& attr, NfsError& status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::fileExists path can not be empty");
    return false;
  }

  NfsFh tmpFh;
  return lookupPath(exp, path, tmpFh, attr, status);
}

bool Nfs3ApiHandle::fsstat(NfsFh &rootFh, NfsFsStat &stat, uint32 &invarSec, NfsError &status)
{
  FSSTAT3args FsStatArg = {};

  FsStatArg.fsstat3_fsroot.fh3_data.fh3_data_len = rootFh.getLength();
  FsStatArg.fsstat3_fsroot.fh3_data.fh3_data_val = (char*)rootFh.getData();

  NFSv3::FsstatCall nfsFsstatCall(FsStatArg);
  enum clnt_stat FsStatRet = nfsFsstatCall.call(m_pConn);
  if (FsStatRet != RPC_SUCCESS)
  {
    status.setRpcError(FsStatRet, "Nfs3ApiHandle::fsstat(): rpc error");
    return false;
  }

  FSSTAT3res &res = nfsFsstatCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::fsstat() failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::fsstat(): nfs_v3_fsstat error: %d\n", res.status);
    return false;
  }

  // return the values we're interested in
  stat.files_avail = res.FSSTAT3res_u.fsstat3ok.fsstat3_fsstat3.fsstat3_afiles;
  stat.files_free  = res.FSSTAT3res_u.fsstat3ok.fsstat3_fsstat3.fsstat3_ffiles;
  stat.files_total = res.FSSTAT3res_u.fsstat3ok.fsstat3_fsstat3.fsstat3_tfiles;
  stat.bytes_avail = res.FSSTAT3res_u.fsstat3ok.fsstat3_fsstat3.fsstat3_abytes;
  stat.bytes_free  = res.FSSTAT3res_u.fsstat3ok.fsstat3_fsstat3.fsstat3_fbytes;
  stat.bytes_total = res.FSSTAT3res_u.fsstat3ok.fsstat3_fsstat3.fsstat3_tbytes;
  invarSec = res.FSSTAT3res_u.fsstat3ok.fsstat3_invarsec;

  return true;
}

// hard link
bool Nfs3ApiHandle::link(NfsFh        &tgtFh,
                         NfsFh        &parentFh,
                         const string &linkName,
                         NfsError     &status)
{
  if (linkName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::link linkName can not be empty");
    return false;
  }

  LINK3args lnkArg = {};

  lnkArg.link3_file.fh3_data.fh3_data_len = tgtFh.getLength();
  lnkArg.link3_file.fh3_data.fh3_data_val = (char*)tgtFh.getData();
  lnkArg.link3_link.dirop3_dir.fh3_data.fh3_data_len = parentFh.getLength();
  lnkArg.link3_link.dirop3_dir.fh3_data.fh3_data_val = (char*)parentFh.getData();
  string Name = linkName;
  lnkArg.link3_link.dirop3_name = (char *)Name.c_str();

  NFSv3::LinkCall nfsLinkCall(lnkArg);
  enum clnt_stat ret = nfsLinkCall.call(m_pConn);

  if (ret != RPC_SUCCESS)
  {
    status.setRpcError(ret, "Nfs3ApiHandle::link(): rpc error");
    return false;
  }

  LINK3res &res = nfsLinkCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::link() : failed");
    // If we see an attempt to create a link that crosses file systems, report it - NFS3ERR_XDEV
    syslog(LOG_ERR, "Nfs3ApiHandle::link(): nfs_v3_link error: %d  <%s>\n", res.status, linkName.c_str());
    return false;
  }
  return true;
}

// symlink
bool Nfs3ApiHandle::symlink(const string &tgtPath,
                            NfsFh        &parentFh,
                            const string &linkName,
                            NfsError     &status)
{
  if (linkName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs3ApiHandle::symlink linkName can not be empty");
    return false;
  }

  SYMLINK3args smlnk = {};
  sattr3 dummyAttr;
  dummyAttr.sattr3_mode.set_it             = true;
  dummyAttr.sattr3_mode.set_mode3_u.mode   = 0511;

  smlnk.symlink3_where.dirop3_dir.fh3_data.fh3_data_len = parentFh.getLength();
  smlnk.symlink3_where.dirop3_dir.fh3_data.fh3_data_val = (char*)parentFh.getData();
  string lnkNmae = linkName;
  string target  = tgtPath;
  smlnk.symlink3_where.dirop3_name = (char*)lnkNmae.c_str(); //TODO sarat - is this link name or target??
  smlnk.symlink3_symlink.symlink3_attributes = dummyAttr;
  smlnk.symlink3_symlink.symlink3_data = (char*)target.c_str(); // same is this target or link name

  NFSv3::SymLinkCall nfsSymLinkCall(smlnk);
  enum clnt_stat ret = nfsSymLinkCall.call(m_pConn);
  if (ret != RPC_SUCCESS)
  {
    status.setRpcError(ret, "Nfs3ApiHandle::symlink(): rpc error");
    return false;
  }

  SYMLINK3res &res = nfsSymLinkCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "Nfs3ApiHandle::symlink(): failed");
    // If we see an attempt to create a link that crosses file systems, report it - NFS3ERR_XDEV
    syslog(LOG_ERR, "Nfs3ApiHandle::symlink(): nfs_v3_link error: %d  <%s>\n", res.status, linkName.c_str());
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::renewCid()
{
  // not a feature of NFS v3
  return false;
}
