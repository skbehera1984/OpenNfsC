#include "DataTypes.h"
#include <nfsrpc/nfs4.h>
#include <time.h>
#include <iostream>
#include <string>

using namespace std;
using namespace OpenNfsC;

void NfsFh::clear()
{
  if (fhVal != NULL)
  {
    free(fhVal);
    fhVal = NULL;
  }
  fhLen = 0;

  OSID.seqid = 0;
  memset(OSID.other, 0, 12);
  LSID.seqid = 0;
  memset(LSID.other, 0, 12);
}

NfsFh::NfsFh()
{
  fhLen = 0;
  fhVal = NULL;
  OSID.seqid = 0;
  memset(OSID.other, 0, 12);
  LSID.seqid = 0;
  memset(LSID.other, 0, 12);
}

NfsFh::NfsFh(uint32_t len, const char *val)
{
  fhLen = len;
  fhVal = (char *)malloc(len);
  if (fhVal == NULL)
    throw std::string("Failed to allocate NfsFh");
  memcpy(fhVal, val, len);

  OSID.seqid = 0;
  memset(OSID.other, 0, 12);
  LSID.seqid = 0;
  memset(LSID.other, 0, 12);
}

NfsFh::NfsFh(const NfsFh &fromFH)
{
  // copy fromFH to this
  fhLen = fromFH.fhLen;
  if (fromFH.fhVal != NULL)
  {
    fhVal = (char *)malloc(fhLen);
    if (fhVal == NULL)
      throw std::string("NfsFh::assign Failed to allocate buffer");

    memcpy(fhVal, fromFH.fhVal , fhLen);
  }

  OSID.seqid = fromFH.OSID.seqid;
  memcpy(OSID.other, fromFH.OSID.other, 12);
  LSID.seqid = fromFH.LSID.seqid;
  memcpy(LSID.other, fromFH.LSID.other, 12);
}

const NfsFh& NfsFh::operator=(const NfsFh &fromFH)
{
  // get rid of any old value we have before we copy
  clear();

  // copy fromFH to this
  fhLen = fromFH.fhLen;
  if (fromFH.fhVal != NULL)
  {
    fhVal = (char *)malloc(fhLen);
    if (fhVal == NULL)
      throw std::string("NfsFh::assign Failed to allocate buffer");

    memcpy(fhVal, fromFH.fhVal , fhLen);
  }

  OSID.seqid = fromFH.OSID.seqid;
  memcpy(OSID.other, fromFH.OSID.other, 12);
  LSID.seqid = fromFH.LSID.seqid;
  memcpy(LSID.other, fromFH.LSID.other, 12);

  return *this;
}

void NfsFh::setOpenState(NfsStateId& opSt)
{
  OSID.seqid = opSt.seqid;
  memcpy(OSID.other, opSt.other, 12);
}

void NfsFh::setLockState(NfsStateId& lkSt)
{
  LSID.seqid = lkSt.seqid;
  memcpy(LSID.other, lkSt.other, 12);
}

void NfsAttr::clear()
{
  mask[0] = 0; mask[1] = 0;
  fileType = FILE_TYPE_NON;
  fmode = 0;
  bSetMode = false;
  nlinks = 0;
  uid = 0;
  bSetUid = false;
  gid = 0;
  bSetGid = false;
  size = 0;
  bSetSize = false;
  rawDevice = 0;
  fsid.FSIDMajor = 0;
  fsid.FSIDMinor = 0;
  fid = 0;
  time_access.seconds = 0;
  time_access.nanosecs = 0;
  bSetAtime = false;
  aTimeHow = NFS_TIME_DONT_CHANGE;
  time_metadata.seconds = 0;
  time_metadata.nanosecs = 0;
  bSetCtime = false;
  cTimeHow = NFS_TIME_DONT_CHANGE;
  time_modify.seconds = 0;
  time_modify.nanosecs = 0;
  bSetMtime = false;
  mTimeHow = NFS_TIME_DONT_CHANGE;

  owner = std::string("");
  group = std::string("");
  mountFid = 0;
  changeID = 0;
  name_max = 0;
  files_avail = 0;
  files_free = 0;
  files_total = 0;
  bytes_avail = 0;
  bytes_free = 0;
  bytes_total = 0;
  bytes_used = 0;

  memset(&lattr, 0, sizeof(lookup_attr));
  lattr.obj_attr_present = false;
  lattr.dir_attr_present = false;
}

NfsAttr::NfsAttr()
{
  clear();
}

int NfsAttr::Fattr3ToNfsAttr(fattr3 *attr)
{
  switch (attr->fattr3_type)
  {
    case NF3NON:
      fileType = FILE_TYPE_NON;
    break;
    case NF3REG:
      fileType = FILE_TYPE_REG;
    break;
    case NF3DIR:
      fileType = FILE_TYPE_DIR;
    break;
    case NF3BLK:
      fileType = FILE_TYPE_BLK;
    break;
    case NF3CHR:
      fileType = FILE_TYPE_CHR;
    break;
    case NF3LNK:
      fileType = FILE_TYPE_LNK;
    break;
    case NF3SOCK:
      fileType = FILE_TYPE_SOCK;
    break;
    case NF3FIFO:
      fileType = FILE_TYPE_SOCK;
    break;
  }

  fmode = attr->fattr3_mode;
  nlinks = attr->fattr3_nlink;
  uid = attr->fattr3_uid;
  gid = attr->fattr3_gid;
  owner = std::to_string(attr->fattr3_uid);
  group = std::to_string(attr->fattr3_gid);
  size = attr->fattr3_size;
  bytes_used = attr->fattr3_used;
  // don't need rdev
  // TODO sarat how to convert fsid ??
  fid = attr->fattr3_fileid;
  time_access.seconds = attr->fattr3_atime.time3_seconds;
  time_access.nanosecs = attr->fattr3_atime.time3_nseconds;
  time_modify.seconds = attr->fattr3_mtime.time3_seconds;
  time_modify.nanosecs = attr->fattr3_mtime.time3_nseconds;
  time_metadata.seconds = attr->fattr3_ctime.time3_seconds;
  time_metadata.nanosecs = attr->fattr3_ctime.time3_nseconds;

  return 0;
}

int NfsAttr::NfsAttrToSattr3(sattr3 *sattr)
{
  if (bSetMode)
  {
    sattr->sattr3_mode.set_it = 1;
    sattr->sattr3_mode.set_mode3_u.mode = fmode;
  }
  else
    sattr->sattr3_mode.set_it = 0;

  if (bSetUid)
  {
    sattr->sattr3_uid.set_it = 1;
    sattr->sattr3_uid.set_uid3_u.uid = uid;
  }
  else
    sattr->sattr3_uid.set_it = 0;

  if (bSetGid)
  {
    sattr->sattr3_gid.set_it = 1;
    sattr->sattr3_gid.set_gid3_u.gid = gid;
  }
  else
    sattr->sattr3_gid.set_it = 0;

  if (bSetSize)
  {
    sattr->sattr3_size.set_it = 1;
    sattr->sattr3_size.set_size3_u.size = size;
  }
  else
    sattr->sattr3_size.set_it = 0;

  if (bSetAtime)
  {
    sattr->sattr3_atime.set_it = (time_how)aTimeHow;
    sattr->sattr3_atime.set_atime_u.atime.time3_seconds = time_access.seconds;
    sattr->sattr3_atime.set_atime_u.atime.time3_nseconds= time_access.nanosecs;
  }
  else
    sattr->sattr3_atime.set_it = TIME_DONT_CHANGE;

  if (bSetMtime)
  {
    sattr->sattr3_mtime.set_it = (time_how)mTimeHow;
    sattr->sattr3_mtime.set_mtime_u.mtime.time3_seconds = time_modify.seconds;
    sattr->sattr3_mtime.set_mtime_u.mtime.time3_nseconds= time_modify.nanosecs;
  }
  else
    sattr->sattr3_mtime.set_it = TIME_DONT_CHANGE;

  return 0;
}

void NfsAttr::print() const
{
  cout << "==================================================" << endl;
  if (mask[0] & (1 << FATTR4_SUPPORTED_ATTRS))
  {
  }
  if (mask[0] & (1 << FATTR4_TYPE))
  {
    std::string type;
    if (fileType == FILE_TYPE_REG)
      type = "File";
    else if (fileType == FILE_TYPE_DIR)
      type = "Directory";
    else if (fileType == FILE_TYPE_BLK)
      type = "Block Device";
    else if (fileType == FILE_TYPE_CHR)
      type = "Character Device";
    else if (fileType == FILE_TYPE_LNK)
      type = "Link";
    else if (fileType == FILE_TYPE_SOCK)
      type = "Socket";
    else if (fileType == FILE_TYPE_FIFO)
      type = "FIFO";
    cout << "Type : " << type << endl;
  }
  if (mask[0] & (1 << FATTR4_FH_EXPIRE_TYPE))
  {
  }
  if (mask[0] & (1 << FATTR4_CHANGE))
  {
    cout << "ChangeID : " << changeID << endl;
  }
  if (mask[0] & (1 << FATTR4_SIZE))
  {
    cout << "Size : " << size << endl;
  }
  if (mask[0] & (1 << FATTR4_LINK_SUPPORT))
  {
  }
  if (mask[0] & (1 << FATTR4_SYMLINK_SUPPORT))
  {
  }
  if (mask[0] & (1 << FATTR4_NAMED_ATTR))
  {
  }
  if (mask[0] & (1 << FATTR4_FSID))
  {
    cout << "FSIDMajor : " << fsid.FSIDMajor << endl;
    cout << "FSIDMinor : " << fsid.FSIDMinor << endl;
  }
  if (mask[0] & (1 << FATTR4_UNIQUE_HANDLES))
  {
  }
  if (mask[0] & (1 << FATTR4_LEASE_TIME))
  {
  }
  if (mask[0] & (1 << FATTR4_RDATTR_ERROR))
  {
  }
  if (mask[0] & (1 << FATTR4_FILEHANDLE))
  {
  }
  if (mask[0] & (1 << FATTR4_ACL))
  {
  }
  if (mask[0] & (1 << FATTR4_ACLSUPPORT))
  {
  }
  if (mask[0] & (1 << FATTR4_ARCHIVE))
  {
  }
  if (mask[0] & (1 << FATTR4_CANSETTIME))
  {
  }
  if (mask[0] & (1 << FATTR4_CASE_INSENSITIVE))
  {
  }
  if (mask[0] & (1 << FATTR4_CASE_PRESERVING))
  {
  }
  if (mask[0] & (1 << FATTR4_CHOWN_RESTRICTED))
  {
  }
  if (mask[0] & (1 << FATTR4_FILEID))
  {
    cout << "FileID : " << fid << endl;
  }
  if (mask[0] & (1 << FATTR4_FILES_AVAIL))
  {
  }
  if (mask[0] & (1 << FATTR4_FILES_FREE))
  {
  }
  if (mask[0] & (1 << FATTR4_FILES_TOTAL))
  {
  }
  if (mask[0] & (1 << FATTR4_FS_LOCATIONS))
  {
  }
  if (mask[0] & (1 << FATTR4_HIDDEN))
  {
  }
  if (mask[0] & (1 << FATTR4_HOMOGENEOUS))
  {
  }
  if (mask[0] & (1 << FATTR4_MAXFILESIZE))
  {
  }
  if (mask[0] & (1 << FATTR4_MAXLINK))
  {
  }
  if (mask[0] & (1 << FATTR4_MAXNAME))
  {
  }
  if (mask[0] & (1 << FATTR4_MAXREAD))
  {
  }
  if (mask[0] & (1 << FATTR4_MAXWRITE))
  {
  }
  if (mask[1] & (1 << (FATTR4_MIMETYPE - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_MODE - 32)))
  {
    cout << "Mode : " << fmode << endl;
  }
  if (mask[1] & (1 << (FATTR4_NO_TRUNC - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_NUMLINKS - 32)))
  {
    cout << "No Of Links : " << nlinks << endl;
  }
  if (mask[1] & (1 << (FATTR4_OWNER - 32)))
  {
    cout << "Owner : " << owner << endl;
  }
  if (mask[1] & (1 << (FATTR4_OWNER_GROUP - 32)))
  {
    cout << "Owner Group : " << group << endl;
  }
  if (mask[1] & (1 << (FATTR4_QUOTA_AVAIL_HARD - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_QUOTA_AVAIL_SOFT - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_QUOTA_USED - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_RAWDEV - 32)))
  {
    cout << "RawDevice : " << rawDevice << endl;
  }
  if (mask[1] & (1 << (FATTR4_SPACE_AVAIL - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_SPACE_FREE - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_SPACE_TOTAL - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_SPACE_USED - 32)))
  {
    cout << "Bytes Used : " << bytes_used << endl;
  }
  if (mask[1] & (1 << (FATTR4_SYSTEM - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_TIME_ACCESS - 32)))
  {
    time_t t = (time_t)time_access.seconds;
    cout << "ACCESS TIME :" << asctime(localtime(&t)) << endl;
  }
  if (mask[1] & (1 << (FATTR4_TIME_ACCESS_SET - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_TIME_BACKUP - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_TIME_CREATE - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_TIME_DELTA - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_TIME_METADATA - 32)))
  {
    time_t t = (time_t)time_metadata.seconds;
    cout << "TIME METADATA :" << asctime(localtime(&t)) << endl;
  }
  if (mask[1] & (1 << (FATTR4_TIME_MODIFY - 32)))
  {
    time_t t = (time_t)time_modify.seconds;
    cout << "TIME MODIFY :" << asctime(localtime(&t)) << endl;
  }
  if (mask[1] & (1 << (FATTR4_TIME_MODIFY_SET - 32)))
  {
  }
  if (mask[1] & (1 << (FATTR4_MOUNTED_ON_FILEID - 32)))
  {
    cout << "MountedFid : " << mountFid << endl;
  }
  cout << "==================================================" << endl;
}

NfsError::NfsError(const NfsError &obj)
{
  this->etype = obj.etype;
  this->err   = obj.err;
  this->err3  = obj.err3;
  this->err4  = obj.err4;
  this->msg   = obj.msg;
}

NfsError& NfsError::operator=(const NfsError &obj)
{
  etype = obj.etype;
  err   = obj.err;
  err3  = obj.err3;
  err4  = obj.err4;
  msg   =   obj.msg;

  return (*this);
}

bool NfsError::operator==(const NfsError &obj)const
{
  if((this->etype  == obj.etype)  &&
      (this->err   == obj.err)    &&
      (this->err3  == obj.err3)  &&
      (this->err4  == obj.err4)  &&
      (this->msg   == obj.msg))
    return true;
  else
    return false;
}

bool NfsError::operator==(const bool &val)const
{
  if (val == true)
  {
    if (err == 0 && err3 == nfsstat3::NFS3_OK && err4 == nfsstat4::NFS4_OK)
      return true;
    else
      return false;
  }
  else
  {
    if (err != 0 || err3 != nfsstat3::NFS3_OK || err4 != nfsstat4::NFS4_OK)
      return true;
    else
      return false;
  }
}

void NfsError::clear()
{
  etype = EType::ETYPE_INTERNAL;
  err   = 0;
  err3  = nfsstat3::NFS3_OK;
  err4  = nfsstat4::NFS4_OK;
  msg.clear();
}

void NfsError::setError4(nfsstat4 code, const std::string &err)
{
  etype = ETYPE_V4;
  err4  = code;
  msg   = err;
}

void NfsError::setError3(nfsstat3 code, const std::string &err)
{
  etype = ETYPE_V3;
  err3  = code;
  msg   = err;
}

void NfsError::setError(uint32_t code, const std::string &emsg)
{
  etype = ETYPE_INTERNAL;
  err   = code;
  msg   = emsg;
}

uint32_t NfsError::getErrorCode()
{
  if (etype == ETYPE_INTERNAL)
    return err;
  else if (etype == ETYPE_V3)
    return err3;
  else if (etype == ETYPE_V4)
    return err4;
  else
    return 0;
}
