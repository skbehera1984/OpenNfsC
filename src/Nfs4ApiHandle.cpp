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
#include <syslog.h>

#include <Nfs4ApiHandle.h>

using namespace OpenNfsC;

//mask[0] = 0x00100112; mask[1] = 0x0030a03a;
static uint32_t std_attr[2] = {
  (1 << FATTR4_TYPE | 1 << FATTR4_SIZE | 1 << FATTR4_FSID | 1 << FATTR4_FILEID),
  (1 << (FATTR4_MODE - 32) |
   1 << (FATTR4_NUMLINKS - 32) |
   1 << (FATTR4_OWNER - 32) |
   1 << (FATTR4_OWNER_GROUP - 32) |
   1 << (FATTR4_SPACE_USED - 32) |
   1 << (FATTR4_TIME_ACCESS - 32) |
   1 << (FATTR4_TIME_METADATA - 32) |
   1 << (FATTR4_TIME_MODIFY - 32))
};

static uint32_t statfs_attr[2] = {
  (1 << FATTR4_FSID |
   1 << FATTR4_FILES_AVAIL |
   1 << FATTR4_FILES_FREE |
   1 << FATTR4_FILES_TOTAL |
   1 << FATTR4_MAXNAME),
  (1 << (FATTR4_SPACE_AVAIL - 32) |
   1 << (FATTR4_SPACE_FREE - 32) |
   1 << (FATTR4_SPACE_TOTAL - 32) |
   1 << (FATTR4_SPACE_USED - 32))
};

Nfs4ApiHandle::Nfs4ApiHandle(NfsConnectionGroup *ptr) : NfsApiHandle(ptr)
{
}

bool Nfs4ApiHandle::connect(std::string &serverIP)
{
  NFSv4::NullCall  ncl;
  enum clnt_stat cst = ncl.call(m_pConn);
  if (cst != ::RPC_SUCCESS)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NULL call failed to serverIP %s\n", __func__, serverIP.c_str());
    return false;
  }

  NFSv4::COMPOUNDCall compCall;

  {
    nfs_argop4 carg;

    /* Set the client id to fma_pid as client name
     * Appending pid is a must. Because if we just have fma as name
     * then multiple connections will perform SETCLIENTID operation using the same name.
     * In such case all the states and locks held by previous clients will be lost/released.
     */
    carg.argop = OP_SETCLIENTID;
    SETCLIENTID4args *sClIdargs = &carg.nfs_argop4_u.opsetclientid;
    memcpy(sClIdargs->client.verifier,
           m_pConn->getInitialClientVerifier(),
           NFS4_VERIFIER_SIZE);
    char id[128] = {0};
    sprintf(id, "fma_%d", getpid());
    sClIdargs->client.id.id_len = strlen(id);
    sClIdargs->client.id.id_val = id;
    sClIdargs->callback.cb_program = 0;
    char r_netid[4] = "tcp";
    r_netid[3] = 0;
    sClIdargs->callback.cb_location.r_netid = r_netid;
    char r_addr[12] = "0.0.0.0.0.0";
    r_addr[11] = 0;
    sClIdargs->callback.cb_location.r_addr = r_addr;
    sClIdargs->callback_ident = 0x00000001;
    compCall.appendCommand(&carg);
  }
  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: SETCLIENTID4 failed to serverIP %s\n", __func__, serverIP.c_str());
    return false;
  }

  SETCLIENTID4res *sRes = &((res.resarray.resarray_val)->nfs_resop4_u.opsetclientid);
  m_pConn->setClientId(sRes->SETCLIENTID4res_u.resok4.clientid);
  m_pConn->setClientVerifier(sRes->SETCLIENTID4res_u.resok4.setclientid_confirm);

  compCall.clear();

  {
    nfs_argop4 carg;
    carg.argop = OP_SETCLIENTID_CONFIRM;
    SETCLIENTID_CONFIRM4args *opsetclientid_confirm = &carg.nfs_argop4_u.opsetclientid_confirm;
    opsetclientid_confirm->clientid = m_pConn->getClientId();
    memcpy(opsetclientid_confirm->setclientid_confirm, m_pConn->getClientVerifier(), NFS4_VERIFIER_SIZE);
    compCall.appendCommand(&carg);
  }
  cst = compCall.call(m_pConn);
  res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: SETCLIENTID_CONFIRM failed to serverIP %s\n", __func__, serverIP.c_str());
    return false;
  }

  m_pConn->setConnected();

  return true;
}

bool Nfs4ApiHandle::getExports(list<string>& Exports)
{
  return false;
}

bool Nfs4ApiHandle::getRootFH(const std::string &nfs_export, NfsFh &rootFh, NfsError &status)
{
  std::vector<std::string> exp_components;
  NfsUtil::splitNfsPath(nfs_export, exp_components);

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTROOTFH;
  compCall.appendCommand(&carg);

  for (std::string &comp : exp_components)
  {
    nfs_argop4 carg;
    carg.argop = OP_LOOKUP;
    LOOKUP4args *largs = &carg.nfs_argop4_u.oplookup;
    largs->objname.utf8string_len = comp.length();
    largs->objname.utf8string_val = const_cast<char *>(comp.c_str());
    compCall.appendCommand(&carg);
  }

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::getRootFH failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::getRootFH failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call getRootFH failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_GETFH\n");
    return false;
  }

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);
  rootFh = fh;

  return true;
}

bool Nfs4ApiHandle::readDir(std::string &expPath, const std::string &dirPath, NfsFiles &files, NfsError &status)
{
  std::string fullpath;
  NfsFh dirFh;

  if ( dirPath.empty() )
    fullpath = expPath;
  else
    fullpath = expPath + "/" + dirPath;

  if (!getDirFh(fullpath, dirFh, status))
  {
    return false;
  }

  bool sts = false;
  bool ReadDirError = false;
  uint64_t Cookie = 0;
  verifier4 CookieVerf;
  bool eof = false;
  NfsError err;

  memset(CookieVerf, 0, NFS4_VERIFIER_SIZE);

  while (!ReadDirError && !eof)
  {
    if (!readDirV4(dirFh, Cookie, CookieVerf, files, eof, status))
    {
      ReadDirError = true;
    }
  }

  if (!ReadDirError)
    sts = true;

  return sts;
}

bool Nfs4ApiHandle::readDir(NfsFh &dirFh, NfsFiles &files, NfsError &status)
{
  bool sts = false;
  bool ReadDirError = false;
  uint64_t Cookie = 0;
  verifier4 CookieVerf;
  bool eof = false;

  memset(CookieVerf, 0, NFS4_VERIFIER_SIZE);

  while (!ReadDirError && !eof)
  {
    if (!readDirV4(dirFh, Cookie, CookieVerf, files, eof, status))
    {
      ReadDirError = true;
    }
  }

  if (!ReadDirError)
    sts = true;

  return sts;
}

bool Nfs4ApiHandle::readDirV4(NfsFh     &dirFh,
                              uint64_t  &cookie,
                              verifier4 &vref,
                              NfsFiles  &files,
                              bool      &eof,
                              NfsError  &status)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;
  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = dirFh.getLength();
  pfhgargs->object.nfs_fh4_val = dirFh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_READDIR;
  READDIR4args *readdir = &carg.nfs_argop4_u.opreaddir;
  readdir->cookie = (nfs_cookie4)cookie;
  memcpy(&(readdir->cookieverf), &(vref), NFS4_VERIFIER_SIZE);
  readdir->dircount = 8192;
  readdir->maxcount = 8192;
  readdir->attr_request.bitmap4_len = 2;
  readdir->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::readDirV4 failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "nfs v4 readdir failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call failed\n", __func__);
    return false;
  }

  int index = compCall.findOPIndex(OP_READDIR);
  if (index == -1)
  {
    cout << "Failed to find op index for - OP_READDIR" << endl;
    return false;
  }

  READDIR4resok *dir_res = &res.resarray.resarray_val[index].nfs_resop4_u.opreaddir.READDIR4res_u.resok4;
  memcpy(&(vref), &(dir_res->cookieverf), NFS4_VERIFIER_SIZE);

  if (dir_res->reply.eof)
    eof = true;
  else
    eof = false;

  entry4 *dirent = dir_res->reply.entries;

  while(dirent)
  {
    NfsFile file;
    file.cookie = dirent->cookie;
    file.name = std::string(dirent->name.utf8string_val, dirent->name.utf8string_len);
    file.path = "";

    NfsUtil::decode_fattr4(&dirent->attrs, std_attr[0], std_attr[1], file.attr);
    file.type = file.attr.fileType;

    files.push_back(file);
    dirent = dirent->nextentry;
  }

  if (files.size() != 0)
    cookie = files.back().cookie;

  return true;
}

bool Nfs4ApiHandle::getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH, NfsError &status)
{
  if (dirPath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::getDirFh dirPath can not be empty");
    return false;
  }

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(dirPath, path_components);

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = rootFH.getLength();
  pfhgargs->object.nfs_fh4_val = rootFH.getData();
  compCall.appendCommand(&carg);

  for (std::string &comp : path_components)
  {
    nfs_argop4 carg;
    carg.argop = OP_LOOKUP;
    LOOKUP4args *largs = &carg.nfs_argop4_u.oplookup;
    largs->objname.utf8string_len = comp.length();
    largs->objname.utf8string_val = const_cast<char *>(comp.c_str());
    compCall.appendCommand(&carg);
  }

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::getDirFh failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::getDirFh failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call getDirFh failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_GETFH\n");
    return false;
  }

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);
  dirFH = fh;

  return true;
}

bool Nfs4ApiHandle::getDirFh(const std::string &dirPath, NfsFh &dirFH, NfsError &status)
{
  if (dirPath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::getDirFh dir path can not be empty");
    return false;
  }

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(dirPath, path_components);

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTROOTFH;
  compCall.appendCommand(&carg);

  for (std::string &comp : path_components)
  {
    nfs_argop4 carg;
    carg.argop = OP_LOOKUP;
    LOOKUP4args *largs = &carg.nfs_argop4_u.oplookup;
    largs->objname.utf8string_len = comp.length();
    largs->objname.utf8string_val = const_cast<char *>(comp.c_str());
    compCall.appendCommand(&carg);
  }

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::getDirFh failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::getDirFh failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call getRootFH failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_GETFH\n");
    return false;
  }

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);
  dirFH = fh;

  return true;
}

bool Nfs4ApiHandle::getFileHandle(NfsFh             &rootFH,
                                  const std::string  path,
                                  NfsFh             &fileFh,
                                  NfsAttr           &attr,
                                  NfsError          &status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::getFileHandle path can not be empty");
    return false;
  }

  NfsFh tmpFh;
  NfsAttr tmpAttr;
  if (!lookupPath(rootFH, path, tmpFh, tmpAttr, status))
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: lookupPath Failed\n", __func__);
    return false;
  }

  if (tmpAttr.getFileType() == FILE_TYPE_DIR)
  {
    fileFh = tmpFh;
    attr = tmpAttr;
    return true;
  }
  else
  {
    return open(rootFH, path, fileFh, attr, status);
  }
  return true;
}

bool Nfs4ApiHandle::rename(NfsFh &fromDirFh,
                           const std::string &fromName,
                           NfsFh &toDirFh,
                           const std::string toName,
                           NfsError &status)
{
  if (fromName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::rename fromName can not be empty");
    return false;
  }
  if (toName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::rename toName can not be empty");
    return false;
  }

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fromDirFh.getLength();
  pfhgargs->object.nfs_fh4_val = fromDirFh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  carg.argop = OP_SAVEFH;
  compCall.appendCommand(&carg);

  carg.argop = OP_PUTFH;
  pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = toDirFh.getLength();
  pfhgargs->object.nfs_fh4_val = toDirFh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_RENAME;
  RENAME4args *oprename = &carg.nfs_argop4_u.oprename;
  oprename->oldname.utf8string_len = fromName.length();
  oprename->oldname.utf8string_val = const_cast<char *>(fromName.c_str());
  oprename->newname.utf8string_len = toName.length();
  oprename->newname.utf8string_val = const_cast<char *>(toName.c_str());
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::rename failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "NFSV4 call RENAME failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call RENAME failed\n", __func__);
    return false;
  }

  //TODO sarat - do we need the source_info and target_info from rename response??
  return true;
}

/*
  from path and to paths don't contain the export. they are relative paths
 */
bool Nfs4ApiHandle::rename(const std::string &nfs_export,
                           const std::string &fromPath,
                           const std::string &toPath,
                           NfsError          &status)
{
  if (fromPath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::rename fromPath can not be empty");
    return false;
  }
  if (toPath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::rename toPath can not be empty");
    return false;
  }

  std::vector<std::string> from_components;
  NfsUtil::splitNfsPath(fromPath, from_components);

  std::vector<std::string> to_components;
  NfsUtil::splitNfsPath(toPath, to_components);

  std::string fromFile = from_components.back();
  from_components.pop_back();
  std::string toFile = to_components.back();
  to_components.pop_back();

  std::string fromDir, toDir;
  NfsUtil::buildNfsPath(fromDir, from_components);
  NfsUtil::buildNfsPath(toDir, to_components);

  NfsFh rootFh;
  if (!getRootFH(nfs_export, rootFh, status))
    return false;

  NfsFh fromDirFh;

  if (from_components.size())
    getDirFh(rootFh, fromDir, fromDirFh, status);
  else
    fromDirFh = rootFh;

  NfsFh toDirFh;
  if (to_components.size())
    getDirFh(rootFh, toDir, toDirFh, status);
  else
    toDirFh = rootFh;

  return rename(fromDirFh, fromFile, toDirFh, toFile, status);
}

bool Nfs4ApiHandle::commit(NfsFh     &fh,
                           uint64_t  offset,
                           uint32_t  bytes,
                           char      *writeverf,
                           NfsError  &status)
{

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fh.getLength();
  pfhgargs->object.nfs_fh4_val = fh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_COMMIT;
  COMMIT4args *aargs = &carg.nfs_argop4_u.opcommit;
  aargs->offset = offset;
  aargs->count = bytes;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::commit failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::commit failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call COMMIT failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_COMMIT);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_COMMIT\n");
    return false;
  }

  if (writeverf != NULL)
  {
    COMMIT4resok *opres = &res.resarray.resarray_val[index].nfs_resop4_u.opcommit.COMMIT4res_u.resok4;
    memcpy(writeverf, opres->writeverf, NFS4_VERIFIER_SIZE);
  }

  return true;
}

bool Nfs4ApiHandle::access(const std::string &filePath,
                           uint32_t          accessRequested,
                           NfsAccess         &acc,
                           NfsError          &status)
{
  if (filePath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::access filePath can not be empty");
    return false;
  }

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(filePath, path_components);

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTROOTFH;
  compCall.appendCommand(&carg);

  for (std::string &comp : path_components)
  {
    nfs_argop4 carg;
    carg.argop = OP_LOOKUP;
    LOOKUP4args *largs = &carg.nfs_argop4_u.oplookup;
    largs->objname.utf8string_len = comp.length();
    largs->objname.utf8string_val = const_cast<char *>(comp.c_str());
    compCall.appendCommand(&carg);
  }

  carg.argop = OP_ACCESS;
  ACCESS4args *aargs = &carg.nfs_argop4_u.opaccess;
  aargs->access = accessRequested;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::access failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::access failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call ACCESS failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_ACCESS);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_ACCESS\n");
    return false;
  }

  ACCESS4resok *opres = &res.resarray.resarray_val[index].nfs_resop4_u.opaccess.ACCESS4res_u.resok4;
  acc.supported = opres->supported;
  acc.access = opres->access;

  return true;
}

/* File Create API
 * creating a regular file or named attribute needs OPEN call
 */
bool Nfs4ApiHandle::create(NfsFh             &dirFh,
                           std::string       &fileName,
                           NfsAttr           *inAttr,
                           NfsFh             &fileFh,
                           NfsAttr           &outAttr,
                           NfsError          &status)
{
  /*
   * Some operations use seqid's, and these operations must be synchronized. Because if not synchronized,
   * multiple threads may send these operations with different seqid's to server. But the order in which these
   * will be received at the server can change. So, a higher seqid may reach before the lower seqid.
   *
   * In another case if the server delays the processing of the request or for failures with certain erros the client must
   * not increment the seqid's.
   *
   * In order to achieve that we will have these APIs that deal with open seqid's synchronized.
   */
  std::lock_guard<std::mutex> guard(m_file_op_seqid_mutex); // monotonic

  if (fileName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::create fileName can not be empty");
    return false;
  }

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = dirFh.getLength();
  pfhgargs->object.nfs_fh4_val = dirFh.getData();
  compCall.appendCommand(&carg);

  fattr4 obj;

  NfsAttr tmp_attr;
  if (inAttr != NULL)
  {
    NfsUtil::NfsAttr_fattr4(*inAttr, &obj);
  }
  else
  {
    // set the file mode
    tmp_attr.setFileMode(0777);
    NfsUtil::NfsAttr_fattr4(tmp_attr, &obj);
  }

  carg.argop = OP_OPEN;
  OPEN4args *opargs = &carg.nfs_argop4_u.opopen;
  opargs->seqid = m_pConn->getFileOPSeqId();
  opargs->share_access = SHARE_ACCESS_BOTH;
  opargs->share_deny = SHARE_DENY_NONE;
  opargs->owner.clientid = m_pConn->getClientId();
  opargs->owner.owner.owner_len = m_pConn->getClientName().length();
  opargs->owner.owner.owner_val = const_cast<char *>(m_pConn->getClientName().c_str());
  opargs->openhow.opentype = OPEN4_CREATE;
  //createhow
  opargs->openhow.openflag4_u.how.mode = GUARDED4;
  opargs->openhow.openflag4_u.how.createhow4_u.createattrs.attrmask.bitmap4_len = obj.attrmask.bitmap4_len;
  opargs->openhow.openflag4_u.how.createhow4_u.createattrs.attrmask.bitmap4_val = obj.attrmask.bitmap4_val;
  opargs->openhow.openflag4_u.how.createhow4_u.createattrs.attr_vals.attrlist4_len = obj.attr_vals.attrlist4_len;
  opargs->openhow.openflag4_u.how.createhow4_u.createattrs.attr_vals.attrlist4_val = obj.attr_vals.attrlist4_val;
  // claim args
  opargs->claim.claim = CLAIM_NULL;
  //TODO sarat - if there is a claim type then we need to append it
  opargs->claim.open_claim4_u.file.utf8string_len = fileName.length();
  opargs->claim.open_claim4_u.file.utf8string_val = const_cast<char*>(fileName.c_str());
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  carg.argop = OP_ACCESS;
  ACCESS4args *aargs = &carg.nfs_argop4_u.opaccess;
  aargs->access = 0x2D;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::create failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::create OPEN failed");
    if (res.status != NFS4ERR_EXIST)
      syslog(LOG_ERR, "Nfs4ApiHandle::%s: OPEN failed. Error - %d\n", __func__, res.status);

    // Section 9.1.7 of rfc 7530
    if (res.status != NFS4ERR_STALE_CLIENTID && res.status != NFS4ERR_STALE_STATEID && res.status !=  NFS4ERR_BAD_STATEID &&
        res.status != NFS4ERR_BAD_SEQID && res.status != NFS4ERR_BADXDR && res.status != NFS4ERR_RESOURCE &&
        res.status != NFS4ERR_NOFILEHANDLE && res.status != NFS4ERR_MOVED)
    {
      m_pConn->incrementFileOPSeqId();
    }

    return false;
  }
  m_pConn->incrementFileOPSeqId();

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_GETFH\n", __func__);
    return false;
  }

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);

  index = compCall.findOPIndex(OP_OPEN);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_OPEN\n", __func__);
    return false;
  }

  OPEN4resok *opres = &res.resarray.resarray_val[index].nfs_resop4_u.opopen.OPEN4res_u.resok4;
  NfsStateId stateid;
  stateid.seqid = opres->stateid.seqid;
  memcpy(stateid.other, opres->stateid.other, 12);
  fh.setOpenState(stateid);

  // get the rflags
  uint32_t rflags = opres->rflags;
  if (rflags & OPEN4_RESULT_CONFIRM)
  {
    // TODO sarat nfs send open confirmation
  }

  index = compCall.findOPIndex(OP_GETATTR);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_GETATTR\n", __func__);
    return false;
  }

  GETATTR4resok *attr_res = &res.resarray.resarray_val[index].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
  if (NfsUtil::decode_fattr4(&attr_res->obj_attributes, std_attr[0], std_attr[1], outAttr) < 0)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to decode OP_GETATTR result\n", __func__);
    return false;
  }

  fileFh = fh;

  return true;
}

bool Nfs4ApiHandle::open(NfsFh             &rootFh,
                         const std::string  filePath,
                         NfsFh             &fileFh,
                         NfsAttr           &fileAttr,
                         NfsError          &status)
{
  if (filePath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::open filePath can not be empty");
    return false;
  }

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(filePath, path_components);

  std::string fileName = path_components.back();
  path_components.pop_back();

  NfsFh dirFH;
  if (path_components.size() >0)
  {
    std::string dirPath;
    NfsUtil::buildNfsPath(dirPath, path_components);

    if (!getDirFh(rootFh, dirPath, dirFH, status))
    {
      syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to get parent directory FH\n", __func__);
      return false;
    }
  }
  else
  {
    dirFH = rootFh;
  }

  std::lock_guard<std::mutex> guard(m_file_op_seqid_mutex); // monotonic

  // Open the actual file
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = dirFH.getLength();
  pfhgargs->object.nfs_fh4_val = dirFH.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_OPEN;
  OPEN4args *opargs = &carg.nfs_argop4_u.opopen;
  opargs->seqid = m_pConn->getFileOPSeqId();
  opargs->share_access = SHARE_ACCESS_BOTH;
  opargs->share_deny = SHARE_DENY_NONE;
  opargs->owner.clientid = m_pConn->getClientId();
  opargs->owner.owner.owner_len = m_pConn->getClientName().length();
  opargs->owner.owner.owner_val = const_cast<char *>(m_pConn->getClientName().c_str());
  opargs->openhow.opentype = OPEN4_NOCREATE;
  //TODO sarat - if the opentype is OPEN4_CREATE then we need to add createhow4
  opargs->claim.claim = CLAIM_NULL;
  //TODO sarat - if there is a claim type then we need to append it
  opargs->claim.open_claim4_u.file.utf8string_len = fileName.length();
  opargs->claim.open_claim4_u.file.utf8string_val = const_cast<char*>(fileName.c_str());
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  carg.argop = OP_ACCESS;
  ACCESS4args *aargs = &carg.nfs_argop4_u.opaccess;
  aargs->access = 0x2D;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::open failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "NFSV4 OPEN failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: OPEN failed. Error - %d\n", __func__, res.status);

    // Section 9.1.7 of rfc 7530
    if (res.status != NFS4ERR_STALE_CLIENTID && res.status != NFS4ERR_STALE_STATEID && res.status !=  NFS4ERR_BAD_STATEID &&
        res.status != NFS4ERR_BAD_SEQID && res.status != NFS4ERR_BADXDR && res.status != NFS4ERR_RESOURCE &&
        res.status != NFS4ERR_NOFILEHANDLE && res.status != NFS4ERR_MOVED)
    {
      m_pConn->incrementFileOPSeqId();
    }
    return false;
  }
  m_pConn->incrementFileOPSeqId();

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_GETFH\n", __func__);
    return false;
  }

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);

  index = compCall.findOPIndex(OP_OPEN);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_OPEN\n", __func__);
    return false;
  }

  OPEN4resok *opres = &res.resarray.resarray_val[index].nfs_resop4_u.opopen.OPEN4res_u.resok4;
  NfsStateId stateid;
  stateid.seqid = opres->stateid.seqid;
  memcpy(stateid.other, opres->stateid.other, 12);
  fh.setOpenState(stateid);

  // get the rflags
  uint32_t rflags = opres->rflags;
  if (rflags & OPEN4_RESULT_CONFIRM)
  {
    // TODO sarat nfs send open confirmation
  }

  index = compCall.findOPIndex(OP_GETATTR);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_GETATTR\n", __func__);
    return false;
  }

  GETATTR4resok *attr_res = &res.resarray.resarray_val[index].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
  if (NfsUtil::decode_fattr4(&attr_res->obj_attributes, std_attr[0], std_attr[1], fileAttr) < 0)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to decode OP_GETATTR result\n", __func__);
    return false;
  }

  fileFh = fh;

  return true;
}

// filePath is absolute path fs path
bool Nfs4ApiHandle::open(const std::string filePath,
                         uint32_t          access,
                         uint32_t          shareAccess,
                         uint32_t          shareDeny,
                         NfsFh             &fileFh,
                         NfsError          &status)
{
  if (filePath.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::open file path can not be empty");
    return false;
  }

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(filePath, path_components);

  std::string fileName = path_components.back();
  path_components.pop_back();

  std::string dirPath;
  NfsUtil::buildNfsPath(dirPath, path_components);

  NfsFh dirFH;
  if (!getDirFh(dirPath, dirFH, status))
  {
    syslog(LOG_ERR, "Failed to get parent directory FH\n");
    return false;
  }

  std::lock_guard<std::mutex> guard(m_file_op_seqid_mutex); // monotonic

  // Open the actual file
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = dirFH.getLength();
  pfhgargs->object.nfs_fh4_val = dirFH.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  carg.argop = OP_ACCESS;
  ACCESS4args *aargs = &carg.nfs_argop4_u.opaccess;
  aargs->access = access;
  compCall.appendCommand(&carg);

  carg.argop = OP_OPEN;
  OPEN4args *opargs = &carg.nfs_argop4_u.opopen;
  opargs->seqid = m_pConn->getFileOPSeqId();
  opargs->share_access = shareAccess;
  opargs->share_deny = shareDeny;
  opargs->owner.clientid = m_pConn->getClientId();
  opargs->owner.owner.owner_len = m_pConn->getClientName().length();
  opargs->owner.owner.owner_val = const_cast<char *>(m_pConn->getClientName().c_str());
  opargs->openhow.opentype = OPEN4_NOCREATE;
  //TODO sarat - if the opentype is OPEN4_CREATE then we need to add createhow4
  opargs->claim.claim = CLAIM_NULL;
  //TODO sarat - if there is a claim type then we need to append it
  opargs->claim.open_claim4_u.file.utf8string_len = fileName.length();
  opargs->claim.open_claim4_u.file.utf8string_val = const_cast<char*>(fileName.c_str());
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::open failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::open failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call OPEN failed. NFS ERR - %ld\n", __func__, (long)res.status);

    // Section 9.1.7 of rfc 7530
    if (res.status != NFS4ERR_STALE_CLIENTID && res.status != NFS4ERR_STALE_STATEID && res.status !=  NFS4ERR_BAD_STATEID &&
        res.status != NFS4ERR_BAD_SEQID && res.status != NFS4ERR_BADXDR && res.status != NFS4ERR_RESOURCE &&
        res.status != NFS4ERR_NOFILEHANDLE && res.status != NFS4ERR_MOVED)
    {
      m_pConn->incrementFileOPSeqId();
    }
    return false;
  }
  m_pConn->incrementFileOPSeqId();

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_GETFH\n");
    return false;
  }

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);

  index = compCall.findOPIndex(OP_OPEN);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_OPEN\n");
    return false;
  }

  OPEN4resok *opres = &res.resarray.resarray_val[index].nfs_resop4_u.opopen.OPEN4res_u.resok4;
  NfsStateId stateid;
  stateid.seqid = opres->stateid.seqid;
  memcpy(stateid.other, opres->stateid.other, 12);
  fh.setOpenState(stateid);

  // get the rflags
  uint32_t rflags = opres->rflags;
  if (rflags & OPEN4_RESULT_CONFIRM)
  {
    // send open confirmation
  }

  fileFh = fh;

  return true;
}

bool Nfs4ApiHandle::read(NfsFh       &fileFH,
                         uint64_t     offset,
                         uint32_t     length,
                         std::string &data,
                         uint32_t    &bytesRead,
                         bool        &eof,
                         NfsAttr     &postAttr,
                         NfsError    &status)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fileFH.getLength();
  pfhgargs->object.nfs_fh4_val = fileFH.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_READ;
  READ4args *rdargs = &carg.nfs_argop4_u.opread;
  rdargs->offset = offset;
  rdargs->count = length;
  NfsStateId stid;
  if (fileFH.isLocked())
    stid = fileFH.getLockState();
  else
    stid = fileFH.getOpenState();
  rdargs->stateid.seqid =stid.seqid;
  memcpy(rdargs->stateid.other, stid.other, 12);
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::read failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "nfs_v4_read failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call READ failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_READ);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_READ\n");
    return false;
  }

  READ4resok *rdres = &res.resarray.resarray_val[index].nfs_resop4_u.opread.READ4res_u.resok4;

  if (rdres->eof == 1)
    eof = true;
  else
    eof = false;

  if (rdres->data.data_len > 0)
  {
    bytesRead = rdres->data.data_len;
    data = std::string(rdres->data.data_val, rdres->data.data_len);
  }

  index = compCall.findOPIndex(OP_GETATTR);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_GETATTR\n", __func__);
    return false;
  }

  GETATTR4resok *attr_res = &res.resarray.resarray_val[index].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
  if (NfsUtil::decode_fattr4(&attr_res->obj_attributes, std_attr[0], std_attr[1], postAttr) < 0)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to decode OP_GETATTR result\n", __func__);
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::write(NfsFh       &fileFH,
                          uint64_t     offset,
                          uint32_t     length,
                          std::string &data,
                          uint32_t    &bytesWritten,
                          NfsError    &status)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fileFH.getLength();
  pfhgargs->object.nfs_fh4_val = fileFH.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_WRITE;
  WRITE4args *wargs = &carg.nfs_argop4_u.opwrite;
  NfsStateId Stid;
  if (fileFH.isLocked())
    Stid = fileFH.getLockState();
  else
    Stid = fileFH.getOpenState();
  wargs->stateid.seqid = Stid.seqid;
  memcpy(wargs->stateid.other, Stid.other, 12);
  wargs->offset = offset;
  wargs->stable = FILE_SYNC4;
  wargs->data.data_len = length;
  wargs->data.data_val = const_cast<char*>(data.c_str());
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  uint32_t mask[2];
  mask[0] = 0x00000018; mask[1] = 0x00300000;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::write failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, string("NFSV4 write Failed"));
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call WRITE failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_WRITE);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_WRITE\n");
    return false;
  }

  WRITE4resok *wres = &res.resarray.resarray_val[index].nfs_resop4_u.opwrite.WRITE4res_u.resok4;
  bytesWritten = wres->count;

  return true;
}

bool Nfs4ApiHandle::write_unstable(NfsFh       &fileFH,
                                   uint64_t     offset,
                                   std::string &data,
                                   uint32_t    &bytesWritten,
                                   char        *verf,
                                   const bool   needverify,
                                   NfsError    &status)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fileFH.getLength();
  pfhgargs->object.nfs_fh4_val = fileFH.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_WRITE;
  WRITE4args *wargs = &carg.nfs_argop4_u.opwrite;
  NfsStateId Stid;
  if (fileFH.isLocked())
    Stid = fileFH.getLockState();
  else
    Stid = fileFH.getOpenState();
  wargs->stateid.seqid = Stid.seqid;
  memcpy(wargs->stateid.other, Stid.other, 12);
  wargs->offset = offset;
  wargs->stable = UNSTABLE4;
  wargs->data.data_len = data.length();
  wargs->data.data_val = const_cast<char*>(data.c_str());
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  uint32_t mask[2];
  mask[0] = 0x00000018; mask[1] = 0x00300000;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::write_unstable failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::write_unstable failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call WRITE failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_WRITE);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_WRITE\n");
    return false;
  }

  WRITE4resok *wres = &res.resarray.resarray_val[index].nfs_resop4_u.opwrite.WRITE4res_u.resok4;
  bytesWritten = wres->count;

  return true;
}

bool Nfs4ApiHandle::close(NfsFh &fileFH, NfsAttr &postAttr, NfsError &status)
{
  if (!fileFH.isOpen())
    return true;

  std::lock_guard<std::mutex> guard(m_file_op_seqid_mutex); // monotonic

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fileFH.getLength();
  pfhgargs->object.nfs_fh4_val = fileFH.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_CLOSE;
  CLOSE4args *clsargs = &carg.nfs_argop4_u.opclose;
  clsargs->seqid = m_pConn->getFileOPSeqId();
  NfsStateId &opStid = fileFH.getOpenState();
  clsargs->open_stateid.seqid = opStid.seqid;
  memcpy(clsargs->open_stateid.other, opStid.other, 12);
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  uint32_t mask[2];
  mask[0] = 0x00000018; mask[1] = 0x00300000;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::close failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::close failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call CLOSE failed. NFS ERR - %ld\n", __func__, (long)res.status);

    // Section 9.1.7 of rfc 7530
    if (res.status != NFS4ERR_STALE_CLIENTID && res.status != NFS4ERR_STALE_STATEID && res.status !=  NFS4ERR_BAD_STATEID &&
        res.status != NFS4ERR_BAD_SEQID && res.status != NFS4ERR_BADXDR && res.status != NFS4ERR_RESOURCE &&
        res.status != NFS4ERR_NOFILEHANDLE && res.status != NFS4ERR_MOVED)
    {
      m_pConn->incrementFileOPSeqId();
    }
    return false;
  }
  m_pConn->incrementFileOPSeqId();

  int index = compCall.findOPIndex(OP_GETATTR);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_GETATTR\n");
    return false;
  }

  GETATTR4resok *attr_res = &res.resarray.resarray_val[index].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
  if (NfsUtil::decode_fattr4(&attr_res->obj_attributes, mask[0], mask[1], postAttr) < 0)
  {
    syslog(LOG_ERR, "Failed to decode OP_GETATTR result\n");
    return false;
  }

  fileFH.close();

  return true;
}

bool Nfs4ApiHandle::remove(const NfsFh &parentFH, const string &name, NfsError &status)
{
  if (name.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::remove name can not be empty");
    return false;
  }

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = parentFH.getLength();
  pfhgargs->object.nfs_fh4_val = parentFH.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_REMOVE;
  REMOVE4args *rmargs = &carg.nfs_argop4_u.opremove;
  rmargs->target.utf8string_len = name.length();
  rmargs->target.utf8string_val = const_cast<char*>(name.c_str());
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::remove failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, string("NFSV4 remove Failed"));
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call REMOVE using parentFH failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  return true;
}


/*
 * directory will be removed if it is non-empty
 */
bool Nfs4ApiHandle::remove(std::string &exp, std::string path, NfsError &status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::remove path can not be empty");
    return false;
  }

  std::string fullpath = exp + "/" + path;
  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(fullpath, path_components);

  std::string compRemove = path_components.back();
  path_components.pop_back();

  std::string parentPath;
  NfsUtil::buildNfsPath(parentPath, path_components);

  NfsFh parentFH;
  if (!getDirFh(parentPath, parentFH, status))
  {
    syslog(LOG_ERR, "Failed to get parent directory FH\n");
    return false;
  }

  return remove(parentFH, compRemove, status);
}

bool Nfs4ApiHandle::setattr(NfsFh &fh, NfsAttr &attr, NfsError &status)
{

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fh.getLength();
  pfhgargs->object.nfs_fh4_val = fh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_SETATTR;
  SETATTR4args *stargs = &carg.nfs_argop4_u.opsetattr;
  NfsStateId stid;
  if (attr.bSetSize && fh.isLocked())
    stid = fh.getLockState();
  else
    stid = fh.getOpenState();
  stargs->stateid.seqid = stid.seqid;
  memcpy(stargs->stateid.other, stid.other, 12);

  fattr4 obj;
  NfsUtil::NfsAttr_fattr4(attr,&obj);

  stargs->obj_attributes.attrmask.bitmap4_len = obj.attrmask.bitmap4_len;
  stargs->obj_attributes.attrmask.bitmap4_val = obj.attrmask.bitmap4_val;
  stargs->obj_attributes.attr_vals.attrlist4_len = obj.attr_vals.attrlist4_len;
  stargs->obj_attributes.attr_vals.attrlist4_val = obj.attr_vals.attrlist4_val;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::setattr failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::setattr failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call SETATTR failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_SETATTR);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_ACCESS\n");
    return false;
  }

  //bitmap4 *resattr = &res.resarray.resarray_val[index].nfs_resop4_u.opsetattr.attrsset;

  return true;
}

bool Nfs4ApiHandle::truncate(NfsFh &fh, uint64_t size, NfsError &status)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fh.getLength();
  pfhgargs->object.nfs_fh4_val = fh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_SETATTR;
  SETATTR4args *stargs = &carg.nfs_argop4_u.opsetattr;
  NfsStateId stid;
  if (fh.isLocked())
    stid = fh.getLockState();
  else
    stid = fh.getOpenState();
  stargs->stateid.seqid = stid.seqid;
  memcpy(stargs->stateid.other, stid.other, 12);

  uint32_t mask[2] = {0}; mask[0] = (1 << FATTR4_SIZE);
  uint64_t fsize = size;
  stargs->obj_attributes.attrmask.bitmap4_len = 2;
  stargs->obj_attributes.attrmask.bitmap4_val = mask;
  stargs->obj_attributes.attr_vals.attrlist4_len = 8;
  stargs->obj_attributes.attr_vals.attrlist4_val = (char*)&fsize;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::truncate failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::truncate failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call TRUNCATE failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::truncate(const std::string &path, uint64_t size, NfsError &status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::truncate path can not be empty");
    return false;
  }

  //TODO sarat nfs - check if path is dir??

  NfsFh fh;
  if (open(path, ACCESS4_MODIFY, SHARE_ACCESS_READ|SHARE_ACCESS_WRITE, SHARE_DENY_BOTH, fh, status) == false)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::truncate: Failed to open file handle\n");
    return false;
  }

  if (truncate(fh, size, status) == false)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::truncate: Failed to set size for file path - %s\n", path.c_str());
    return false;
  }

  NfsAttr attr;
  if (close(fh, attr, status) == false)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::truncate: Failed to close file handle\n");
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::mkdir(const NfsFh       &parentFH,
                          const std::string  dirName,
                          uint32_t           mode,
                          NfsFh             &dirFH,
                          NfsError          &status)
{
  if (dirName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::mkdir dirName can not be empty");
    return false;
  }

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = parentFH.getLength();
  pfhgargs->object.nfs_fh4_val = parentFH.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_CREATE;
  CREATE4args *crargs = &carg.nfs_argop4_u.opcreate;
  crargs->objtype.type = NF4DIR;
  crargs->objname.utf8string_val = const_cast<char*>(dirName.c_str());
  crargs->objname.utf8string_len = dirName.length();
  uint32_t mask[2] = {0}; mask[1] = 0x00000002;
  crargs->createattrs.attrmask.bitmap4_len = 2;
  crargs->createattrs.attrmask.bitmap4_val = mask;
  crargs->createattrs.attr_vals.attrlist4_len = 4; // only send mode
  crargs->createattrs.attr_vals.attrlist4_val = (char*)&mode;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  uint32_t mask2[2] = {0}; mask2[0] = 0x0010011a; mask2[1] = 0x00b0a23a;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask2;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::mkdir failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::mkdir failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call CREATE failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_GETFH\n");
    return false;
  }

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);
  dirFH = fh;

  return true;
}

bool Nfs4ApiHandle::mkdir(const std::string &path, uint32_t mode, NfsError &status, bool createPath)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::mkdir path can not be empty");
    return false;
  }

  if (!createPath)
  {
    std::vector<std::string> path_components;
    NfsUtil::splitNfsPath(path, path_components);

    std::string dirName = path_components.back();
    path_components.pop_back();

    std::string parentPath;
    NfsUtil::buildNfsPath(parentPath, path_components);

    NfsFh parentFH;
    if (!getDirFh(parentPath, parentFH, status))
    {
      syslog(LOG_ERR, "Failed to get parent directory FH\n");
      return false;
    }

    //TODO sarat nfs - call mkdir
    //TODO sarat - do we need to check access before create?
    NFSv4::COMPOUNDCall compCall;
    enum clnt_stat cst = RPC_SUCCESS;

    nfs_argop4 carg;

    carg.argop = OP_PUTFH;
    PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
    pfhgargs->object.nfs_fh4_len = parentFH.getLength();
    pfhgargs->object.nfs_fh4_val = parentFH.getData();
    compCall.appendCommand(&carg);

    carg.argop = OP_CREATE;
    CREATE4args *crargs = &carg.nfs_argop4_u.opcreate;
    crargs->objtype.type = NF4DIR;
    crargs->objname.utf8string_val = const_cast<char*>(dirName.c_str());
    crargs->objname.utf8string_len = dirName.length();
    uint32_t mask[2] = {0}; mask[1] = 0x00000002;
    crargs->createattrs.attrmask.bitmap4_len = 2;
    crargs->createattrs.attrmask.bitmap4_val = mask;
    crargs->createattrs.attr_vals.attrlist4_len = 4; // only send mode
    crargs->createattrs.attr_vals.attrlist4_val = (char*)&mode;
    compCall.appendCommand(&carg);

    cst = compCall.call(m_pConn);
    if (cst != RPC_SUCCESS)
    {
      status.setRpcError(cst, "Nfs4ApiHandle::mkdir failed - rpc error");
      return false;
    }

    COMPOUND4res res = compCall.getResult();
    if (res.status != NFS4_OK)
    {
      status.setError4(res.status, "Nfs4ApiHandle::mkdir failed");
      syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call CREATE failed. NFS ERR - %ld\n", __func__, (long)res.status);
      return false;
    }
  }
  else
  {
    // TODO sarat - create path components
  }
  return true;
}

bool Nfs4ApiHandle::rmdir(const NfsFh &parentFH, const string &name, NfsError &status)
{
  return remove(parentFH, name, status);
}

bool Nfs4ApiHandle::rmdir(std::string &exp, const std::string &path, NfsError &status)
{
  return remove(exp, path, status);
}

/* To lock the entire file use offset = 0 and length = 0xFFFFFFFFFFFFFFFF
 */
bool Nfs4ApiHandle::lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status, bool reclaim)
{
  std::lock_guard<std::mutex> guard(m_file_op_seqid_mutex); // monotonic

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fh.getLength();
  pfhgargs->object.nfs_fh4_val = fh.getData();
  compCall.appendCommand(&carg);

  uint64_t len = length;
  if (len == 0)
  {
    // In V4 length 0 is invalid, But in V3 it means to the end of file.
    // In V4 eof is identified by all bits set to 1 i.e 0xffffffffffffffff
    len = 0xffffffffffffffff;
  }

  carg.argop = OP_LOCK;
  LOCK4args *lkargs = &carg.nfs_argop4_u.oplock;
  lkargs->locktype = (nfs_lock_type4)lockType;
  lkargs->reclaim = (reclaim) ? 1 : 0;
  lkargs->offset = offset;
  lkargs->length = len;

  if (!reclaim)
  {
    lkargs->locker.new_lock_owner  = 1;
    lkargs->locker.locker4_u.open_owner.open_seqid = m_pConn->getFileOPSeqId();
    NfsStateId &stid = fh.getOpenState();
    lkargs->locker.locker4_u.open_owner.open_stateid.seqid = stid.seqid;
    memcpy(lkargs->locker.locker4_u.open_owner.open_stateid.other, stid.other, 12);
    lkargs->locker.locker4_u.open_owner.lock_seqid = fh.getFileLockSeqId();
    lkargs->locker.locker4_u.open_owner.lock_owner.clientid = m_pConn->getClientId();
    lkargs->locker.locker4_u.open_owner.lock_owner.owner.owner_len = m_pConn->getClientName().length();
    lkargs->locker.locker4_u.open_owner.lock_owner.owner.owner_val = const_cast<char*>(m_pConn->getClientName().c_str());
  }
  else
  {
    // TODO sarat - reclaim the lock
  }
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::lock failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::lock failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call LOCK failed. NFS ERR - %ld\n", __func__, (long)res.status);

    // Section 9.1.7 of rfc 7530
    if (res.status != NFS4ERR_STALE_CLIENTID && res.status != NFS4ERR_STALE_STATEID && res.status !=  NFS4ERR_BAD_STATEID &&
        res.status != NFS4ERR_BAD_SEQID && res.status != NFS4ERR_BADXDR && res.status != NFS4ERR_RESOURCE &&
        res.status != NFS4ERR_NOFILEHANDLE && res.status != NFS4ERR_MOVED)
    {
      m_pConn->incrementFileOPSeqId();
    }
    return false;
  }
  m_pConn->incrementFileOPSeqId();

  int index = compCall.findOPIndex(OP_LOCK);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_LOCK\n");
    return false;
  }

  LOCK4resok *lck_res = &res.resarray.resarray_val[index].nfs_resop4_u.oplock.LOCK4res_u.resok4;
  //LOCK4denied *nlck_res = &res.resarray.resarray_val[index].nfs_resop4_u.oplock.LOCK4res_u.denied;

  NfsStateId stateid;
  stateid.seqid = lck_res->lock_stateid.seqid;
  memcpy(stateid.other, lck_res->lock_stateid.other, 12);
  fh.setLockState(stateid);
  fh.setLocked();

  // TODO sarat - if lock fails we need to get the failure details

  return true;
}

/* To unlock the entire file use offset = 0 and length = 0xFFFFFFFFFFFFFFFF
 */
bool Nfs4ApiHandle::unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fh.getLength();
  pfhgargs->object.nfs_fh4_val = fh.getData();
  compCall.appendCommand(&carg);

  uint64_t len = length;
  if (len == 0)
  {
    // In V4 length 0 is invalid, But in V3 it means to the end of file.
    // In V4 eof is identified by all bits set to 1 i.e 0xffffffffffffffff
    len = 0xffffffffffffffff;
  }

  carg.argop = OP_LOCKU;
  LOCKU4args *ulkargs = &carg.nfs_argop4_u.oplocku;
  ulkargs->locktype = (nfs_lock_type4)lockType;
  ulkargs->seqid = fh.getFileLockSeqId();
  NfsStateId stid = fh.getLockState();
  ulkargs->lock_stateid.seqid = stid.seqid;
  memcpy(ulkargs->lock_stateid.other, stid.other, 12);
  ulkargs->offset = offset;
  ulkargs->length = len;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::unlock failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "Nfs4ApiHandle::unlock failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call LOCKU failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_LOCKU);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_LOCKU\n");
    return false;
  }

  stateid4 *lck_st = &res.resarray.resarray_val[index].nfs_resop4_u.oplocku.LOCKU4res_u.lock_stateid;

  NfsStateId stateid;
  stateid.seqid = lck_st->seqid;
  memcpy(stateid.other, lck_st->other, 12);
  fh.setLockState(stateid);
  fh.setUnlocked();

  return true;
}

bool Nfs4ApiHandle::lookup(NfsFh &dirFh, const std::string &file, NfsFh &lookup_fh, NfsAttr &attr, NfsError &status)
{
  if (file.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::lookup file can not be empty");
    return false;
  }

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = dirFh.getLength();
  pfhgargs->object.nfs_fh4_val = dirFh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_LOOKUP;
  LOOKUP4args *largs = &carg.nfs_argop4_u.oplookup;
  largs->objname.utf8string_len = file.length();
  largs->objname.utf8string_val = const_cast<char *>(file.c_str());
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::lookup failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "NFSV4 call LOOKUP failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: LOOKUP failed. Error - %d\n", __func__, res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_GETFH");
    return false;
  }

  lookup_fh.clear();

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);
  lookup_fh = fh;

  index = compCall.findOPIndex(OP_GETATTR);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_GETATTR\n", __func__);
    return false;
  }

  GETATTR4resok *attr_res = &res.resarray.resarray_val[index].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
  if (NfsUtil::decode_fattr4(&attr_res->obj_attributes, std_attr[0], std_attr[1], attr) < 0)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to decode OP_GETATTR result\n", __func__);
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::lookup(const std::string &path, NfsFh &lookup_fh, NfsError &status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::lookup path can not be empty");
    return false;
  }

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(path, path_components);

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTROOTFH;
  compCall.appendCommand(&carg);

  for (std::string &comp : path_components)
  {
    nfs_argop4 carg;
    carg.argop = OP_LOOKUP;
    LOOKUP4args *largs = &carg.nfs_argop4_u.oplookup;
    largs->objname.utf8string_len = comp.length();
    largs->objname.utf8string_val = const_cast<char *>(comp.c_str());
    compCall.appendCommand(&carg);
  }

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::lookup failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "NFSV4 call LOOKUP failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: LOOKUP failed. Error %d\n", __func__, res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_GETFH\n");
    return false;
  }

  lookup_fh.clear();

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);
  lookup_fh = fh;

  return true;
}

bool Nfs4ApiHandle::lookupPath(const std::string &exp_path,
                               const std::string &pathFromRoot,
                               NfsFh             &lookup_fh,
                               NfsAttr           &lookup_attr,
                               NfsError          &status)
{
  if (pathFromRoot.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::lookupPath pathFromRoot can not be empty");
    return false;
  }

  NfsFh rootFh;
  if (!getRootFH(exp_path, rootFh, status))
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s() failed for getRootFH\n", __func__);
    return false;
  }

  return lookupPath(rootFh, pathFromRoot, lookup_fh, lookup_attr, status);
}

bool Nfs4ApiHandle::lookupPath(NfsFh             &rootFh,
                               const std::string &pathFromRoot,
                               NfsFh             &lookup_fh,
                               NfsAttr           &lookup_attr,
                               NfsError          &status)
{
  if (pathFromRoot.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::lookupPath pathFromRoot can not be empty");
    return false;
  }

  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(pathFromRoot, path_components);

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = rootFh.getLength();
  pfhgargs->object.nfs_fh4_val = rootFh.getData();
  compCall.appendCommand(&carg);

  for (std::string &comp : path_components)
  {
    nfs_argop4 carg;
    carg.argop = OP_LOOKUP;
    LOOKUP4args *largs = &carg.nfs_argop4_u.oplookup;
    largs->objname.utf8string_len = comp.length();
    largs->objname.utf8string_val = const_cast<char *>(comp.c_str());
    compCall.appendCommand(&carg);
  }

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = std_attr;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::lookupPath failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status);
    if (res.status != NFS4ERR_NOENT)
      syslog(LOG_ERR, "Nfs4ApiHandle::%s: LOOKUP failed. Error - %d\n", __func__, res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_GETFH\n", __func__);
    return false;
  }

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);
  lookup_fh = fh;

  index = compCall.findOPIndex(OP_GETATTR);
  if (index == -1)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to find op index for - OP_GETATTR\n", __func__);
    return false;
  }

  GETATTR4resok *attr_res = &res.resarray.resarray_val[index].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
  if (NfsUtil::decode_fattr4(&attr_res->obj_attributes, std_attr[0], std_attr[1], lookup_attr) < 0)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: Failed to decode OP_GETATTR result\n", __func__);
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::fileExists(const std::string& exp, const std::string& path, NfsAttr& attr, NfsError& status)
{
  if (path.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::fileExists path can not be empty");
    return false;
  }

  NfsFh tmpFh;
  bool sts = lookupPath(exp, path, tmpFh, attr, status);

  NfsAttr postAttr;
  NfsError tErr;
  close(tmpFh, postAttr, tErr);

  return sts;
}

bool Nfs4ApiHandle::getAttr(NfsFh &fh, NfsAttr &attr, NfsError &status)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fh.getLength();
  pfhgargs->object.nfs_fh4_val = fh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  uint32_t mask[2] = {0}; mask[0] = 0x0010011a; mask[1] = 0x00b0a23a; // TODO sarat - change the masks to match v3 attrs
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::getAttr failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "nfs v4 getattr failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call GETATTR failed\n", __func__);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETATTR);
  if (index == -1)
  {
    cout << "Failed to find op index for - OP_GETATTR" << endl;
    return false;
  }

  GETATTR4resok *attr_res = &res.resarray.resarray_val[index].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
  if (NfsUtil::decode_fattr4(&attr_res->obj_attributes, mask[0], mask[1], attr) < 0)
  {
    cout << "Failed to decode OP_GETATTR result" << endl;
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::getAttr(const std::string& exp, const std::string& path, NfsAttr& attr, NfsError& status)
{
  NfsFh tmpFh;
  bool sts = lookupPath(exp, path, tmpFh, attr, status);

  NfsAttr postAttr;
  NfsError tErr;
  close(tmpFh, postAttr, tErr);

  return sts;
}

bool Nfs4ApiHandle::fsstat(NfsFh &rootFh, NfsFsStat &stat, uint32 &invarSec, NfsError &status)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = rootFh.getLength();
  pfhgargs->object.nfs_fh4_val = rootFh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = statfs_attr;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::fsstat failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "nfs v4 fsstat failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call FSSTAT failed\n", __func__);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETATTR);
  if (index == -1)
  {
    cout << "Failed to find op index for - OP_GETATTR" << endl;
    return false;
  }

  NfsAttr attr;
  GETATTR4resok *attr_res = &res.resarray.resarray_val[index].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
  if (NfsUtil::decode_fattr4(&attr_res->obj_attributes, statfs_attr[0], statfs_attr[1], attr) < 0)
  {
    cout << "Failed to decode OP_GETATTR result" << endl;
    return false;
  }

  stat.files_avail = attr.getFilesAvailable();
  stat.files_free  = attr.getFilesFree();
  stat.files_total = attr.getFilesTotal();
  stat.bytes_avail = attr.getBytesAvailable();
  stat.bytes_free  = attr.getBytesFree();
  stat.bytes_total = attr.getBytesTotal();

  return true;
}

// hard link
bool Nfs4ApiHandle::link(NfsFh        &tgtFh,
                         NfsFh        &parentFh,
                         const string &linkName,
                         NfsError     &status)
{
  if (linkName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::link linkName can not be empty");
    return false;
  }

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;
  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = tgtFh.getLength();
  pfhgargs->object.nfs_fh4_val = tgtFh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_SAVEFH;
  compCall.appendCommand(&carg);

  carg.argop = OP_PUTFH;
  pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = parentFh.getLength();
  pfhgargs->object.nfs_fh4_val = parentFh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_LINK;
  LINK4args *lnkargs = &carg.nfs_argop4_u.oplink;
  lnkargs->newname.utf8string_len = linkName.length();
  lnkargs->newname.utf8string_val = const_cast<char*>(linkName.c_str());
  compCall.appendCommand(&carg);

  carg.argop = OP_RESTOREFH;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  uint32_t mask[2] = {0}; mask[0] = 0x0010011a; mask[1] = 0x00b0a23a;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::link failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "nfs v4 hard link failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call LINK failed\n", __func__);
    return false;
  }

  return true;
}

// symlink
bool Nfs4ApiHandle::symlink(const string &tgtPath,
                            NfsFh        &parentFh,
                            const string &linkName,
                            NfsError     &status)
{
  if (linkName.empty())
  {
    status.setError(NFSERR_INTERNAL_PATH_EMPTY, "Nfs4ApiHandle::symlink linkName can not be empty");
    return false;
  }

  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;
  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = parentFh.getLength();
  pfhgargs->object.nfs_fh4_val = parentFh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_CREATE;
  CREATE4args *crargs = &carg.nfs_argop4_u.opcreate;
  crargs->objtype.type = NF4LNK;
  crargs->objtype.createtype4_u.linkdata.utf8string_len = tgtPath.length();
  crargs->objtype.createtype4_u.linkdata.utf8string_val = const_cast<char*>(tgtPath.c_str());
  crargs->objname.utf8string_val = const_cast<char*>(linkName.c_str());
  crargs->objname.utf8string_len = linkName.length();
  uint32_t mask[2] = {0}; mask[1] = 0x00000002;
  crargs->createattrs.attrmask.bitmap4_len = 2;
  crargs->createattrs.attrmask.bitmap4_val = mask;
  crargs->createattrs.attr_vals.attrlist4_len = 4;
  uint32_t mode = 0511;
  crargs->createattrs.attr_vals.attrlist4_val = (char*)&mode;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETATTR;
  GETATTR4args *gargs = &carg.nfs_argop4_u.opgetattr;
  mask[0] = 0x0010011a; mask[1] = 0x00b0a23a;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  if (cst != RPC_SUCCESS)
  {
    status.setRpcError(cst, "Nfs4ApiHandle::symlink failed - rpc error");
    return false;
  }

  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    status.setError4(res.status, "nfs v4 symlink failed");
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call CREATE failed\n", __func__);
    return false;
  }

  return true;
}

/* send RENEW client Id every 12 secs to keep the connection alive
 */
bool Nfs4ApiHandle::renewCid()
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_RENEW;
  RENEW4args *renArgs = &carg.nfs_argop4_u.oprenew;
  renArgs->clientid = m_pConn->getClientId();
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call RENEW failed\n", __func__);
    return false;
  }

  //RENEW4res *renRes = &res.resarray.resarray_val[index].nfs_resop4_u.oprenew;

  return true;
}
