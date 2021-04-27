#include "DataTypes.h"
#include <nfsrpc/nfs4.h>
#include <time.h>
#include <iostream>
#include <string>

using namespace std;
using namespace OpenNfsC;

bool NfsFile::isDirectory()
{
  return (type == FILE_TYPE_DIR);
}

bool NfsFile::isSymlink()
{
  return (type == FILE_TYPE_LNK);
}

bool NfsFile::isZeroByteFile()
{
  if ( !attr.empty() && (attr.getFileType() == FILE_TYPE_REG) )
  {
    if (attr.getSize() == 0)
      return true;
    else
    {
      if (attr.getSizeUsed() == 0)
      {
      }
    }
  }
  return false;
}

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
  locked = false;
  m_file_lock_seqid = 0;
  m_opened = false;
}

NfsFh::NfsFh()
{
  fhLen = 0;
  fhVal = NULL;
  OSID.seqid = 0;
  memset(OSID.other, 0, 12);
  LSID.seqid = 0;
  memset(LSID.other, 0, 12);
  locked = false;
  m_file_lock_seqid = 0;
  m_opened = false;
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
  locked = false;
  m_file_lock_seqid = 0;
  m_opened = false;
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
  locked = fromFH.locked;
  m_file_lock_seqid = fromFH.m_file_lock_seqid;
  m_opened = fromFH.m_opened;
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
  locked = fromFH.locked;
  m_file_lock_seqid = fromFH.m_file_lock_seqid;
  m_opened = fromFH.m_opened;

  return *this;
}

bool NfsFh::operator==(const NfsFh &fromFH)
{
  bool tRet = false;

  if ( (fhLen == fromFH.fhLen) &&
       (memcmp(fhVal,fromFH.fhVal,fromFH.fhLen) == 0 ))
  {
    tRet = true;
  }
  return tRet;
}

void NfsFh::setOpenState(NfsStateId& opSt)
{
  OSID.seqid = opSt.seqid;
  memcpy(OSID.other, opSt.other, 12);
  m_opened = true;
}

void NfsFh::setLockState(NfsStateId& lkSt)
{
  LSID.seqid = lkSt.seqid;
  memcpy(LSID.other, lkSt.other, 12);
}

uint32_t NfsFh::getFileLockSeqId()
{
  std::lock_guard<std::mutex> guard(m_lock_seqid_mutex);
  uint32_t seqid = m_file_lock_seqid;
  ++m_file_lock_seqid;
  return seqid;
}

void NfsAttr::clear()
{
  mask[0] = 0; mask[1] = 0;
  fileType = FILE_TYPE_NON;
  fmode = 0;
  bSetMode = false;
  nlinks = 0;
  owner.clear();
  bSetUid = false;
  group.clear();
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

  memset(m_buf, 0, sizeof(m_buf));
}

NfsAttr::NfsAttr()
{
  clear();
}

NfsAttr::~NfsAttr()
{
  clear();
}

bool NfsAttr::empty()
{
  return (fileType == FILE_TYPE_NON);
}

NfsAttr::NfsAttr(const NfsAttr &obj)
{
  this->mask[0] = obj.mask[0];
  this->mask[1] = obj.mask[1];
  this->fileType = obj.fileType;
  this->fmode = obj.fmode;
  this->bSetMode = obj.bSetMode;
  this->nlinks = obj.nlinks;
  this->owner = obj.owner;
  this->bSetUid = obj.bSetUid;
  this->group = obj.group;
  this->bSetGid = obj.bSetGid;
  this->size = obj.size;
  this->bSetSize = obj.bSetSize;
  this->rawDevice = obj.rawDevice;
  this->fsid.FSIDMajor = obj.fsid.FSIDMajor;
  this->fsid.FSIDMinor = obj.fsid.FSIDMinor;
  this->fid = obj.fid;
  this->time_access.seconds = obj.time_access.seconds;
  this->time_access.nanosecs = obj.time_access.nanosecs;
  this->bSetAtime = obj.bSetAtime;
  this->aTimeHow = obj.aTimeHow;
  this->time_metadata.seconds = obj.time_metadata.seconds;
  this->time_metadata.nanosecs = obj.time_metadata.nanosecs;
  this->bSetCtime = obj.bSetCtime;
  this->cTimeHow = obj.cTimeHow;
  this->time_modify.seconds = obj.time_modify.seconds;
  this->time_modify.nanosecs = obj.time_modify.nanosecs;
  this->bSetMtime = obj.bSetMtime;
  this->mTimeHow = obj.mTimeHow;
  this->mountFid = obj.mountFid;
  this->changeID = obj.changeID;
  this->name_max = obj.name_max;
  this->files_avail = obj.files_avail;
  this->files_free = obj.files_free;
  this->files_total = obj.files_total;
  this->bytes_avail = obj.bytes_avail;
  this->bytes_free = obj.bytes_free;
  this->bytes_total = obj.bytes_total;
  this->bytes_used = obj.bytes_used;

  memcpy(this->m_buf, obj.m_buf, sizeof(m_buf));
}

NfsAttr& NfsAttr::operator=(const NfsAttr &obj)
{
  this->mask[0] = obj.mask[0];
  this->mask[1] = obj.mask[1];
  this->fileType = obj.fileType;
  this->fmode = obj.fmode;
  this->bSetMode = obj.bSetMode;
  this->nlinks = obj.nlinks;
  this->owner = obj.owner;
  this->bSetUid = obj.bSetUid;
  this->group = obj.group;
  this->bSetGid = obj.bSetGid;
  this->size = obj.size;
  this->bSetSize = obj.bSetSize;
  this->rawDevice = obj.rawDevice;
  this->fsid.FSIDMajor = obj.fsid.FSIDMajor;
  this->fsid.FSIDMinor = obj.fsid.FSIDMinor;
  this->fid = obj.fid;
  this->time_access.seconds = obj.time_access.seconds;
  this->time_access.nanosecs = obj.time_access.nanosecs;
  this->bSetAtime = obj.bSetAtime;
  this->aTimeHow = obj.aTimeHow;
  this->time_metadata.seconds = obj.time_metadata.seconds;
  this->time_metadata.nanosecs = obj.time_metadata.nanosecs;
  this->bSetCtime = obj.bSetCtime;
  this->cTimeHow = obj.cTimeHow;
  this->time_modify.seconds = obj.time_modify.seconds;
  this->time_modify.nanosecs = obj.time_modify.nanosecs;
  this->bSetMtime = obj.bSetMtime;
  this->mTimeHow = obj.mTimeHow;
  this->mountFid = obj.mountFid;
  this->changeID = obj.changeID;
  this->name_max = obj.name_max;
  this->files_avail = obj.files_avail;
  this->files_free = obj.files_free;
  this->files_total = obj.files_total;
  this->bytes_avail = obj.bytes_avail;
  this->bytes_free = obj.bytes_free;
  this->bytes_total = obj.bytes_total;
  this->bytes_used = obj.bytes_used;

  memcpy(this->m_buf, obj.m_buf, sizeof(m_buf));

  return (*this);
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
  owner = std::to_string(attr->fattr3_uid);
  group = std::to_string(attr->fattr3_gid);
  size = attr->fattr3_size;
  bytes_used = attr->fattr3_used;
  // don't need rdev
  fsid.FSIDMinor = attr->fattr3_fsid;
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
    sattr->sattr3_uid.set_uid3_u.uid = std::stoul(owner);
  }
  else
    sattr->sattr3_uid.set_it = 0;

  if (bSetGid)
  {
    sattr->sattr3_gid.set_it = 1;
    sattr->sattr3_gid.set_gid3_u.gid = std::stoul(group);
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

int NfsAttr::NfsAttrToFattr3(fattr3 *attr)
{
  attr->fattr3_type = (ftype3)fileType;
  attr->fattr3_mode = fmode;
  attr->fattr3_nlink = nlinks;
  attr->fattr3_uid = std::stoul(owner);
  attr->fattr3_gid = std::stoul(group);
  attr->fattr3_size = size;
  attr->fattr3_used = bytes_used;
  //attr->fattr3_rdev = rawDevice;
  attr->fattr3_fsid = fsid.FSIDMinor;
  attr->fattr3_fileid = fid;
  attr->fattr3_atime.time3_seconds = time_access.seconds;
  attr->fattr3_atime.time3_nseconds = time_access.nanosecs;
  attr->fattr3_mtime.time3_seconds = time_modify.seconds;
  attr->fattr3_mtime.time3_nseconds = time_modify.nanosecs;
  attr->fattr3_ctime.time3_seconds = time_metadata.seconds;
  attr->fattr3_ctime.time3_nseconds = time_metadata.nanosecs;
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

NfsError::v3ErrMap NfsError::g3Map[] =
{
  {NFS3_OK, "NFS3_OK"},
  {NFS3ERR_PERM, "NFS3ERR_PERM"},
  {NFS3ERR_NOENT, "NFS3ERR_NOENT"},
  {NFS3ERR_IO, "NFS3ERR_IO"},
  {NFS3ERR_NXIO, "NFS3ERR_NXIO"},
  {NFS3ERR_ACCES, "NFS3ERR_ACCES"},
  {NFS3ERR_EXIST, "NFS3ERR_EXIST"},
  {NFS3ERR_XDEV, "NFS3ERR_XDEV"},
  {NFS3ERR_NODEV, "NFS3ERR_NODEV"},
  {NFS3ERR_NOTDIR, "NFS3ERR_NOTDIR"},
  {NFS3ERR_ISDIR, "NFS3ERR_ISDIR"},
  {NFS3ERR_INVAL, "NFS3ERR_INVAL"},
  {NFS3ERR_FBIG, "NFS3ERR_FBIG"},
  {NFS3ERR_NOSPC, "NFS3ERR_NOSPC"},
  {NFS3ERR_ROFS, "NFS3ERR_ROFS"},
  {NFS3ERR_MLINK, "NFS3ERR_MLINK"},
  {NFS3ERR_NAMETOOLONG, "NFS3ERR_NAMETOOLONG"},
  {NFS3ERR_NOTEMPTY, "NFS3ERR_NOTEMPTY"},
  {NFS3ERR_DQUOT, "NFS3ERR_DQUOT"},
  {NFS3ERR_STALE, "NFS3ERR_STALE"},
  {NFS3ERR_REMOTE, "NFS3ERR_REMOTE"},
  {NFS3ERR_BADHANDLE, "NFS3ERR_BADHANDLE"},
  {NFS3ERR_NOT_SYNC, "NFS3ERR_NOT_SYNC"},
  {NFS3ERR_BAD_COOKIE, "NFS3ERR_BAD_COOKIE"},
  {NFS3ERR_NOTSUPP, "NFS3ERR_NOTSUPP"},
  {NFS3ERR_TOOSMALL, "NFS3ERR_TOOSMALL"},
  {NFS3ERR_SERVERFAULT, "NFS3ERR_SERVERFAULT"},
  {NFS3ERR_BADTYPE, "NFS3ERR_BADTYPE"},
  {NFS3ERR_JUKEBOX, "NFS3ERR_JUKEBOX"}
};

NfsError::v4ErrMap NfsError::g4Map[] =
{
  {NFS4_OK, ""},
  {NFS4ERR_PERM, "NFS4ERR_PERM"},
  {NFS4ERR_NOENT, "NFS4ERR_NOENT"},
  {NFS4ERR_IO, "NFS4ERR_IO"},
  {NFS4ERR_NXIO, "NFS4ERR_NXIO"},
  {NFS4ERR_ACCESS, "NFS4ERR_ACCESS"},
  {NFS4ERR_EXIST, "NFS4ERR_EXIST"},
  {NFS4ERR_XDEV, "NFS4ERR_XDEV"},
  {NFS4ERR_NOTDIR, "NFS4ERR_NOTDIR"},
  {NFS4ERR_ISDIR, "NFS4ERR_ISDIR"},
  {NFS4ERR_INVAL, "NFS4ERR_INVAL"},
  {NFS4ERR_FBIG, "NFS4ERR_FBIG"},
  {NFS4ERR_NOSPC, "NFS4ERR_NOSPC"},
  {NFS4ERR_ROFS, "NFS4ERR_ROFS"},
  {NFS4ERR_MLINK, "NFS4ERR_MLINK"},
  {NFS4ERR_NAMETOOLONG, "NFS4ERR_NAMETOOLONG"},
  {NFS4ERR_NOTEMPTY, "NFS4ERR_NOTEMPTY"},
  {NFS4ERR_DQUOT, "NFS4ERR_DQUOT"},
  {NFS4ERR_STALE, "NFS4ERR_STALE"},
  {NFS4ERR_BADHANDLE, "NFS4ERR_BADHANDLE"},
  {NFS4ERR_BAD_COOKIE, "NFS4ERR_BAD_COOKIE"},
  {NFS4ERR_NOTSUPP, "NFS4ERR_NOTSUPP"},
  {NFS4ERR_TOOSMALL, "NFS4ERR_TOOSMALL"},
  {NFS4ERR_SERVERFAULT, "NFS4ERR_SERVERFAULT"},
  {NFS4ERR_BADTYPE, "NFS4ERR_BADTYPE"},
  {NFS4ERR_DELAY, "NFS4ERR_DELAY"},
  {NFS4ERR_SAME, "NFS4ERR_SAME"},
  {NFS4ERR_DENIED, "NFS4ERR_DENIED"},
  {NFS4ERR_EXPIRED, "NFS4ERR_EXPIRED"},
  {NFS4ERR_LOCKED, "NFS4ERR_LOCKED"},
  {NFS4ERR_GRACE, "NFS4ERR_GRACE"},
  {NFS4ERR_FHEXPIRED, "NFS4ERR_FHEXPIRED"},
  {NFS4ERR_SHARE_DENIED, "NFS4ERR_SHARE_DENIED"},
  {NFS4ERR_WRONGSEC, "NFS4ERR_WRONGSEC"},
  {NFS4ERR_CLID_INUSE, "NFS4ERR_CLID_INUSE"},
  {NFS4ERR_RESOURCE, "NFS4ERR_RESOURCE"},
  {NFS4ERR_MOVED, "NFS4ERR_MOVED"},
  {NFS4ERR_NOFILEHANDLE, "NFS4ERR_NOFILEHANDLE"},
  {NFS4ERR_MINOR_VERS_MISMATCH, "NFS4ERR_MINOR_VERS_MISMATCH"},
  {NFS4ERR_STALE_CLIENTID, "NFS4ERR_STALE_CLIENTID"},
  {NFS4ERR_STALE_STATEID, "NFS4ERR_STALE_STATEID"},
  {NFS4ERR_OLD_STATEID, "NFS4ERR_OLD_STATEID"},
  {NFS4ERR_BAD_STATEID, "NFS4ERR_BAD_STATEID"},
  {NFS4ERR_BAD_SEQID, "NFS4ERR_BAD_SEQID"},
  {NFS4ERR_NOT_SAME, "NFS4ERR_NOT_SAME"},
  {NFS4ERR_LOCK_RANGE, "NFS4ERR_LOCK_RANGE"},
  {NFS4ERR_SYMLINK, "NFS4ERR_SYMLINK"},
  {NFS4ERR_RESTOREFH, "NFS4ERR_RESTOREFH"},
  {NFS4ERR_LEASE_MOVED, "NFS4ERR_LEASE_MOVED"},
  {NFS4ERR_ATTRNOTSUPP, "NFS4ERR_ATTRNOTSUPP"},
  {NFS4ERR_NO_GRACE, "NFS4ERR_NO_GRACE"},
  {NFS4ERR_RECLAIM_BAD, "NFS4ERR_RECLAIM_BAD"},
  {NFS4ERR_RECLAIM_CONFLICT, "NFS4ERR_RECLAIM_CONFLICT"},
  {NFS4ERR_BADXDR, "NFS4ERR_BADXDR"},
  {NFS4ERR_LOCKS_HELD, "NFS4ERR_LOCKS_HELD"},
  {NFS4ERR_OPENMODE, "NFS4ERR_OPENMODE"},
  {NFS4ERR_BADOWNER, "NFS4ERR_BADOWNER"},
  {NFS4ERR_BADCHAR, "NFS4ERR_BADCHAR"},
  {NFS4ERR_BADNAME, "NFS4ERR_BADNAME"},
  {NFS4ERR_BAD_RANGE, "NFS4ERR_BAD_RANGE"},
  {NFS4ERR_LOCK_NOTSUPP, "NFS4ERR_LOCK_NOTSUPP"},
  {NFS4ERR_OP_ILLEGAL, "NFS4ERR_OP_ILLEGAL"},
  {NFS4ERR_DEADLOCK, "NFS4ERR_DEADLOCK"},
  {NFS4ERR_FILE_OPEN, "NFS4ERR_FILE_OPEN"},
  {NFS4ERR_ADMIN_REVOKED, "NFS4ERR_ADMIN_REVOKED"},
  {NFS4ERR_CB_PATH_DOWN, "NFS4ERR_CB_PATH_DOWN"}
};

NfsError::mntErrMap NfsError::gmntMap[] =
{
  {MNT3_OK, "MNT3_OK"},
  {MNT3ERR_PERM, "MNT3ERR_PERM"},
  {MNT3ERR_NOENT, "MNT3ERR_NOENT"},
  {MNT3ERR_IO, "MNT3ERR_IO"},
  {MNT3ERR_ACCES, "MNT3ERR_ACCES"},
  {MNT3ERR_NOTDIR, "MNT3ERR_NOTDIR"},
  {MNT3ERR_INVAL, "MNT3ERR_INVAL"},
  {MNT3ERR_NAMETOOLONG, "MNT3ERR_NAMETOOLONG"},
  {MNT3ERR_NOTSUPP, "MNT3ERR_NOTSUPP"},
  {MNT3ERR_SERVERFAULT, "MNT3ERR_SERVERFAULT"}
};

NfsError::nlmErrMap NfsError::gnlmMap[] =
{
  {NLMSTAT4_GRANTED, "NLMSTAT4_GRANTED"},
  {NLMSTAT4_DENIED, "NLMSTAT4_DENIED"},
  {NLMSTAT4_DENIED_NOLOCKS, "NLMSTAT4_DENIED_NOLOCKS"},
  {NLMSTAT4_BLOCKED, "NLMSTAT4_BLOCKED"},
  {NLMSTAT4_DENIED_GRACE_PERIOD, "NLMSTAT4_DENIED_GRACE_PERIOD"},
  {NLMSTAT4_DEADLCK, "NLMSTAT4_DEADLCK"},
  {NLMSTAT4_ROFS, "NLMSTAT4_ROFS"},
  {NLMSTAT4_STALE_FH, "NLMSTAT4_STALE_FH"},
  {NLMSTAT4_FBIG, "NLMSTAT4_FBIG"},
  {NLMSTAT4_FAILED, "NLMSTAT4_FAILED"}
};

NfsError::rpcErrMap NfsError::grpcMap[] =
{
  {RPC_SUCCESS, "RPC_SUCCESS"},
  {RPC_CANTENCODEARGS, "RPC_CANTENCODEARGS"},
  {RPC_CANTDECODERES, "RPC_CANTDECODERES"},
  {RPC_CANTSEND, "RPC_CANTSEND"},
  {RPC_CANTRECV, "RPC_CANTRECV"},
  {RPC_TIMEDOUT, "RPC_TIMEDOUT"},
  {RPC_VERSMISMATCH, "RPC_VERSMISMATCH"},
  {RPC_AUTHERROR, "RPC_AUTHERROR"},
  {RPC_PROGUNAVAIL, "RPC_PROGUNAVAIL"},
  {RPC_PROGVERSMISMATCH, "RPC_PROGVERSMISMATCH"},
  {RPC_PROCUNAVAIL, "RPC_PROCUNAVAIL"},
  {RPC_CANTDECODEARGS, "RPC_CANTDECODEARGS"},
  {RPC_SYSTEMERROR, "RPC_SYSTEMERROR"},
  {RPC_NOBROADCAST, "RPC_NOBROADCAST"},
  {RPC_UNKNOWNHOST, "RPC_UNKNOWNHOST"},
  {RPC_UNKNOWNPROTO, "RPC_UNKNOWNPROTO"},
  {RPC_UNKNOWNADDR, "RPC_UNKNOWNADDR"},
  {RPC_PMAPFAILURE, "RPC_PMAPFAILURE"},
  {RPC_PROGNOTREGISTERED, "RPC_PROGNOTREGISTERED"},
  {RPC_N2AXLATEFAILURE, "RPC_N2AXLATEFAILURE"},
  {RPC_FAILED, "RPC_FAILED"},
  {RPC_INTR, "RPC_INTR"},
  {RPC_TLIERROR, "RPC_TLIERROR"},
  {RPC_UDERROR, "RPC_UDERROR"},
  {RPC_INPROGRESS, "RPC_INPROGRESS"},
  {RPC_STALERACHANDLE, "RPC_STALERACHANDLE"}
};

NfsError::NfsError(const NfsError &obj)
{
  this->etype = obj.etype;
  this->err   = obj.err;
  this->err3  = obj.err3;
  this->err4  = obj.err4;
  this->enlm  = obj.enlm;
  this->emnt  = obj.emnt;
  this->erpc  = obj.erpc;
  this->msg   = obj.msg;
}

NfsError& NfsError::operator=(const NfsError &obj)
{
  etype = obj.etype;
  err   = obj.err;
  err3  = obj.err3;
  err4  = obj.err4;
  enlm  = obj.enlm;
  emnt  = obj.emnt;
  erpc  = obj.erpc;
  msg   = obj.msg;

  return (*this);
}

bool NfsError::operator==(const NfsError &obj)const
{
  if((this->etype  == obj.etype) &&
      (this->err   == obj.err)   &&
      (this->err3  == obj.err3)  &&
      (this->err4  == obj.err4)  &&
      (this->enlm  == obj.enlm)  &&
      (this->emnt  == obj.emnt)  &&
      (this->erpc  == obj.erpc)  &&
      (this->msg   == obj.msg))
    return true;
  else
    return false;
}

bool NfsError::operator==(const bool &val)const
{
  NfsECode ecode = getErrorCode();

  if (val == true)
  {
    if (ecode == NFSERR_INTERNAL_NON || ecode == NFS_MNT3_OK || ecode == NFS_NLMSTAT4_GRANTED || ecode == NFS_OK || ecode == NFS_RPC_SUCCESS)
      return true;
    else
      return false;
  }
  else
  {
    if (!(ecode == NFSERR_INTERNAL_NON || ecode == NFS_MNT3_OK || ecode == NFS_NLMSTAT4_GRANTED || ecode == NFS_OK || ecode == NFS_RPC_SUCCESS))
      return true;
    else
      return false;
  }
}

bool NfsError::operator==(NfsECode ecode)const
{
  return (getErrorCode() == ecode);
}

void NfsError::clear()
{
  etype = EType::ETYPE_INTERNAL;
  err   = NFSERR_INTERNAL_NON;
  err3  = nfsstat3::NFS3_OK;
  err4  = nfsstat4::NFS4_OK;
  enlm  = (nlm4_stats)0;
  emnt  = (mountstat3)0;
  erpc  = (clnt_stat)0;
  msg.clear();
}

void NfsError::setError4(nfsstat4 code, const std::string err)
{
  etype = ETYPE_V4;
  err4  = code;
  if (!err.empty())
  {
    msg = err;
  }
  else
  {
    msg = getV4ErrorMsg(code);
  }
}

void NfsError::setError3(nfsstat3 code, const std::string err)
{
  etype = ETYPE_V3;
  err3  = code;
  if (!err.empty())
  {
    msg = err;
  }
  else
  {
    msg = getV3ErrorMsg(code);
  }
}

void NfsError::setNlmError(nlm4_stats code, const std::string err)
{
  etype = ETYPE_NLM;
  enlm  = code;
  if (!err.empty())
  {
    msg = err;
  }
  else
  {
    msg = getNlmErrorMsg(code);
  }
}

void NfsError::setMntError(mountstat3 code, const std::string err)
{
  etype = ETYPE_MNT;
  emnt  = code;
  msg = err + ": ERROR - " + getMntErrorMsg(code);
}

void NfsError::setRpcError(clnt_stat code, const std::string err)
{
  etype = ETYPE_RPC;
  erpc = code;
  msg = err;
}

void NfsError::setError(NfsECode code, const std::string emsg)
{
  etype = ETYPE_INTERNAL;
  err   = code;
  msg   = emsg;
}

NfsECode NfsError::getErrorCode() const
{
  NfsECode ecode = NFS_OK;

  if (etype == ETYPE_INTERNAL)
  {
    return err;
  }
  else if (etype == ETYPE_V3)
  {
    switch (err3)
    {
      case NFS3_OK: ecode = NFS_OK; break;
      case NFS3ERR_PERM: ecode = NFSERR_PERM; break;
      case NFS3ERR_NOENT: ecode = NFSERR_NOENT; break;
      case NFS3ERR_IO: ecode = NFSERR_IO; break;
      case NFS3ERR_NXIO: ecode = NFSERR_NXIO; break;
      case NFS3ERR_ACCES: ecode = NFSERR_ACCESS; break;
      case NFS3ERR_EXIST: ecode = NFSERR_EXIST; break;
      case NFS3ERR_XDEV: ecode = NFSERR_XDEV; break;
      case NFS3ERR_NODEV: ecode = NFSERR_NODEV; break;
      case NFS3ERR_NOTDIR: ecode = NFSERR_NOTDIR; break;
      case NFS3ERR_ISDIR: ecode = NFSERR_ISDIR; break;
      case NFS3ERR_INVAL: ecode = NFSERR_INVAL; break;
      case NFS3ERR_FBIG: ecode = NFSERR_FBIG; break;
      case NFS3ERR_NOSPC: ecode = NFSERR_NOSPC; break;
      case NFS3ERR_ROFS: ecode = NFSERR_ROFS; break;
      case NFS3ERR_MLINK: ecode = NFSERR_MLINK; break;
      case NFS3ERR_NAMETOOLONG: ecode = NFSERR_NAMETOOLONG; break;
      case NFS3ERR_NOTEMPTY: ecode = NFSERR_NOTEMPTY; break;
      case NFS3ERR_DQUOT: ecode = NFSERR_DQUOT; break;
      case NFS3ERR_STALE: ecode = NFSERR_STALE; break;
      case NFS3ERR_REMOTE: ecode = NFSERR_REMOTE; break;
      case NFS3ERR_BADHANDLE: ecode = NFSERR_BADHANDLE; break;
      case NFS3ERR_NOT_SYNC: ecode = NFSERR_NOT_SYNC; break;
      case NFS3ERR_BAD_COOKIE: ecode = NFSERR_BAD_COOKIE; break;
      case NFS3ERR_NOTSUPP: ecode = NFSERR_NOTSUPP; break;
      case NFS3ERR_TOOSMALL: ecode = NFSERR_TOOSMALL; break;
      case NFS3ERR_SERVERFAULT: ecode = NFSERR_SERVERFAULT; break;
      case NFS3ERR_BADTYPE: ecode = NFSERR_BADTYPE; break;
      case NFS3ERR_JUKEBOX: ecode = NFSERR_DELAY; break;
    }
    return ecode;
  }
  else if (etype == ETYPE_V4)
  {
    switch (err4)
    {
      case NFS4_OK: ecode = NFS_OK; break;
      case NFS4ERR_PERM: ecode = NFSERR_PERM; break;
      case NFS4ERR_NOENT: ecode = NFSERR_NOENT; break;
      case NFS4ERR_IO: ecode = NFSERR_IO; break;
      case NFS4ERR_NXIO: ecode = NFSERR_NXIO; break;
      case NFS4ERR_ACCESS: ecode = NFSERR_ACCESS; break;
      case NFS4ERR_EXIST: ecode = NFSERR_EXIST; break;
      case NFS4ERR_XDEV: ecode = NFSERR_XDEV; break;
      case NFS4ERR_NOTDIR: ecode = NFSERR_NOTDIR; break;
      case NFS4ERR_ISDIR: ecode = NFSERR_ISDIR; break;
      case NFS4ERR_INVAL: ecode = NFSERR_INVAL; break;
      case NFS4ERR_FBIG: ecode = NFSERR_FBIG; break;
      case NFS4ERR_NOSPC: ecode = NFSERR_NOSPC; break;
      case NFS4ERR_ROFS: ecode = NFSERR_ROFS; break;
      case NFS4ERR_MLINK: ecode = NFSERR_MLINK; break;
      case NFS4ERR_NAMETOOLONG: ecode = NFSERR_NAMETOOLONG; break;
      case NFS4ERR_NOTEMPTY: ecode = NFSERR_NOTEMPTY; break;
      case NFS4ERR_DQUOT: ecode = NFSERR_DQUOT; break;
      case NFS4ERR_STALE: ecode = NFSERR_STALE; break;
      case NFS4ERR_BADHANDLE: ecode = NFSERR_BADHANDLE; break;
      case NFS4ERR_BAD_COOKIE: ecode = NFSERR_BAD_COOKIE; break;
      case NFS4ERR_NOTSUPP: ecode = NFSERR_NOTSUPP; break;
      case NFS4ERR_TOOSMALL: ecode = NFSERR_TOOSMALL; break;
      case NFS4ERR_SERVERFAULT: ecode = NFSERR_SERVERFAULT; break;
      case NFS4ERR_BADTYPE: ecode = NFSERR_BADTYPE; break;
      case NFS4ERR_DELAY: ecode = NFSERR_DELAY; break;
      case NFS4ERR_SAME: ecode = NFSERR_SAME; break;
      case NFS4ERR_DENIED: ecode = NFSERR_DENIED; break;
      case NFS4ERR_EXPIRED: ecode = NFSERR_EXPIRED; break;
      case NFS4ERR_LOCKED: ecode = NFSERR_LOCKED; break;
      case NFS4ERR_GRACE: ecode = NFSERR_GRACE; break;
      case NFS4ERR_FHEXPIRED: ecode = NFSERR_FHEXPIRED; break;
      case NFS4ERR_SHARE_DENIED: ecode = NFSERR_SHARE_DENIED; break;
      case NFS4ERR_WRONGSEC: ecode = NFSERR_WRONGSEC; break;
      case NFS4ERR_CLID_INUSE: ecode = NFSERR_CLID_INUSE; break;
      case NFS4ERR_RESOURCE: ecode = NFSERR_RESOURCE; break;
      case NFS4ERR_MOVED: ecode = NFSERR_MOVED; break;
      case NFS4ERR_NOFILEHANDLE: ecode = NFSERR_NOFILEHANDLE; break;
      case NFS4ERR_MINOR_VERS_MISMATCH: ecode = NFSERR_MINOR_VERS_MISMATCH; break;
      case NFS4ERR_STALE_CLIENTID: ecode = NFSERR_STALE_CLIENTID; break;
      case NFS4ERR_STALE_STATEID: ecode = NFSERR_STALE_STATEID; break;
      case NFS4ERR_OLD_STATEID: ecode = NFSERR_OLD_STATEID; break;
      case NFS4ERR_BAD_STATEID: ecode = NFSERR_BAD_STATEID; break;
      case NFS4ERR_BAD_SEQID: ecode = NFSERR_BAD_SEQID; break;
      case NFS4ERR_NOT_SAME: ecode = NFSERR_NOT_SAME; break;
      case NFS4ERR_LOCK_RANGE: ecode = NFSERR_LOCK_RANGE; break;
      case NFS4ERR_SYMLINK: ecode = NFSERR_SYMLINK; break;
      case NFS4ERR_RESTOREFH: ecode = NFSERR_RESTOREFH; break;
      case NFS4ERR_LEASE_MOVED: ecode = NFSERR_LEASE_MOVED; break;
      case NFS4ERR_ATTRNOTSUPP: ecode = NFSERR_ATTRNOTSUPP; break;
      case NFS4ERR_NO_GRACE: ecode = NFSERR_NO_GRACE; break;
      case NFS4ERR_RECLAIM_BAD: ecode = NFSERR_RECLAIM_BAD; break;
      case NFS4ERR_RECLAIM_CONFLICT: ecode = NFSERR_RECLAIM_CONFLICT; break;
      case NFS4ERR_BADXDR: ecode = NFSERR_BADXDR; break;
      case NFS4ERR_LOCKS_HELD: ecode = NFSERR_LOCKS_HELD; break;
      case NFS4ERR_OPENMODE: ecode = NFSERR_OPENMODE; break;
      case NFS4ERR_BADOWNER: ecode = NFSERR_BADOWNER; break;
      case NFS4ERR_BADCHAR: ecode = NFSERR_BADCHAR; break;
      case NFS4ERR_BADNAME: ecode = NFSERR_BADNAME; break;
      case NFS4ERR_BAD_RANGE: ecode = NFSERR_BAD_RANGE; break;
      case NFS4ERR_LOCK_NOTSUPP: ecode = NFSERR_LOCK_NOTSUPP; break;
      case NFS4ERR_OP_ILLEGAL: ecode = NFSERR_OP_ILLEGAL; break;
      case NFS4ERR_DEADLOCK: ecode = NFSERR_DEADLOCK; break;
      case NFS4ERR_FILE_OPEN: ecode = NFSERR_FILE_OPEN; break;
      case NFS4ERR_ADMIN_REVOKED: ecode = NFSERR_ADMIN_REVOKED; break;
      case NFS4ERR_CB_PATH_DOWN: ecode = NFSERR_CB_PATH_DOWN; break;
    }
    return ecode;
  }
  else if (etype == ETYPE_NLM)
  {
    switch (enlm)
    {
      case NLMSTAT4_GRANTED: ecode = NFS_NLMSTAT4_GRANTED; break;
      case NLMSTAT4_DENIED: ecode = NFS_NLMSTAT4_DENIED; break;
      case NLMSTAT4_DENIED_NOLOCKS: ecode = NFS_NLMSTAT4_DENIED_NOLOCKS; break;
      case NLMSTAT4_BLOCKED: ecode = NFS_NLMSTAT4_BLOCKED; break;
      case NLMSTAT4_DENIED_GRACE_PERIOD: ecode = NFS_NLMSTAT4_DENIED_GRACE_PERIOD; break;
      case NLMSTAT4_DEADLCK: ecode = NFS_NLMSTAT4_DEADLCK; break;
      case NLMSTAT4_ROFS: ecode = NFS_NLMSTAT4_ROFS; break;
      case NLMSTAT4_STALE_FH: ecode = NFS_NLMSTAT4_STALE_FH; break;
      case NLMSTAT4_FBIG: ecode = NFS_NLMSTAT4_FBIG; break;
      case NLMSTAT4_FAILED: ecode = NFS_NLMSTAT4_FAILED; break;
    }
    return ecode;
  }
  else if (etype == ETYPE_MNT)
  {
    switch (emnt)
    {
      case MNT3_OK: ecode = NFS_MNT3_OK; break;
      case MNT3ERR_PERM: ecode = NFS_MNT3ERR_PERM; break;
      case MNT3ERR_NOENT: ecode = NFS_MNT3ERR_NOENT; break;
      case MNT3ERR_IO: ecode = NFS_MNT3ERR_IO; break;
      case MNT3ERR_ACCES: ecode = NFS_MNT3ERR_ACCES; break;
      case MNT3ERR_NOTDIR: ecode = NFS_MNT3ERR_NOTDIR; break;
      case MNT3ERR_INVAL: ecode = NFS_MNT3ERR_INVAL; break;
      case MNT3ERR_NAMETOOLONG: ecode = NFS_MNT3ERR_NAMETOOLONG; break;
      case MNT3ERR_NOTSUPP: ecode = NFS_MNT3ERR_NOTSUPP; break;
      case MNT3ERR_SERVERFAULT: ecode = NFS_MNT3ERR_SERVERFAULT; break;
    }
    return ecode;
  }
  else if (etype == ETYPE_RPC)
  {
    switch (erpc)
    {
      case RPC_SUCCESS: ecode = NFS_RPC_SUCCESS; break;
      case RPC_CANTENCODEARGS: ecode = NFS_RPC_CANTENCODEARGS; break;
      case RPC_CANTDECODERES: ecode = NFS_RPC_CANTDECODERES; break;
      case RPC_CANTSEND: ecode = NFS_RPC_CANTSEND; break;
      case RPC_CANTRECV: ecode = NFS_RPC_CANTRECV; break;
      case RPC_TIMEDOUT: ecode = NFS_RPC_TIMEDOUT; break;
      case RPC_VERSMISMATCH: ecode = NFS_RPC_VERSMISMATCH; break;
      case RPC_AUTHERROR: ecode = NFS_RPC_AUTHERROR; break;
      case RPC_PROGUNAVAIL: ecode = NFS_RPC_PROGUNAVAIL; break;
      case RPC_PROGVERSMISMATCH: ecode = NFS_RPC_PROGVERSMISMATCH; break;
      case RPC_PROCUNAVAIL: ecode = NFS_RPC_PROCUNAVAIL; break;
      case RPC_CANTDECODEARGS: ecode = NFS_RPC_CANTDECODEARGS; break;
      case RPC_SYSTEMERROR: ecode = NFS_RPC_SYSTEMERROR; break;
      case RPC_NOBROADCAST: ecode = NFS_RPC_NOBROADCAST; break;
      case RPC_UNKNOWNHOST: ecode = NFS_RPC_UNKNOWNHOST; break;
      case RPC_UNKNOWNPROTO: ecode = NFS_RPC_UNKNOWNPROTO; break;
      case RPC_UNKNOWNADDR: ecode = NFS_RPC_UNKNOWNADDR; break;
      case RPC_PMAPFAILURE: ecode = NFS_RPC_PMAPFAILURE; break;
      case RPC_PROGNOTREGISTERED: ecode = NFS_RPC_PROGNOTREGISTERED; break;
      case RPC_N2AXLATEFAILURE: ecode = NFS_RPC_N2AXLATEFAILURE; break;
      case RPC_FAILED: ecode = NFS_RPC_FAILED; break;
      case RPC_INTR: ecode = NFS_RPC_INTR; break;
      case RPC_TLIERROR: ecode = NFS_RPC_TLIERROR; break;
      case RPC_UDERROR: ecode = NFS_RPC_UDERROR; break;
      case RPC_INPROGRESS: ecode = NFS_RPC_INPROGRESS; break;
      case RPC_STALERACHANDLE: ecode = NFS_RPC_STALERACHANDLE; break;
    }
    return ecode;
  }
  else
    return NFS_OK;
}

std::string NfsError::getErrorString()
{
  if (etype == ETYPE_V3)
  {
    int count = sizeof(g3Map)/sizeof(v3ErrMap);
    for (int i = 0; i < count; i++)
    {
      if (g3Map[i].err == err3)
        return g3Map[i].name;
    }
    return "NULL V3 ERROR";
  }
  else if (etype == ETYPE_V4)
  {
    int count = sizeof(g4Map)/sizeof(v4ErrMap);
    for (int i = 0; i < count; i++)
    {
      if (g4Map[i].err == err4)
        return g4Map[i].name;
    }
    return "NULL V4 ERROR";
  }
  else if (etype == ETYPE_NLM)
  {
    int count = sizeof(gnlmMap)/sizeof(nlmErrMap);
    for (int i = 0; i < count; i++)
    {
      if (gnlmMap[i].err == enlm)
        return gnlmMap[i].name;
    }
    return "NULL NLM ERROR";
  }
  else if (etype == ETYPE_MNT)
  {
    int count = sizeof(gmntMap)/sizeof(mntErrMap);
    for (int i = 0; i < count; i++)
    {
      if (gmntMap[i].err == emnt)
        return gmntMap[i].name;
    }
    return "NULL MNT ERROR";
  }
  else if (etype == ETYPE_RPC)
  {
    int count = sizeof(grpcMap)/sizeof(rpcErrMap);
    for (int i = 0; i < count; i++)
    {
      if (grpcMap[i].err == erpc)
        return grpcMap[i].name;
    }
    return "NULL RPC ERROR";
  }
  return "INVALID NFS ERROR";
}

std::string NfsError::getMntErrorMsg(mountstat3 status)
{
  switch ( status )
  {
    case MNT3ERR_PERM:
      return "Not owner";
    case MNT3ERR_NOENT:
      return "No such file or directory";
    case MNT3ERR_IO:
      return  "I/O error";
    case MNT3ERR_ACCES:
      return  "Permission denied";
    case MNT3ERR_NOTDIR:
      return "Not a directory";
    case MNT3ERR_INVAL:
      return "Invalid argument";
    case MNT3ERR_NAMETOOLONG:
      return "Filename too long";
    case MNT3ERR_NOTSUPP:
      return "Operation not supported";
    case MNT3ERR_SERVERFAULT:
      return "A failure on the server";
    default:
      return "unknown error";
  }
}

std::string NfsError::getV3ErrorMsg(nfsstat3 status)
{
  return "";
}

std::string NfsError::getV4ErrorMsg(nfsstat4 status)
{
  return "";
}

std::string NfsError::getNlmErrorMsg(nlm4_stats status)
{
  return "";
}
