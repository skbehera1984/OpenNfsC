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

#ifndef _DATA_TYPES_
#define _DATA_TYPES_

#include <string>
#include <vector>
#include <mutex>
#include <string.h>
#include<nfsrpc/nfs4.h>
#include<nfsrpc/nfs.h>

namespace OpenNfsC {

// LOG levels --  copied from syslog.h
#define LOG_EMERG   0   /* system is unusable */
#define LOG_ALERT   1   /* action must be taken immediately */
#define LOG_CRIT    2   /* critical conditions */
#define LOG_ERR     3   /* error conditions */
#define LOG_WARNING 4   /* warning conditions */
#define LOG_NOTICE  5   /* normal but significant condition */
#define LOG_INFO    6   /* informational */
#define LOG_DEBUG   7   /* debug-level messages */

// ACCESS
#define	ACCESS4_READ	0x00000001
#define	ACCESS4_LOOKUP	0x00000002
#define	ACCESS4_MODIFY	0x00000004
#define	ACCESS4_EXTEND	0x00000008
#define	ACCESS4_DELETE	0x00000010
#define	ACCESS4_EXECUTE	0x00000020

// SHARE ACCESS
#define	SHARE_ACCESS_READ	0x00000001
#define	SHARE_ACCESS_WRITE	0x00000002
#define	SHARE_ACCESS_BOTH	0x00000003
#define	SHARE_DENY_NONE		0x00000000
#define	SHARE_DENY_READ		0x00000001
#define	SHARE_DENY_WRITE	0x00000002
#define	SHARE_DENY_BOTH		0x00000003

enum NfsFileType
{
  FILE_TYPE_NON = 0,
  FILE_TYPE_REG = 1,
  FILE_TYPE_DIR = 2,
  FILE_TYPE_BLK = 3,
  FILE_TYPE_CHR = 4,
  FILE_TYPE_LNK = 5,
  FILE_TYPE_SOCK = 6,
  FILE_TYPE_FIFO = 7,
  FILE_TYPE_ATTRDIR = 8,
  FILE_TYPE_NAMEDATTR = 9,
};

struct NfsFsId
{
  uint64_t FSIDMajor;
  uint64_t FSIDMinor;
};

struct NfsTime
{
  uint64_t getSeconds() { return seconds; }
  uint32_t getNanoSecs() { return nanosecs; }
  uint64_t seconds;
  uint32_t nanosecs;
};

#define NFS_BLKSIZE 4096
struct NfsFsStat
{
  uint64_t files_avail;
  uint64_t files_free;
  uint64_t files_total;
  uint64_t bytes_avail;
  uint64_t bytes_free;
  uint64_t bytes_total;
};

enum NfsTimeHow
{
  NFS_TIME_DONT_CHANGE = 0,
  NFS_TIME_SET_TO_SERVER_TIME = 1,
  NFS_TIME_SET_TO_CLIENT_TIME = 2,
};

struct NfsAttr
{
public:
  NfsAttr();
  ~NfsAttr();
  void clear();
  bool empty();
  void print() const;

  NfsAttr(const NfsAttr &obj);
  NfsAttr& operator=(const NfsAttr &obj);

  // conversion functions
  int Fattr3ToNfsAttr(fattr3 *attr);
  int NfsAttrToFattr3(fattr3 *attr);
  int NfsAttrToSattr3(sattr3 *sattr);

  // getters
  NfsFileType getFileType() { return fileType; }
  uint32_t    getFileMode() { return fmode; }
  uint32_t    getNumLinks() { return nlinks; }
  uint32_t    getUid() { return uid; }
  uint32_t    getGid() { return gid; }
  std::string getOwner() { return owner; }
  std::string getGroup() { return group; }
  uint64_t    getSize() { return size; }
  uint64_t    getSizeUsed() { return bytes_used; }
  uint64_t    getFsId() { return fsid.FSIDMajor; }
  uint64_t    getFid() { return fid; }
  NfsTime     getAccessTime() { return time_access; }
  uint64_t    getAccessTimeSecs() { return time_access.seconds; }
  NfsTime     getChangeTime() { return time_metadata; }
  uint64_t    getChangeTimeSecs() { return time_metadata.seconds; }
  NfsTime     getModifyTime() { return time_modify; }
  uint64_t    getModifyTimeSecs() { return time_modify.seconds; }

  // setters
  void setFileMode(uint32_t mode)
  {
    fmode = mode;
    bSetMode = true;
    mask[1] |= (1 << (FATTR4_MODE - 32));
  }
  void unsetFileMode()
  {
    bSetMode = false;
    mask[1] &= ~(1 << (FATTR4_MODE - 32));
  }
  void setUid(uint32_t UID)
  {
    uid = UID;
    bSetUid = true;
    mask[1] |= (1 << (FATTR4_OWNER - 32));
  }
  void unsetUid()
  {
    bSetUid = false;
    mask[1] &= ~(1 << (FATTR4_OWNER - 32));
  }
  void setGid(uint32_t GID)
  {
    gid = GID;
    bSetGid = true;
    mask[1] |= (1 << (FATTR4_OWNER_GROUP - 32));
  }
  void unsetGid()
  {
    bSetGid = false;
    mask[1] &= ~(1 << (FATTR4_OWNER_GROUP - 32));
  }
  void setOwner(std::string& Owner)
  {
    owner = Owner;
    bSetUid = true;
    mask[1] |= (1 << (FATTR4_OWNER - 32));
  }
  void unsetOwner()
  {
    bSetUid = false;
    mask[1] &= ~(1 << (FATTR4_OWNER - 32));
  }
  void setGroup(std::string& Group)
  {
    group= Group;
    bSetGid = true;
    mask[1] |= (1 << (FATTR4_OWNER_GROUP - 32));
  }
  void unsetGroup()
  {
    bSetGid = false;
    mask[1] &= ~(1 << (FATTR4_OWNER_GROUP - 32));
  }
  void setSize(uint64_t ssize)
  {
    size = ssize;
    bSetSize = true;
    mask[0] |= (1 << FATTR4_SIZE);
  }
  void unsetSize()
  {
    bSetSize = false;
    mask[0] &= ~(1 << FATTR4_SIZE);
  }
  void setAccessTime(NfsTime aTime, NfsTimeHow how)
  {
    time_access = aTime;
    bSetAtime   = true;
    aTimeHow    = how;
    mask[1] |= ( 1 << (FATTR4_TIME_ACCESS_SET - 32));
  }
  void setAccessTime(uint64_t seconds, uint32_t nanosecs = 0, NfsTimeHow how = NFS_TIME_DONT_CHANGE)
  {
    time_access.seconds = seconds;
    time_access.nanosecs = nanosecs;
    bSetAtime   = true;
    aTimeHow    = how;
    mask[1] |= (1 << (FATTR4_TIME_ACCESS_SET - 32));
  }
  void unsetAccessTime()
  {
    bSetAtime   = false;
    mask[1] &= ~((1 << (FATTR4_TIME_ACCESS - 32) | 1 << (FATTR4_TIME_ACCESS_SET - 32)));
  }
  void setModifyTime(NfsTime mTime, NfsTimeHow how)
  {
    time_modify = mTime;
    bSetMtime   = true;
    mTimeHow    = how;
    mask[1] |= (1 << (FATTR4_TIME_MODIFY_SET - 32));
  }
  void setModifyTime(uint64_t seconds, uint32_t nanosecs = 0, NfsTimeHow how = NFS_TIME_DONT_CHANGE)
  {
    time_modify.seconds = seconds;
    time_modify.nanosecs = nanosecs;
    bSetMtime   = true;
    mTimeHow    = how;
    mask[1] |= (1 << (FATTR4_TIME_MODIFY_SET - 32));
  }
  void unsetModifyTime()
  {
    bSetMtime   = false;
    mask[1] &= ~((1 << (FATTR4_TIME_MODIFY - 32) | 1 << (FATTR4_TIME_MODIFY_SET - 32)));
  }
  void setChangeTime(uint64_t seconds, uint32_t nanosecs = 0, NfsTimeHow how = NFS_TIME_DONT_CHANGE)
  {
    time_metadata.seconds = seconds;
    time_metadata.nanosecs = nanosecs;
    bSetCtime   = true;
    cTimeHow    = how;
    mask[1] |= 1 << (FATTR4_TIME_METADATA - 32);
  }
  void unsetChangeTime()
  {
    bSetCtime   = false;
    mask[1] &= ~(1 << (FATTR4_TIME_METADATA - 32));
  }

  // FSSTAT getters
  uint64_t getFilesAvailable() { return files_avail; }
  uint64_t getFilesFree() { return files_free; }
  uint64_t getFilesTotal() { return files_total; }
  uint64_t getBytesAvailable() { return bytes_avail; }
  uint64_t getBytesFree() { return bytes_free; }
  uint64_t getBytesTotal() { return bytes_total; }

public:
  uint32_t    mask[2];
  NfsFileType fileType;
  uint32_t    fmode;
  bool        bSetMode;
  uint32_t    nlinks;
  uint32_t    uid;
  bool        bSetUid;
  uint32_t    gid;
  bool        bSetGid;
  uint64_t    size;
  bool        bSetSize;
  uint64_t    rawDevice;
  NfsFsId     fsid;
  uint64_t    fid; // inode
  NfsTime     time_access;
  bool        bSetAtime;
  NfsTimeHow  aTimeHow;
  NfsTime     time_metadata;
  bool        bSetCtime;
  NfsTimeHow  cTimeHow;
  NfsTime     time_modify;
  bool        bSetMtime;
  NfsTimeHow  mTimeHow;

  std::string owner;
  std::string group;
  uint64_t    mountFid;
  uint64_t    changeID;
  uint32_t    name_max;
  uint64_t    files_avail;
  uint64_t    files_free;
  uint64_t    files_total;
  uint64_t    bytes_avail;
  uint64_t    bytes_free;
  uint64_t    bytes_total;
  uint64_t    bytes_used;
};

struct NfsAccess
{
  uint32_t supported;
  uint32_t access;
};

struct NfsFile
{
  uint64_t    cookie;
  std::string name;
  std::string path;
  NfsFileType type;
  NfsAttr     attr;
  bool isDirectory();
  bool isSymlink();
  bool isZeroByteFile();
};
typedef std::vector<NfsFile> NfsFiles;

struct NfsStateId
{
  uint32_t seqid;
  char other[12];
};

class NfsFh
{
  public:
    NfsFh();
    NfsFh(uint32_t len, const char *val);
    NfsFh(const NfsFh &fromFH);
    ~NfsFh() { clear(); }
    const NfsFh &operator=(const NfsFh &fromFH);
    void clear();

    void setOpenState(NfsStateId& opSt);
    void setLockState(NfsStateId& lkSt);
    NfsStateId& getOpenState() { return OSID; }
    NfsStateId& getLockState() { return LSID; }

    char *getData() const { return fhVal; }
    uint32_t getLength() const { return fhLen; }

  private:
    uint32_t    fhLen;
    char        *fhVal;
    NfsStateId   OSID; // Open State ID
    NfsStateId   LSID; // Lock State ID
};

enum NfsLockType
{
  READ_LOCK = 1,
  WRITE_LOCK = 2,
  READ_LOCK_BLOCKING = 3,  // the client will wait till sever grants it
  WRIT_WLOCK_BLOCKING = 4, // the client will wait till sever grants it
};

class NfsError
{
  public:
    enum EType
    {
      ETYPE_INTERNAL = 0,
      ETYPE_V3 = 3,
      ETYPE_V4 = 4,
    };

    void clear();
    NfsError() { clear(); }
    NfsError(EType type) { clear(); etype = type; }

    void setError4(nfsstat4 code, const std::string &msg);
    void setError3(nfsstat3 code, const std::string &msg);
    void setError(uint32_t code, const std::string &err); // set internal error

    uint32_t getErrorCode();
    nfsstat3 getV3ErrorCode() { return err3; }
    nfsstat4 getV4ErrorCode() { return err4; }
    std::string& getErrorMsg() { return msg; }

    NfsError(const NfsError &obj);
    NfsError& operator=(const NfsError &obj);
    bool operator==(const NfsError &obj)const;
    bool operator==(const bool &val)const;

  private:
    enum EType  etype;
    uint32_t    err;  // internal error code
    nfsstat3    err3; // nfs v3 error code
    nfsstat4    err4; // nfs v4 error code
    std::string msg;
};

} // end of namespace
#endif /* _DATA_TYPES_ */
