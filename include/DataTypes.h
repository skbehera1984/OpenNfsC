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
#include <rpc/clnt.h>
#include <nfsrpc/nfs4.h>
#include <nfsrpc/nfs.h>
#include <nfsrpc/nlm.h>
#include <nfsrpc/mount.h>

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
  std::string getOwner() { return owner; }
  std::string getGroup() { return group; }
  uint64_t    getSize() { return size; }
  uint64_t    getSizeUsed() { return bytes_used; }
  uint64_t    getFsId() { return fsid.FSIDMinor; }
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
  void setOwner(std::string& Owner)
  {
    if (Owner.empty())
      return;
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
    if (Group.empty())
      return;
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
  std::string owner;
  bool        bSetUid;
  std::string group;
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

  char        m_buf[4096]; // used for encoding v4 attr
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
    bool operator==(const NfsFh &fromFH);
    void clear();
    void clearStates();

    void setOpenState(NfsStateId& opSt);
    void setLockState(NfsStateId& lkSt);
    NfsStateId& getOpenState() { return OSID; }
    NfsStateId& getLockState() { return LSID; }

    bool isLocked() { return locked; }
    void setLocked() { locked = true; }
    void setUnlocked() { locked = false; }
    uint32_t getFileLockSeqId();

    bool isOpen() { return m_opened; }
    void close() { m_opened = false; }

    char *getData() const { return fhVal; }
    uint32_t getLength() const { return fhLen; }

    void setPath(const std::string& path) { m_path = path; }
    std::string& getPath() { return m_path; }

  private:
    uint32_t    fhLen;
    char        *fhVal;
    NfsStateId   OSID; // Open State ID
    NfsStateId   LSID; // Lock State ID
    bool         locked;

    std::mutex   m_lock_seqid_mutex;
    uint32_t     m_file_lock_seqid;

    bool         m_opened;
    std::string  m_path; // path for which this handle is obtained
};

enum NfsLockType
{
  READ_LOCK = 1,
  WRITE_LOCK = 2,
  READ_LOCK_BLOCKING = 3,  // the client will wait till sever grants it
  WRIT_WLOCK_BLOCKING = 4, // the client will wait till sever grants it
};

enum NfsECode
{
  NFS_OK = 0,
  NFSERR_PERM = 1,
  NFSERR_NOENT = 2,
  NFSERR_IO = 5,
  NFSERR_NXIO = 6,
  NFSERR_ACCESS = 13,
  NFSERR_EXIST = 17,
  NFSERR_XDEV = 18,
  NFSERR_NODEV = 19, // only v3
  NFSERR_NOTDIR = 20,
  NFSERR_ISDIR = 21,
  NFSERR_INVAL = 22,
  NFSERR_FBIG = 27,
  NFSERR_NOSPC = 28,
  NFSERR_ROFS = 30,
  NFSERR_MLINK = 31,
  NFSERR_NAMETOOLONG = 63,
  NFSERR_NOTEMPTY = 66,
  NFSERR_DQUOT = 69,
  NFSERR_STALE = 70,
  NFSERR_REMOTE = 71, // only v3
  NFSERR_BADHANDLE = 10001,
  NFSERR_NOT_SYNC = 10002, // only v3
  NFSERR_BAD_COOKIE = 10003,
  NFSERR_NOTSUPP = 10004,
  NFSERR_TOOSMALL = 10005,
  NFSERR_SERVERFAULT = 10006,
  NFSERR_BADTYPE = 10007,
  NFSERR_DELAY = 10008, // NFS3ERR_JUKEBOX in v3
  NFSERR_SAME = 10009,
  NFSERR_DENIED = 10010,
  NFSERR_EXPIRED = 10011,
  NFSERR_LOCKED = 10012,
  NFSERR_GRACE = 10013,
  NFSERR_FHEXPIRED = 10014,
  NFSERR_SHARE_DENIED = 10015,
  NFSERR_WRONGSEC = 10016,
  NFSERR_CLID_INUSE = 10017,
  NFSERR_RESOURCE = 10018,
  NFSERR_MOVED = 10019,
  NFSERR_NOFILEHANDLE = 10020,
  NFSERR_MINOR_VERS_MISMATCH = 10021,
  NFSERR_STALE_CLIENTID = 10022,
  NFSERR_STALE_STATEID = 10023,
  NFSERR_OLD_STATEID = 10024,
  NFSERR_BAD_STATEID = 10025,
  NFSERR_BAD_SEQID = 10026,
  NFSERR_NOT_SAME = 10027,
  NFSERR_LOCK_RANGE = 10028,
  NFSERR_SYMLINK = 10029,
  NFSERR_RESTOREFH = 10030,
  NFSERR_LEASE_MOVED = 10031,
  NFSERR_ATTRNOTSUPP = 10032,
  NFSERR_NO_GRACE = 10033,
  NFSERR_RECLAIM_BAD = 10034,
  NFSERR_RECLAIM_CONFLICT = 10035,
  NFSERR_BADXDR = 10036,
  NFSERR_LOCKS_HELD = 10037,
  NFSERR_OPENMODE = 10038,
  NFSERR_BADOWNER = 10039,
  NFSERR_BADCHAR = 10040,
  NFSERR_BADNAME = 10041,
  NFSERR_BAD_RANGE = 10042,
  NFSERR_LOCK_NOTSUPP = 10043,
  NFSERR_OP_ILLEGAL = 10044,
  NFSERR_DEADLOCK = 10045,
  NFSERR_FILE_OPEN = 10046,
  NFSERR_ADMIN_REVOKED = 10047,
  NFSERR_CB_PATH_DOWN = 10048,

  NFS_MNT3_OK = 20000,
  NFS_MNT3ERR_PERM = 20001,
  NFS_MNT3ERR_NOENT = 20002,
  NFS_MNT3ERR_IO = 20003,
  NFS_MNT3ERR_ACCES = 20004,
  NFS_MNT3ERR_NOTDIR = 20005,
  NFS_MNT3ERR_INVAL = 20006,
  NFS_MNT3ERR_NAMETOOLONG = 20007,
  NFS_MNT3ERR_NOTSUPP = 20008,
  NFS_MNT3ERR_SERVERFAULT = 20009,

  NFS_NLMSTAT4_GRANTED = 30000,
  NFS_NLMSTAT4_DENIED = 30001,
  NFS_NLMSTAT4_DENIED_NOLOCKS = 30002,
  NFS_NLMSTAT4_BLOCKED = 30003,
  NFS_NLMSTAT4_DENIED_GRACE_PERIOD = 30004,
  NFS_NLMSTAT4_DEADLCK = 30005,
  NFS_NLMSTAT4_ROFS = 30006,
  NFS_NLMSTAT4_STALE_FH = 30007,
  NFS_NLMSTAT4_FBIG = 30008,
  NFS_NLMSTAT4_FAILED = 30009,

  NFS_RPC_SUCCESS = 40000,
  NFS_RPC_CANTENCODEARGS = 40001,
  NFS_RPC_CANTDECODERES = 40002,
  NFS_RPC_CANTSEND = 40003,
  NFS_RPC_CANTRECV = 40004,
  NFS_RPC_TIMEDOUT = 40005,
  NFS_RPC_VERSMISMATCH = 40006,
  NFS_RPC_AUTHERROR = 40007,
  NFS_RPC_PROGUNAVAIL = 40008,
  NFS_RPC_PROGVERSMISMATCH = 40009,
  NFS_RPC_PROCUNAVAIL = 40010,
  NFS_RPC_CANTDECODEARGS = 40011,
  NFS_RPC_SYSTEMERROR = 40012,
  NFS_RPC_NOBROADCAST = 40013,
  NFS_RPC_UNKNOWNHOST = 40014,
  NFS_RPC_UNKNOWNPROTO = 40015,
  NFS_RPC_UNKNOWNADDR = 40016,
  NFS_RPC_PMAPFAILURE = 40017,
  NFS_RPC_PROGNOTREGISTERED = 40018,
  NFS_RPC_N2AXLATEFAILURE = 40019,
  NFS_RPC_FAILED = 40020,
  NFS_RPC_INTR = 40021,
  NFS_RPC_TLIERROR = 40022,
  NFS_RPC_UDERROR = 40023,
  NFS_RPC_INPROGRESS = 40024,
  NFS_RPC_STALERACHANDLE = 40025,

  // Internal error codes
  NFSERR_INTERNAL_NON = 50000,
  NFSERR_INTERNAL_PATH_EMPTY = 50001,
};

class NfsError
{
  public:
    enum EType
    {
      ETYPE_INTERNAL = 0,
      ETYPE_MNT = 1,
      ETYPE_NLM = 2,
      ETYPE_V3 = 3,
      ETYPE_V4 = 4,
      ETYPE_RPC = 5,
    };

    void clear();
    NfsError() { clear(); }
    NfsError(EType type) { clear(); etype = type; }

    void setError4(nfsstat4 code, const std::string err = "");
    void setError3(nfsstat3 code, const std::string err = "");
    void setNlmError(nlm4_stats code, const std::string err = "");
    void setMntError(mountstat3 code, const std::string err = "");
    void setRpcError(clnt_stat code, const std::string err = "");
    void setError(NfsECode code, const std::string err);//internal error

  private:
    std::string getV3ErrorMsg(nfsstat3 status);
    std::string getV4ErrorMsg(nfsstat4 status);
    std::string getNlmErrorMsg(nlm4_stats status);
    std::string getMntErrorMsg(mountstat3 status);

  public:
    NfsECode getErrorCode() const; // for internal use this uniform code
    // Error string is the string representation of error code
    std::string getErrorString(); // for logging and for cout use this
    std::string getErrorMsg() { return msg; }

    NfsError(const NfsError &obj);
    NfsError& operator=(const NfsError &obj);

    bool operator==(const bool &val)const;
    bool operator==(NfsECode ecode)const;
    bool operator==(const NfsError &obj)const;

  private:
    enum EType  etype;
    NfsECode    err;  // internal error code
    nfsstat3    err3; // nfs v3 error code
    nfsstat4    err4; // nfs v4 error code
    nlm4_stats  enlm; // nlm v4 error code
    mountstat3  emnt; // mount v3 error code
    clnt_stat   erpc; // RPC error code
    std::string msg;

    struct v3ErrMap
    {
      nfsstat3           err;
      const std::string  name;
    };
    static v3ErrMap g3Map[];

    struct v4ErrMap
    {
      nfsstat4           err;
      const std::string  name;
    };
    static v4ErrMap g4Map[];

    struct mntErrMap
    {
      mountstat3         err;
      const std::string  name;
    };
    static mntErrMap gmntMap[];

    struct nlmErrMap
    {
      nlm4_stats         err;
      const std::string  name;
    };
    static nlmErrMap gnlmMap[];

    struct rpcErrMap
    {
      clnt_stat          err;
      const std::string  name;
    };
    static rpcErrMap grpcMap[];
};

} // end of namespace
#endif /* _DATA_TYPES_ */
