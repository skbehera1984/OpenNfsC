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

    /*set the client id to fma as client name */
    carg.argop = OP_SETCLIENTID;
    SETCLIENTID4args *sClIdargs = &carg.nfs_argop4_u.opsetclientid;
    memcpy(sClIdargs->client.verifier,
           m_pConn->getInitialClientVerifier(),
           NFS4_VERIFIER_SIZE);
    char id[4] = "fma";
    id[3] = 0;
    sClIdargs->client.id.id_len = 3;
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

  return true;
}

bool Nfs4ApiHandle::getRootFH(const std::string &nfs_export)
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
  uint32_t mask[2];
  mask[0] = 0x00100012; mask[1] = 0x0030a03a;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call getRootFH failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  int index = compCall.findOPIndex(OP_GETFH);
  if (index == -1)
  {
    syslog(LOG_ERR, "Failed to find op index for - OP_GETFH\n");
    return false;
  }

  m_rootFH.clear();

  GETFH4resok *fetfhgres = &res.resarray.resarray_val[index].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;
  NfsFh fh(fetfhgres->object.nfs_fh4_len, fetfhgres->object.nfs_fh4_val);
  m_rootFH = fh;

  return true;
}

bool Nfs4ApiHandle::parseReadDir(entry4 *entries, uint32_t mask1, uint32_t mask2, NfsFiles &files)
{
  if (entries == NULL)
    return true;

  // always append to files list
  entry4 *dirent = entries;

  while(dirent)
  {
    NfsFile file;
    file.cookie = dirent->cookie;
    file.name = std::string(dirent->name.utf8string_val, dirent->name.utf8string_len);
    file.path = "";

    NfsUtil::decode_fattr4(&dirent->attrs, mask1, mask2, file.attr);
    file.type = file.attr.fileType;

    files.push_back(file);
    dirent = dirent->nextentry;
  }

  return true;
}

bool Nfs4ApiHandle::readDir(const std::string &dirPath, NfsFiles &files)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  std::vector<std::string> components;
  NfsUtil::splitNfsPath(dirPath, components);

  nfs_argop4 carg;

  carg.argop = OP_PUTROOTFH;
  compCall.appendCommand(&carg);

  for (std::string &comp : components)
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
  uint32_t mask[2];
  mask[0] = 0x00100012; mask[1] = 0x0030a03a;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call failed. NFS ERR - %ld\n", __func__, (long)res.status);
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

  uint32 eof = 0;
  verifier4 scvref;
  memset(scvref, 0, NFS4_VERIFIER_SIZE);
  uint64_t  scookie = 0;

  while (eof == 0)
  {
    compCall.clear();

    carg.argop = OP_PUTFH;
    PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
    pfhgargs->object.nfs_fh4_len = fh.getLength();
    pfhgargs->object.nfs_fh4_val = fh.getData();
    compCall.appendCommand(&carg);

    carg.argop = OP_GETATTR;
    gargs = &carg.nfs_argop4_u.opgetattr;
    gargs->attr_request.bitmap4_len = 2;
    gargs->attr_request.bitmap4_val = mask;
    compCall.appendCommand(&carg);

    carg.argop = OP_GETFH;
    compCall.appendCommand(&carg);

    carg.argop = OP_READDIR;
    READDIR4args *readdir = &carg.nfs_argop4_u.opreaddir;
    readdir->cookie = (nfs_cookie4)scookie;
    memcpy(readdir->cookieverf, scvref, NFS4_VERIFIER_SIZE);
    readdir->dircount = 8192;
    readdir->maxcount = 8192;
    readdir->attr_request.bitmap4_len = 2;
    readdir->attr_request.bitmap4_val = mask;
    compCall.appendCommand(&carg);

    cst = compCall.call(m_pConn);
    res = compCall.getResult();
    if (res.status != NFS4_OK)
    {
      syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call failed. NFS ERR - %ld\n", __func__, (long)res.status);
      return false;
    }

    index = compCall.findOPIndex(OP_READDIR);
    if (index == -1)
    {
      syslog(LOG_ERR, "Failed to find op index for - OP_READDIR\n");
      return false;
    }

    READDIR4resok *dir_res = &res.resarray.resarray_val[index].nfs_resop4_u.opreaddir.READDIR4res_u.resok4;
    memcpy(scvref, dir_res->cookieverf, NFS4_VERIFIER_SIZE);
    eof = dir_res->reply.eof;

    if (!parseReadDir(dir_res->reply.entries, mask[0], mask[1], files))
    {
      syslog(LOG_ERR, "Failed to parse READDIR entries\n");
      return false;
    }

    if (files.size() != 0)
      scookie = files.back().cookie;
  }

  return true;
}

bool Nfs4ApiHandle::getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH)
{
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
  uint32_t mask[2];
  mask[0] = 0x00100012; mask[1] = 0x0030a03a;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
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

bool Nfs4ApiHandle::getDirFh(const std::string &dirPath, NfsFh &dirFH)
{
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
  uint32_t mask[2];
  mask[0] = 0x00100012; mask[1] = 0x0030a03a;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
  compCall.appendCommand(&carg);

  carg.argop = OP_GETFH;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
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

/*
  from path and to paths don't contain the export. they are relative paths
 */
bool Nfs4ApiHandle::rename(const std::string &nfs_export,
                           const std::string &fromPath,
                           const std::string &toPath)
{
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

  getRootFH(nfs_export);

  NfsFh fromDirFh;

  if (from_components.size())
    getDirFh(m_rootFH, fromDir, fromDirFh);
  else
    fromDirFh = m_rootFH;

  NfsFh toDirFh;
  if (to_components.size())
    getDirFh(m_rootFH, toDir, toDirFh);
  else
    toDirFh = m_rootFH;

  // call rename
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
  uint32_t mask[2];
  mask[0] = 0x00100012; mask[1] = 0x0030a03a;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
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
  oprename->oldname.utf8string_len = fromFile.length();
  oprename->oldname.utf8string_val = const_cast<char *>(fromFile.c_str());
  oprename->newname.utf8string_len = toFile.length();
  oprename->newname.utf8string_val = const_cast<char *>(toFile.c_str());
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call RENAME failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  //TODO sarat - do we need the source_info and target_info from rename response??
  return true;
}

bool Nfs4ApiHandle::commit(NfsFh             &fh,
                           uint64_t          offset,
                           uint32_t          bytes,
                           char              *writeverf)
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
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
                           NfsAccess         &acc)
{
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
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

// filePath is absolute path fs path
bool Nfs4ApiHandle::open(const std::string filePath,
                         uint32_t          access,
                         uint32_t          shareAccess,
                         uint32_t          shareDeny,
                         NfsFh             &fileFh)
{
  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(filePath, path_components);

  std::string fileName = path_components.back();
  path_components.pop_back();

  std::string dirPath;
  NfsUtil::buildNfsPath(dirPath, path_components);

  NfsFh dirFH;
  if (!getDirFh(dirPath, dirFH))
  {
    syslog(LOG_ERR, "Failed to get parent directory FH\n");
    return false;
  }

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
  uint32_t mask[2];
  mask[0] = 0x00100012; mask[1] = 0x0030a03a;
  gargs->attr_request.bitmap4_len = 2;
  gargs->attr_request.bitmap4_val = mask;
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call OPEN failed. NFS ERR - %ld\n", __func__, (long)res.status);
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
                         bool        &eof)
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
  NfsStateId &stid = fileFH.getOpenState();
  rdargs->stateid.seqid =stid.seqid;
  memcpy(rdargs->stateid.other, stid.other, 12);
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
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

  return true;
}

bool Nfs4ApiHandle::write(NfsFh       &fileFH,
                          uint64_t     offset,
                          std::string &data,
                          uint32_t    &bytesWritten,
                          NfsAttr     &postAttr)
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
  NfsStateId &Stid = fileFH.getOpenState();
  wargs->stateid.seqid = Stid.seqid;
  memcpy(wargs->stateid.other, Stid.other, 12);
  wargs->offset = offset;
  wargs->stable = FILE_SYNC4;
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
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
  //stable_how4 commited = wres->committed;

  //TODO sarat - get the post attr
  return true;
}

bool Nfs4ApiHandle::write_unstable(NfsFh       &fileFH,
                                   uint64_t     offset,
                                   std::string &data,
                                   uint32_t    &bytesWritten,
                                   NfsAttr     &postAttr)
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
  NfsStateId &Stid = fileFH.getOpenState();
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
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
  //stable_how4 commited = wres->committed;

  //TODO Rabi - get the post attr
  return true;
}

bool Nfs4ApiHandle::close(NfsFh &fileFH, NfsAttr &postAttr)
{
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call CLOSE failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

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

  return true;
}

bool Nfs4ApiHandle::remove(const NfsFh &parentFH, const string &name)
{
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call REMOVE using parentFH failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  return true;
}


/*
 * directory will be removed if it is non-empty
 */
bool Nfs4ApiHandle::remove(std::string path)
{
  std::vector<std::string> path_components;
  NfsUtil::splitNfsPath(path, path_components);

  std::string compRemove = path_components.back();
  path_components.pop_back();

  std::string parentPath;
  NfsUtil::buildNfsPath(parentPath, path_components);

  NfsFh parentFH;
  if (!getDirFh(parentPath, parentFH))
  {
    syslog(LOG_ERR, "Failed to get parent directory FH\n");
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
  rmargs->target.utf8string_len = compRemove.length();
  rmargs->target.utf8string_val = const_cast<char*>(compRemove.c_str());
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call REMOVE failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::setattr( NfsFh &fh, NfsAttr &attr )
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
  NfsStateId &stid = fh.getOpenState();
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
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

bool Nfs4ApiHandle::truncate(NfsFh &fh, uint64_t size)
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
  NfsStateId &stid = fh.getOpenState();
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call TRUNCATE failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::truncate(const std::string &path, uint64_t size)
{
  NfsFh fh;
  if (open(path, ACCESS4_MODIFY, SHARE_ACCESS_READ|SHARE_ACCESS_WRITE, SHARE_DENY_BOTH, fh) == false)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::truncate: Failed to open file handle\n");
    return false;
  }

  if (truncate(fh, size) == false)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::truncate: Failed to set size for file path - %s\n", path.c_str());
    return false;
  }

  NfsAttr attr;
  if (close(fh, attr) == false)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::truncate: Failed to close file handle\n");
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::mkdir(const NfsFh &parentFH, const std::string dirName, uint32_t mode)
{
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call CREATE failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

  return true;
}

bool Nfs4ApiHandle::mkdir(const std::string &path, uint32_t mode, bool createPath)
{
  if (!createPath)
  {
    std::vector<std::string> path_components;
    NfsUtil::splitNfsPath(path, path_components);

    std::string dirName = path_components.back();
    path_components.pop_back();

    std::string parentPath;
    NfsUtil::buildNfsPath(parentPath, path_components);

    NfsFh parentFH;
    if (!getDirFh(parentPath, parentFH))
    {
      syslog(LOG_ERR, "Failed to get parent directory FH\n");
      return false;
    }

    //TODO sarat - do we need to check access befire create?
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
    COMPOUND4res res = compCall.getResult();
    if (res.status != NFS4_OK)
    {
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

bool Nfs4ApiHandle::rmdir(const std::string &path)
{
  return remove(path);
}

/* To lock the entire file use offset = 0 and length = 0xFFFFFFFFFFFFFFFF
 */
bool Nfs4ApiHandle::lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, bool reclaim)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fh.getLength();
  pfhgargs->object.nfs_fh4_val = fh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_LOCK;
  LOCK4args *lkargs = &carg.nfs_argop4_u.oplock;
  lkargs->locktype = (nfs_lock_type4)lockType;
  lkargs->reclaim = (reclaim) ? 1 : 0;
  lkargs->offset = offset;
  lkargs->length = length;

  if (!reclaim)
  {
    lkargs->locker.new_lock_owner  = 1;
    lkargs->locker.locker4_u.open_owner.open_seqid = m_pConn->getFileOPSeqId();
    NfsStateId &stid = fh.getOpenState();
    lkargs->locker.locker4_u.open_owner.open_stateid.seqid = stid.seqid;
    memcpy(lkargs->locker.locker4_u.open_owner.open_stateid.other, stid.other, 12);
    lkargs->locker.locker4_u.open_owner.lock_seqid = m_pConn->getFileLockSeqId();
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
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
    syslog(LOG_ERR, "Nfs4ApiHandle::%s: NFSV4 call LOCK failed. NFS ERR - %ld\n", __func__, (long)res.status);
    return false;
  }

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

  // TODO sarat - if lock fails we need to get the failure details

  return true;
}

/* To unlock the entire file use offset = 0 and length = 0xFFFFFFFFFFFFFFFF
 */
bool Nfs4ApiHandle::unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length)
{
  NFSv4::COMPOUNDCall compCall;
  enum clnt_stat cst = RPC_SUCCESS;

  nfs_argop4 carg;

  carg.argop = OP_PUTFH;
  PUTFH4args *pfhgargs = &carg.nfs_argop4_u.opputfh;
  pfhgargs->object.nfs_fh4_len = fh.getLength();
  pfhgargs->object.nfs_fh4_val = fh.getData();
  compCall.appendCommand(&carg);

  carg.argop = OP_LOCKU;
  LOCKU4args *ulkargs = &carg.nfs_argop4_u.oplocku;
  ulkargs->locktype = (nfs_lock_type4)lockType;
  ulkargs->seqid = m_pConn->getFileLockSeqId();
  NfsStateId stid = fh.getLockState();
  ulkargs->lock_stateid.seqid = stid.seqid;
  memcpy(ulkargs->lock_stateid.other, stid.other, 12);
  ulkargs->offset = offset;
  ulkargs->length = length;
  compCall.appendCommand(&carg);

  cst = compCall.call(m_pConn);
  COMPOUND4res res = compCall.getResult();
  if (res.status != NFS4_OK)
  {
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

  return true;
}

/* send RENEW client Id every 12 secs to keep the connection alive
 */
