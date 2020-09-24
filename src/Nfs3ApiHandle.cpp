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
  NfsFh currentFH;
  currentFH = rootFH;

  vector<string> segments;
  NfsUtil::splitNfsPath(dirPath, segments);

  vector<string>::iterator Iter = segments.begin();
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
      //rtNfsErrorMsg = NFS3ERR_SERVERFAULT;
      return false;
    }

    LOOKUP3res &res = nfsLookupCall.getResult();
    if (res.status != NFS3_OK)
    {
      //rtNfsErrorMsg = res.status;
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

bool Nfs3ApiHandle::getDirFh(const std::string &dirPath, NfsFh &dirFH, NfsError &status)
{
  return true;
}

bool Nfs3ApiHandle::create(NfsFh             &dirFh,
                           std::string       &fileName,
                           NfsAttr           *inAttr,
                           NfsFh             &fileFh,
                           NfsAttr           &outAttr,
                           NfsError          &status)
{
  CREATE3args createArg = {};

  createArg.create3_where.dirop3_dir.fh3_data.fh3_data_len = dirFh.getLength();
  createArg.create3_where.dirop3_dir.fh3_data.fh3_data_val = (char*)dirFh.getData();
  createArg.create3_where.dirop3_name = (char*)fileName.c_str();
  createArg.create3_how.mode = CREATE_GUARDED;

  if (inAttr)
  {
    memcpy(&(createArg.create3_how.createhow3_u.create3_obj_attributes),
           &(inAttr->attr3.sattr), sizeof(sattr3));
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
    status.setError3(NFS3ERR_SERVERFAULT, "create(): nfs v3 rpc failed for create");
    return false;
  }

  CREATE3res &res = nfsCreateCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "nfs v3 create failed");
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
    outAttr.attr3.gattr.fattr3_fsid   = res.CREATE3res_u.create3_ok.create3_obj_attributes.post_op_attr_u.post_op_attr.fattr3_fsid;
    outAttr.attr3.gattr.fattr3_fileid = res.CREATE3res_u.create3_ok.create3_obj_attributes.post_op_attr_u.post_op_attr.fattr3_fileid;
    nfs_fh3 &fh3 = res.CREATE3res_u.create3_ok.create3_obj.post_op_fh3_u.post_op_fh3;
    NfsFh fh(fh3.fh3_data.fh3_data_len, fh3.fh3_data.fh3_data_val);
    fileFh = fh;
    return true;
  }

  // create didn't return the file handle and attributes we do a lookup to get them
  NfsAttr lkpAttr;
  if (lookup(dirFh, fileName, fileFh, lkpAttr, status))
  {
    if (lkpAttr.attr3.lattr.obj_attr_present)
    {
      outAttr.attr3.gattr.fattr3_fsid   = lkpAttr.attr3.lattr.obj_attr.fattr3_fsid;
      outAttr.attr3.gattr.fattr3_fileid = lkpAttr.attr3.lattr.obj_attr.fattr3_fileid;
    }
    else
    {
      // fail if the attributes aren't available
      return false;
    }
  }
  else
  {
    return false;
  }

  return true;
}

bool Nfs3ApiHandle::getRootFH(const string &nfs_export, NfsFh &rootFh, NfsError &status)
{
  enum clnt_stat retval;
  dirpath pszMountPoint = (dirpath) nfs_export.c_str();
  string serverIP = m_pConn->getIP();

  Mount::MntCall mountMntCall(pszMountPoint);
  retval = mountMntCall.call(m_pConn);
  if ( retval != RPC_SUCCESS )
  {
    status.setError3(NFS3ERR_SERVERFAULT, "getRootFH(): nfs v3 rpc failed for mount");
    return false;
  }

  mountres3 mntRes = mountMntCall.getResult();
  if ( mntRes.fhs_status != MNT3_OK )
  {
    //rtMountError = mntRes.fhs_status;
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
  NfsFh currentFH;
  currentFH = rootFH;

  vector<string> segments;
  NfsUtil::splitNfsPath(path, segments);

  vector<string>::iterator Iter = segments.begin();
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
      status.setError3(NFS3ERR_SERVERFAULT, "getFileHandle(): nfs v3 rpc failed for lookup");
      return false;
    }

    LOOKUP3res &res = nfsLookupCall.getResult();
    if (res.status != NFS3_OK)
    {
      //rtNfsErrorMsg = res.status;
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
    status.setError3(NFS3ERR_SERVERFAULT, "getFileHandle(): nfs v3 rpc failed for getattr");
    return false;
  }

  GETATTR3res &res = nfsGetattrCall.getResult();
  if (res.status != NFS3_OK)
  {
    //rtNfsErrorMsg = res.status;
    syslog(LOG_ERR, "SL_NASFileServer::nfs_getFileHandle(): nfs_v3_getattr error: %d\n", res.status);
    return false;
  }

  // copy the object attributes out of the result struct
  memcpy(&attr.attr3.gattr, &(res.GETATTR3res_u.getattr3ok.getattr3_obj_attributes), sizeof(fattr3));
  return true;
}

bool Nfs3ApiHandle::open(const std::string filePath,
                         uint32_t          access,
                         uint32_t          shareAccess,
                         uint32_t          shareDeny,
                         NfsFh             &file,
                         NfsError          &status)
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
    status.setError3(NFS3ERR_SERVERFAULT, "read(): nfs v3 rpc failed for read");
    return false;
  }

  READ3res &res = nfsReadCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "nfs_v3_read failed");
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
    postAttr.attr3.gattr = res.READ3res_u.read3ok.read3_file_attributes.post_op_attr_u.post_op_attr;
  }
  else
  {
    syslog(LOG_DEBUG, "Nfs3ApiHandle::%s() failed no post op attrs returned\n", __func__);
    memset(&postAttr.attr3.gattr, 0, sizeof(fattr3));
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
    status.setError3(NFS3ERR_SERVERFAULT, "write(): nfs v3 rpc failed for write");
    return false;
  }

  WRITE3res &writeRes = nfsWriteCall.getResult();
  if (writeRes.status != NFS3_OK)
  {
    status.setError3(writeRes.status, "nfs_v3_write failed");
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
    status.setError3(NFS3ERR_SERVERFAULT, "write_unstable(): nfs v3 rpc failed for write");
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

bool Nfs3ApiHandle::close(NfsFh &fileFh, NfsAttr &postAttr, NfsError &status)
{
  return true;
}

bool Nfs3ApiHandle::remove(std::string &exp, std::string path, NfsError &status)
{
  NfsFh rootFh;
  NfsFh parentFH;

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(path, path_components);

  std::string name = path_components.back();

  if(!getRootFH(exp, rootFh, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getRootFH using export\n", __func__);
    return false;
  }

  if (!getDirFh(rootFh, path, parentFH, status))
  {
    syslog(LOG_ERR, "Nfs3ApiHandle::%s() failed for getDirFh using rootFh\n", __func__);
    return false;
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
    status.setError3(NFS3ERR_SERVERFAULT, "remove(): nfs v3 rpc failed for remove");
    return false;
  }

  REMOVE3res &res = nfsRemoveCall.getResult();

  if (res.status != NFS3_OK)
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
                           NfsError &status)
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
    status.setError3(NFS3ERR_SERVERFAULT, "rename(): nfs v3 rpc failed for rename");
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
                           const std::string &toPath,
                           NfsError          &status)
{
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
    status.setError3(NFS3ERR_SERVERFAULT, "readDirPlus(): nfs v3 rpc failed for readDirPlus");
    return false;
  }

  READDIRPLUS3res &res = nfsReaddirPlusCall.getResult();
  //rtNfsError = res.status;
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "nfs v3 readdirplus failed");
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
    file.attr.attr3.gattr = ptCurr->entryplus3_name_attributes.post_op_attr_u.post_op_attr;
    files.push_back(file);
    cookie = ptCurr -> entryplus3_cookie;
    ptCurr = ptCurr -> entryplus3_nextentry;
  }

  return true;
}

bool Nfs3ApiHandle::truncate(NfsFh &fh, uint64_t size, NfsError &status)
{
  return true;
}

bool Nfs3ApiHandle::truncate(const std::string &path, uint64_t size, NfsError &status)
{
  return true;
}

bool Nfs3ApiHandle::access(const std::string &filePath, uint32_t accessRequested, NfsAccess &acc, NfsError &status)
{
  return true;
}

bool Nfs3ApiHandle::mkdir(const NfsFh       &parentFH,
                          const std::string  dirName,
                          uint32_t           mode,
                          NfsFh             &dirFH,
                          NfsError          &status)
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
    status.setError3(NFS3ERR_SERVERFAULT, "mkdir(): nfs v3 rpc failed for mkdir");
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
    status.setError3(NFS3ERR_SERVERFAULT, "mkdir(): nfs v3 rpc failed for lookup");
    return false;
  }

  LOOKUP3res &lookupRes = nfsLookupCall.getResult();
  if (lookupRes.status != NFS3_OK)
  {
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
  return true;
}

bool Nfs3ApiHandle::rmdir(std::string &exp, const std::string &path, NfsError &status)
{
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
    status.setError3(NFS3ERR_SERVERFAULT, "rmdir(): nfs v3 rpc failed for rmdir");
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
    status.setError3(NFS3ERR_SERVERFAULT, "commit(): nfs v3 rpc failed for commit");
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
    status.setError3(NFS3ERR_SERVERFAULT, "lock(): nlm v4 rpc failed for nlm lock");
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
    status.setError3(NFS3ERR_SERVERFAULT, "unlock(): nlm v4 rpc failed for nlm unlock");
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

bool Nfs3ApiHandle::setattr(NfsFh &fh, NfsAttr &attr, NfsError &status)
{
  SETATTR3args sattrArg = {};

  sattrArg.setattr3_object.fh3_data.fh3_data_len = fh.getLength();
  sattrArg.setattr3_object.fh3_data.fh3_data_val = (char*)fh.getData();
  memcpy(&(sattrArg.setattr3_new_attributes), &attr.attr3.sattr, sizeof(sattr3));
  sattrArg.setattr3_guard.check = 0; // don't check for a ctime match

  NFSv3::SetAttrCall nfsSetattrCall(sattrArg);
  enum clnt_stat sattrRet = nfsSetattrCall.call(m_pConn);

  if (sattrRet != RPC_SUCCESS)
  {
    status.setError3(NFS3ERR_SERVERFAULT, "setattr(): nfs v3 rpc failed for setattr");
    return false;
  }

  SETATTR3res &res = nfsSetattrCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "nfs v3 setAttr failed");
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
    status.setError3(NFS3ERR_SERVERFAULT, "getAttr(): nfs v3 rpc failedfor getAttr");
    return false;
  }

  GETATTR3res &res = nfsGetattrCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "nfs v3 getattr failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::getAttr(): nfs_v3_getattr error: %d\n", res.status);
    return false;
  }

  // copy the object attributes out of the result struct
  memcpy(&attr.attr3.gattr, &(res.GETATTR3res_u.getattr3ok.getattr3_obj_attributes), sizeof(fattr3));

  return true;
}

bool Nfs3ApiHandle::lookup(const std::string &path, NfsFh &lookup_fh, NfsError &status)
{
  return true;
}

bool Nfs3ApiHandle::lookup(NfsFh &dirFh, const std::string &file, NfsFh &lookup_fh, NfsAttr &attr, NfsError &status)
{
  LOOKUP3args lkpArg = {};

  lkpArg.lookup3_what.dirop3_dir.fh3_data.fh3_data_len = dirFh.getLength();
  lkpArg.lookup3_what.dirop3_dir.fh3_data.fh3_data_val = (char*)dirFh.getData();
  string name = file;
  lkpArg.lookup3_what.dirop3_name = (char*)name.c_str();

  NFSv3::LookUpCall nfsLookupCall(lkpArg);
  enum clnt_stat lkpRet = nfsLookupCall.call(m_pConn);
  if (lkpRet != RPC_SUCCESS)
  {
    status.setError3(NFS3ERR_SERVERFAULT, "lookup(): nfs v3 rpc failed for lookup");
    return false;
  }

  LOOKUP3res res = nfsLookupCall.getResult();
  if (res.status != NFS3_OK)
  {
    if (res.status != NFS3ERR_NOENT)
    {
      syslog(LOG_ERR, "Nfs3ApiHandle::lookup(): nfs_v3_lookup error: %d  <%s>\n", res.status, name.c_str());
    }
    return false;
  }

  nfs_fh3 *fh3 = &(res.LOOKUP3res_u.lookup3ok.lookup3_object);
  NfsFh fh(fh3->fh3_data.fh3_data_len, fh3->fh3_data.fh3_data_val);
  lookup_fh = fh;

  if (res.LOOKUP3res_u.lookup3ok.lookup3_obj_attributes.attributes_follow)
  {
    attr.attr3.lattr.obj_attr_present = true;
    attr.attr3.lattr.obj_attr = res.LOOKUP3res_u.lookup3ok.lookup3_obj_attributes.post_op_attr_u.post_op_attr;
  }
  if (res.LOOKUP3res_u.lookup3ok.lookup3_dir_attributes.attributes_follow)
  {
    attr.attr3.lattr.dir_attr_present = true;
    attr.attr3.lattr.dir_attr = res.LOOKUP3res_u.lookup3ok.lookup3_dir_attributes.post_op_attr_u.post_op_attr;
  }

  return true;
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
    status.setError3(NFS3ERR_SERVERFAULT, "fsstat(): nfs v3 rpc failed for fsstat");
    return false;
  }

  FSSTAT3res &res = nfsFsstatCall.getResult();
  if (res.status != NFS3_OK)
  {
    status.setError3(res.status, "nfs v3 fsstat failed");
    syslog(LOG_ERR, "Nfs3ApiHandle::fsstat(): nfs_v3_fsstat error: %d\n", res.status);
    return false;
  }

  // return the values we're interested in
  stat.stat_u.stat3 = res.FSSTAT3res_u.fsstat3ok.fsstat3_fsstat3;
  invarSec = res.FSSTAT3res_u.fsstat3ok.fsstat3_invarsec;

  return true;
}

// hard link
bool Nfs3ApiHandle::link(NfsFh        &tgtFh,
                         NfsFh        &parentFh,
                         const string &linkName,
                         NfsError     &status)
{
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
    status.setError3(NFS3ERR_SERVERFAULT, "link(): nfs v3 rpc failed for link");
    return false;
  }

  LINK3res &res = nfsLinkCall.getResult();
  if (res.status != NFS3_OK)
  {
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
    status.setError3(NFS3ERR_SERVERFAULT, "symlink(): nfs v3 rpc failed for symlink");
    return false;
  }

  SYMLINK3res &res = nfsSymLinkCall.getResult();
  if (res.status != NFS3_OK)
  {
    // If we see an attempt to create a link that crosses file systems, report it - NFS3ERR_XDEV
    syslog(LOG_ERR, "Nfs3ApiHandle::symlink(): nfs_v3_link error: %d  <%s>\n", res.status, linkName.c_str());
    return false;
  }

  return true;
}
