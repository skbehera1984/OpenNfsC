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
  FILE_TYPE__FIFO = 7,
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
  uint64_t seconds;
  uint32_t nanosecs;
};

struct NfsAttr
{
  NfsAttr();
  void print() const;

  uint32_t    mask[2];
  NfsFileType fileType;
  uint64_t    changeID;
  uint64_t    size;
  NfsFsId     fsid;
  uint64_t    fid;
  uint32_t    fmode;
  uint32_t    nlinks;
  std::string owner;
  std::string group;
  uint64_t    rawDevice;
  uint64_t    spaceUsed;
  uint64_t    mountFid;
  NfsTime     time_access;
  NfsTime     time_metadata;
  NfsTime     time_modify;
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

} // end of namespace
#endif /* _DATA_TYPES_ */
