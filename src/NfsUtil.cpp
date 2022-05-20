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

#include "NfsUtil.h"
#include "RpcPacket.h"
#include "ByteBuffer.h"
#include "RpcDefs.h"

#include <string.h>

namespace OpenNfsC {
namespace NfsUtil {

int decodeFH3(RpcPacketPtr pkt, struct nfs_fh3* fh)
{
  unsigned char* str = NULL;
  uint32 len = 0;
  RETURN_ON_ERROR(pkt->xdrDecodeString(str, len));

  fh->fh3_data.fh3_data_val = (char*)str;
  fh->fh3_data.fh3_data_len = len;
  return 0;
}

int decodePostOpAttr(RpcPacketPtr pkt, struct post_op_attr *post)
{
  uint32 attr;
  RETURN_ON_ERROR(pkt->xdrDecodeUint32(&attr));
  post->attributes_follow = attr;
  if(attr)
  {
    RETURN_ON_ERROR(decodeFattr3(pkt, &post->post_op_attr_u.post_op_attr));
  }
  return 0;
}

int decodePreOpAttr(RpcPacketPtr pkt, struct pre_op_attr *pre)
{
  uint32 attr = 0;
  RETURN_ON_ERROR(pkt->xdrDecodeUint32(&attr));
  pre->attributes_follow = attr;
  if (attr)
  {
#define DECODEWCC(BIT, VAR) RETURN_ON_ERROR(pkt->xdrDecodeUint##BIT((&pre->pre_op_attr_u.pre_op_attr.wcc_##VAR)))
    DECODE_UQUAD(pkt, pre->pre_op_attr_u.pre_op_attr.wcc_size);
    DECODEWCC(32, mtime.time3_seconds);
    DECODEWCC(32, mtime.time3_nseconds);
    DECODEWCC(32, ctime.time3_seconds);
    DECODEWCC(32, ctime.time3_nseconds);
  }
  return 0;
}

int decodeWccData(RpcPacketPtr pkt, struct wcc_data *wcc)
{
  RETURN_ON_ERROR(decodePreOpAttr(pkt, &wcc->wcc_before));
  RETURN_ON_ERROR(decodePostOpAttr(pkt, &wcc->wcc_after));

  return 0;
}


int decodePostOpFH3(RpcPacketPtr pkt, struct post_op_fh3 *postfh)
{
  RETURN_ON_ERROR(pkt->xdrDecodeUint32((uint32*)&postfh->handle_follows));
  if (postfh->handle_follows)
  {
    nfs_fh3* fh = &postfh->post_op_fh3_u.post_op_fh3;
    return decodeFH3(pkt, fh);
  }
  return 0;
}

int encodeSattr3(RpcPacketPtr pkt, sattr3 *sa)
{
#define ENCODEINT(BIT, VAR) pkt->xdrEncodeUint##BIT((VAR))

#define ENCODE(TYPE, VAR, BIT) \
RETURN_ON_ERROR(pkt->xdrEncodeUint32((uint32)sa->sattr3_##VAR.set_it)); \
if (sa->sattr3_##VAR.set_it) \
RETURN_ON_ERROR(ENCODEINT(BIT, sa->sattr3_##VAR.set_##TYPE##3##_u.TYPE))
  ENCODE(mode, mode, 32);
  ENCODE(uid, uid, 32);
  ENCODE(gid, gid, 32);
  ENCODE(size, size, 64);
#undef ENCODE

  RETURN_ON_ERROR(pkt->xdrEncodeUint32((uint32)sa->sattr3_atime.set_it));
  if (sa->sattr3_atime.set_it == TIME_SET_TO_CLIENT_TIME)
  {
    RETURN_ON_ERROR(ENCODEINT(32, sa->sattr3_atime.set_atime_u.atime.time3_seconds));
    RETURN_ON_ERROR(ENCODEINT(32, sa->sattr3_atime.set_atime_u.atime.time3_nseconds));
  }

  RETURN_ON_ERROR(pkt->xdrEncodeUint32((uint32)sa->sattr3_mtime.set_it));
  if (sa->sattr3_mtime.set_it == TIME_SET_TO_CLIENT_TIME)
  {
    RETURN_ON_ERROR(ENCODEINT(32, sa->sattr3_mtime.set_mtime_u.mtime.time3_seconds));
    RETURN_ON_ERROR(ENCODEINT(32, sa->sattr3_mtime.set_mtime_u.mtime.time3_nseconds));
  }
#undef ENCODEINT

  return 0;
}

int decodeFattr3(RpcPacketPtr pkt, fattr3 *sa)
{
  uint32 tmp;
  RETURN_ON_ERROR(pkt->xdrDecodeUint32(&tmp));
  sa->fattr3_type = (ftype3)tmp;
#define DECODEINT(BIT, VAR) RETURN_ON_ERROR(pkt->xdrDecodeUint##BIT(&sa->fattr3_##VAR))
  DECODEINT(32, mode);
  DECODEINT(32, nlink);
  DECODEINT(32, uid);
  DECODEINT(32, gid);
  DECODE_UQUAD(pkt, sa->fattr3_size);
  DECODE_UQUAD(pkt, sa->fattr3_used);
  DECODEINT(32, rdev.specdata1);
  DECODEINT(32, rdev.specdata2);
  DECODE_UQUAD(pkt, sa->fattr3_fsid);
  DECODE_UQUAD(pkt, sa->fattr3_fileid);
  DECODEINT(32, atime.time3_seconds);
  DECODEINT(32, atime.time3_nseconds);
  DECODEINT(32, mtime.time3_seconds);
  DECODEINT(32, mtime.time3_nseconds);
  DECODEINT(32, ctime.time3_seconds);
  DECODEINT(32, ctime.time3_nseconds);
#undef DECODEINT
  return 0;
}

int decodeString(RpcPacketPtr pkt, char**name)
{
  unsigned char* str = NULL;
  uint32 len = 0;
  if (pkt == NULL || name == NULL )
    return -1;
  RETURN_ON_ERROR(pkt->xdrDecodeString(str, len));
  char *filename = new char[len+1];
  memcpy(filename, str, len);
  filename[len] = '\0';
  *name = filename;
  return 0;
}

int decodeStringToBuffer(RpcPacketPtr pkt, Buffer* buf)
{
  unsigned char* str = NULL;
  unsigned char endofstring = 0;
  uint32 len = 0;
  if (pkt == NULL || buf == NULL )
    return -1;
  RETURN_ON_ERROR(pkt->xdrDecodeString(str, len));
  RETURN_ON_ERROR(buf->append(str, len));
  RETURN_ON_ERROR(buf->append(&endofstring, 1));
  return 0;
}

void splitNfsPath(const std::string &Path, std::vector<std::string> &Segments)
{
  // break Path into a list of path segments
  char *ptCopy = strdup(Path.c_str());
  char *ptSave = NULL;
  char *ptToken = strtok_r(ptCopy, "/", &ptSave);
  while( ptToken )
  {
    Segments.push_back( string( ptToken ) );
    ptToken = strtok_r( NULL, "/", &ptSave );
  }
  free( ptCopy );
  ptCopy = NULL;
}

void buildNfsPath(std::string &Path, std::vector<std::string> &Segments)
{
  std::vector<std::string>::iterator tIter = Segments.begin();
  while( tIter != Segments.end() )
  {
    Path += std::string("/") + (*tIter);
    ++tIter;
  }
}

/* We must send two masks even if one of them is zero.
   And maintain the order of these two masks.
   So all calls sending fattr4 must send two masks
 */
int encode_fattr4(RpcPacketPtr packet, const fattr4 *attr)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(attr->attrmask.bitmap4_len));
  for (unsigned int i = 0; i < attr->attrmask.bitmap4_len; i++)
  {
    uint32_t mask = attr->attrmask.bitmap4_val[i];
    RETURN_ON_ERROR(packet->xdrEncodeUint32(mask));
  }

  RETURN_ON_ERROR(packet->xdrEncodeUint32(attr->attr_vals.attrlist4_len));

  char *avals = attr->attr_vals.attrlist4_val;

  uint32_t mask1 = attr->attrmask.bitmap4_val[0];
  uint32_t mask2 = attr->attrmask.bitmap4_val[1];

  if (mask1 !=0 )
  {
    if (mask1 & (1 << FATTR4_SUPPORTED_ATTRS))
    {
    }
    if (mask1 & (1 << FATTR4_TYPE))
    {
      uint32_t *ftype = (uint32_t*)avals;
      RETURN_ON_ERROR(packet->xdrEncodeUint32(*ftype));
      avals += sizeof(uint32_t);
    }
    if (mask1 & (1 << FATTR4_FH_EXPIRE_TYPE))
    {
    }
    if (mask1 & (1 << FATTR4_CHANGE))
    {
      uint64_t *chid = (uint64_t*)avals;
      RETURN_ON_ERROR(packet->xdrEncodeUint64(*chid));
      avals += sizeof(uint64_t);
    }
    if (mask1 & (1 << FATTR4_SIZE))
    {
      uint64_t *size = (uint64_t*)avals;
      RETURN_ON_ERROR(packet->xdrEncodeUint64(*size));
      avals += sizeof(uint64_t);
    }
    if (mask1 & (1 << FATTR4_LINK_SUPPORT))
    {
    }
    if (mask1 & (1 << FATTR4_SYMLINK_SUPPORT))
    {
    }
    if (mask1 & (1 << FATTR4_NAMED_ATTR))
    {
    }
    if (mask1 & (1 << FATTR4_FSID))
    {
    }
    if (mask1 & (1 << FATTR4_UNIQUE_HANDLES))
    {
    }
    if (mask1 & (1 << FATTR4_LEASE_TIME))
    {
    }
    if (mask1 & (1 << FATTR4_RDATTR_ERROR))
    {
    }
    if (mask1 & (1 << FATTR4_FILEHANDLE))
    {
    }
    if (mask1 & (1 << FATTR4_ACL))
    {
      // get the encoded acl size
      uint32_t *size = (uint32_t*)avals;
      avals += sizeof(uint32_t);
      // append the encoded acl
      if (packet->append((unsigned char*)avals, *size) < 0)
      {
        cout << "Failed to append ACL encoded buffer" <<endl;
        return -1;
      }
      // advance the buffer
      avals += (*size);
    }
    if (mask1 & (1 << FATTR4_ACLSUPPORT))
    {
    }
    if (mask1 & (1 << FATTR4_ARCHIVE))
    {
    }
    if (mask1 & (1 << FATTR4_CANSETTIME))
    {
    }
    if (mask1 & (1 << FATTR4_CASE_INSENSITIVE))
    {
    }
    if (mask1 & (1 << FATTR4_CASE_PRESERVING))
    {
    }
    if (mask1 & (1 << FATTR4_CHOWN_RESTRICTED))
    {
    }
    if (mask1 & (1 << FATTR4_FILEID))
    {
    }
    if (mask1 & (1 << FATTR4_FILES_AVAIL))
    {
    }
    if (mask1 & (1 << FATTR4_FILES_FREE))
    {
    }
    if (mask1 & (1 << FATTR4_FILES_TOTAL))
    {
    }
    if (mask1 & (1 << FATTR4_FS_LOCATIONS))
    {
    }
    if (mask1 & (1 << FATTR4_HIDDEN))
    {
    }
    if (mask1 & (1 << FATTR4_HOMOGENEOUS))
    {
    }
    if (mask1 & (1 << FATTR4_MAXFILESIZE))
    {
    }
    if (mask1 & (1 << FATTR4_MAXLINK))
    {
    }
    if (mask1 & (1 << FATTR4_MAXNAME))
    {
    }
    if (mask1 & (1 << FATTR4_MAXREAD))
    {
    }
    if (mask1 & (1 << FATTR4_MAXWRITE))
    {
    }
  }

  if (mask2 != 0)
  {
    if (mask2 & (1 << (FATTR4_MIMETYPE - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_MODE - 32)))
    {
      uint32_t *mode = (uint32_t*)avals;
      RETURN_ON_ERROR(packet->xdrEncodeUint32(*mode));
      avals += sizeof(uint32_t);
    }
    if (mask2 & (1 << (FATTR4_NO_TRUNC - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_NUMLINKS - 32)))
    {
      // TODO sarat implement this
    }
    if (mask2 & (1 << (FATTR4_OWNER - 32)))
    {
      // strings are padded with '\0' to differentiate, skip that null character
      char *owner = avals;
      size_t len = strlen(owner);
      RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)owner, len));
      avals += (len + 1);
    }
    if (mask2 & (1 << (FATTR4_OWNER_GROUP - 32)))
    {
      // strings are padded with '\0' to differentiate, skip that null character
      char *group = avals;
      size_t len = strlen(group);
      RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)group, len));
      avals += (len + 1);
    }
    if (mask2 & (1 << (FATTR4_QUOTA_AVAIL_HARD - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_QUOTA_AVAIL_SOFT - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_QUOTA_USED - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_RAWDEV - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_SPACE_AVAIL - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_SPACE_FREE - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_SPACE_TOTAL - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_SPACE_USED - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_SYSTEM - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_ACCESS - 32)))
    {
      // Flag Not used to set access time
    }
    if (mask2 & (1 << (FATTR4_TIME_ACCESS_SET - 32)))
    {
      // First set the setit to 1
      uint32_t setit = 1;
      RETURN_ON_ERROR(packet->xdrEncodeUint32(setit));

      uint64_t *seconds = (uint64_t*)avals;
      RETURN_ON_ERROR(packet->xdrEncodeUint64(*seconds));
      avals += sizeof(uint64_t);
      uint32_t *nanosec = (uint32_t*)avals;
      RETURN_ON_ERROR(packet->xdrEncodeUint32(*nanosec));
      avals += sizeof(uint32_t);
    }
    if (mask2 & (1 << (FATTR4_TIME_BACKUP - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_CREATE - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_DELTA - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_METADATA - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_MODIFY - 32)))
    {
      // Flag Not used to set modify time
    }
    if (mask2 & (1 << (FATTR4_TIME_MODIFY_SET - 32)))
    {
      // First set the setit to 1
      uint32_t setit = 1;
      RETURN_ON_ERROR(packet->xdrEncodeUint32(setit));

      uint64_t *seconds = (uint64_t*)avals;
      RETURN_ON_ERROR(packet->xdrEncodeUint64(*seconds));
      avals += sizeof(uint64_t);
      uint32_t *nanosec = (uint32_t*)avals;
      RETURN_ON_ERROR(packet->xdrEncodeUint32(*nanosec));
      avals += sizeof(uint32_t);
    }
    if (mask2 & (1 << (FATTR4_MOUNTED_ON_FILEID - 32)))
    {
    }
  }

  return 0;
}

int NfsAttr_fattr4(NfsAttr &attr, fattr4 *fattr)
{
  uint32_t mask1 = attr.mask[0];
  uint32_t mask2 = attr.mask[1];
  uint32_t buflen = sizeof(attr.m_buf);
  char     *buf = attr.m_buf;

  fattr->attrmask.bitmap4_len = 2;
  fattr->attrmask.bitmap4_val = attr.mask;
  fattr->attr_vals.attrlist4_len = 0;
  char *temp_attr = buf;
  fattr->attr_vals.attrlist4_val = buf;

  if (mask1 !=0)
  {
    if (mask1 & (1 << FATTR4_SUPPORTED_ATTRS))
    {
    }
    if (mask1 & (1 << FATTR4_TYPE))
    {
      uint32_t *ptr = (uint32_t*)temp_attr;
      *ptr = attr.fileType;
      fattr->attr_vals.attrlist4_len += 4;
      temp_attr = temp_attr + 4;
      buflen -=4;

    }
    if (mask1 & (1 << FATTR4_FH_EXPIRE_TYPE))
    {
    }
    if (mask1 & (1 << FATTR4_CHANGE))
    {
      uint64_t *ptr = (uint64_t*)temp_attr;
      *ptr = attr.changeID;
      fattr->attr_vals.attrlist4_len += 8;
      temp_attr = temp_attr + 8;
      buflen -=8;
    }
    if (mask1 & (1 << FATTR4_SIZE))
    {
      uint64_t *ptr = (uint64_t*)temp_attr;
      *ptr = attr.size;
      fattr->attr_vals.attrlist4_len += 8;
      temp_attr = temp_attr + 8;
      buflen -=8;
    }
    if (mask1 & (1 << FATTR4_LINK_SUPPORT))
    {
    }
    if (mask1 & (1 << FATTR4_SYMLINK_SUPPORT))
    {
    }
    if (mask1 & (1 << FATTR4_NAMED_ATTR))
    {
    }
    if (mask1 & (1 << FATTR4_FSID))
    {
      NfsFsId *ptr = (NfsFsId*)temp_attr;
      memcpy(ptr, &attr.fsid, sizeof(NfsFsId));
      fattr->attr_vals.attrlist4_len += sizeof(NfsFsId);
      temp_attr = temp_attr + sizeof(NfsFsId);
      buflen -= sizeof(NfsFsId);
    }
    if (mask1 & (1 << FATTR4_UNIQUE_HANDLES))
    {
    }
    if (mask1 & (1 << FATTR4_LEASE_TIME))
    {
    }
    if (mask1 & (1 << FATTR4_RDATTR_ERROR))
    {
    }
    if (mask1 & (1 << FATTR4_FILEHANDLE))
    {
    }
    if (mask1 & (1 << FATTR4_ACL))
    {
      // This size isn't part of encoding but a way to pass the size of encoded acl
      // copy the acl size to buffer, advance the buffer
      uint32_t *ptr = (uint32_t*)temp_attr;
      *ptr = attr.acl.length();
      temp_attr = temp_attr + 4;
      buflen -=4;
      // copy the encoded acl to buffer and advance the buffer
      memcpy(temp_attr, attr.acl.c_str(), attr.acl.length());
      fattr->attr_vals.attrlist4_len += attr.acl.length();
      temp_attr = temp_attr + attr.acl.length();
      buflen -= attr.acl.length();
    }
    if (mask1 & (1 << FATTR4_ACLSUPPORT))
    {
    }
    if (mask1 & (1 << FATTR4_ARCHIVE))
    {
    }
    if (mask1 & (1 << FATTR4_CANSETTIME))
    {
    }
    if (mask1 & (1 << FATTR4_CASE_INSENSITIVE))
    {
    }
    if (mask1 & (1 << FATTR4_CASE_PRESERVING))
    {
    }
    if (mask1 & (1 << FATTR4_CHOWN_RESTRICTED))
    {
    }
    if (mask1 & (1 << FATTR4_FILEID))
    {
    }
    if (mask1 & (1 << FATTR4_FILES_AVAIL))
    {
    }
    if (mask1 & (1 << FATTR4_FILES_FREE))
    {
    }
    if (mask1 & (1 << FATTR4_FILES_TOTAL))
    {
    }
    if (mask1 & (1 << FATTR4_FS_LOCATIONS))
    {
    }
    if (mask1 & (1 << FATTR4_HIDDEN))
    {
    }
    if (mask1 & (1 << FATTR4_HOMOGENEOUS))
    {
    }
    if (mask1 & (1 << FATTR4_MAXFILESIZE))
    {
    }
    if (mask1 & (1 << FATTR4_MAXLINK))
    {
    }
    if (mask1 & (1 << FATTR4_MAXNAME))
    {
    }
    if (mask1 & (1 << FATTR4_MAXREAD))
    {
    }
    if (mask1 & (1 << FATTR4_MAXWRITE))
    {
    }
  }
  if (mask2 !=0)
  {
    if (mask2 & (1 << (FATTR4_MIMETYPE - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_MODE - 32)))
    {
      uint32_t *ptr = (uint32_t*)temp_attr;
      *ptr = attr.fmode;
      fattr->attr_vals.attrlist4_len += 4;
      temp_attr = temp_attr + 4;
      buflen -=4;
    }
    if (mask2 & (1 << (FATTR4_NO_TRUNC - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_NUMLINKS - 32)))
    {
      uint32_t *ptr = (uint32_t*)temp_attr;
      *ptr = attr.nlinks;
      fattr->attr_vals.attrlist4_len += 4;
      temp_attr = temp_attr + 4;
      buflen -=4;
    }
    if (mask2 & (1 << (FATTR4_OWNER - 32)))
    {
      // padding the string with a '\0' to differentiate
      char *owner = temp_attr;
      int length = attr.owner.length();
      int size = length + 1;
      strncpy(owner, attr.owner.c_str(), length);
      owner += length;
      *owner = '\0';
      temp_attr = temp_attr + size;
      buflen -= size;
      // calculate what actual size will be consumed by string after encoding
      int padding = 0;
      if ((length & 0x03) != 0)
      {
        padding = 4 - (length & 0x03);
      }
      // encoded size = length place holder + length + padding
      fattr->attr_vals.attrlist4_len += (4 + length + padding);
    }
    if (mask2 & (1 << (FATTR4_OWNER_GROUP - 32)))
    {
      // padding the string with a '\0' to differentiate
      char *group = temp_attr;
      int length = attr.group.length();
      int size = length + 1;
      strncpy(group, attr.group.c_str(), length);
      group += length;
      *group = '\0';
      temp_attr = temp_attr + size;
      buflen -= size;
      // calculate what actual size will be consumed by string after encoding
      int padding = 0;
      if ((length & 0x03) != 0)
      {
        padding = 4 - (length & 0x03);
      }
      // encoded size = length place holder + length + padding
      fattr->attr_vals.attrlist4_len += (4 + length + padding);
    }
    if (mask2 & (1 << (FATTR4_QUOTA_AVAIL_HARD - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_QUOTA_AVAIL_SOFT - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_QUOTA_USED - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_RAWDEV - 32)))
    {
      uint64_t *ptr = (uint64_t*)temp_attr;
      *ptr = attr.rawDevice;
      fattr->attr_vals.attrlist4_len += 8;
      temp_attr = temp_attr + 8;
      buflen -=8;
    }
    if (mask2 & (1 << (FATTR4_SPACE_AVAIL - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_SPACE_FREE - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_SPACE_TOTAL - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_SPACE_USED - 32)))
    {
      uint64_t *ptr = (uint64_t*)temp_attr;
      *ptr = attr.bytes_used;
      fattr->attr_vals.attrlist4_len += 8;
      temp_attr = temp_attr + 8;
      buflen -=8;
    }
    if (mask2 & (1 << (FATTR4_SYSTEM - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_ACCESS - 32)))
    {
      // Flag Not used for set access time
    }
    if (mask2 & (1 << (FATTR4_TIME_ACCESS_SET - 32)))
    {
      fattr->attr_vals.attrlist4_len += sizeof(uint32_t); // for set it field
      uint64_t *sec = (uint64_t*)temp_attr;
      *sec = attr.time_access.seconds;
      fattr->attr_vals.attrlist4_len += sizeof(uint64_t);
      temp_attr = temp_attr + sizeof(uint64_t);
      buflen -= sizeof(uint64_t);
      uint32_t *nanosec = (uint32_t*)temp_attr;
      *nanosec = attr.time_access.nanosecs;
      fattr->attr_vals.attrlist4_len += sizeof(uint32_t);
      temp_attr = temp_attr + sizeof(uint32_t);
      buflen -= sizeof(uint32_t);
    }
    if (mask2 & (1 << (FATTR4_TIME_BACKUP - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_CREATE - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_DELTA - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_METADATA - 32)))
    {
      // Meta time or ctime can't be set
    }
    if (mask2 & (1 << (FATTR4_TIME_MODIFY - 32)))
    {
      // Flag Not used for set modify time
    }
    if (mask2 & (1 << (FATTR4_TIME_MODIFY_SET - 32)))
    {
      fattr->attr_vals.attrlist4_len += sizeof(uint32_t); // for set it field
      uint64_t *sec = (uint64_t*)temp_attr;
      *sec = attr.time_modify.seconds;
      fattr->attr_vals.attrlist4_len += sizeof(uint64_t);
      temp_attr = temp_attr + sizeof(uint64_t);
      buflen -= sizeof(uint64_t);
      uint32_t *nanosec = (uint32_t*)temp_attr;
      *nanosec = attr.time_modify.nanosecs;
      fattr->attr_vals.attrlist4_len += sizeof(uint32_t);
      temp_attr = temp_attr + sizeof(uint32_t);
      buflen -= sizeof(uint32_t);
    }
    if (mask2 & (1 << (FATTR4_MOUNTED_ON_FILEID - 32)))
    {
    }
  }

  return 0;
}


// mask1 and mask2 are inmasks that we used to send getattr request
// 2 masks are sufficient to hold all attributes
int decode_fattr4(fattr4 *fattr, uint32_t mask1, uint32_t mask2, NfsAttr &attr)
{
  RpcPacketPtr attrPacket = new RpcPacket(0);
  if ( attrPacket.empty() )
  {
    syslog(LOG_ERR, "Failed to allocate attr packet for decoding\n");
    return -1;
  }

  if (attrPacket->append((unsigned char*)fattr->attr_vals.attrlist4_val,
                         fattr->attr_vals.attrlist4_len) < 0)
  {
    syslog(LOG_ERR, "Failed to append to attr packet for decoding\n");
    return -1;
  }

  if (mask1 !=0)
  {
    attr.mask[0] = mask1;
    /* The order of these if blocks must be maintained
       The server sends the attrs in a particular order. Like the first bit set in the first mask is added first.
     */
    if (mask1 & (1 << FATTR4_SUPPORTED_ATTRS))
    {
    }
    if (mask1 & (1 << FATTR4_TYPE))
    {
      uint32_t ftype = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&ftype));
      attr.fileType = (NfsFileType)ftype;
    }
    if (mask1 & (1 << FATTR4_FH_EXPIRE_TYPE))
    {
    }
    if (mask1 & (1 << FATTR4_CHANGE))
    {
      uint64 chid = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&chid));
      attr.changeID = chid;
    }
    if (mask1 & (1 << FATTR4_SIZE))
    {
      uint64 sz = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&sz));
      attr.size = sz;
    }
    if (mask1 & (1 << FATTR4_LINK_SUPPORT))
    {
    }
    if (mask1 & (1 << FATTR4_SYMLINK_SUPPORT))
    {
    }
    if (mask1 & (1 << FATTR4_NAMED_ATTR))
    {
    }
    if (mask1 & (1 << FATTR4_FSID))
    {
      uint64 major = 0, minor = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&major));
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&minor));
      attr.fsid.FSIDMajor = major;
      attr.fsid.FSIDMinor = minor;
    }
    if (mask1 & (1 << FATTR4_UNIQUE_HANDLES))
    {
    }
    if (mask1 & (1 << FATTR4_LEASE_TIME))
    {
    }
    if (mask1 & (1 << FATTR4_RDATTR_ERROR))
    {
    }
    if (mask1 & (1 << FATTR4_FILEHANDLE))
    {
    }
    if (mask1 & (1 << FATTR4_ACL))
    {
      Nfs4ACL acl;
      uint32 aclSize = 0;
      uint32 noOfAces = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&noOfAces));
      acl.no_of_aces = noOfAces;
      aclSize += sizeof(uint32_t);

      for (uint32 i = 0; i < noOfAces; i++)
      {
        uint32 aceSize = 0;
        uint32 acetype = 0, aceflag = 0, accessmask = 0;
        RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&acetype));
        aceSize += sizeof(uint32_t);
        RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&aceflag));
        aceSize += sizeof(uint32_t);
        RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&accessmask));
        aceSize += sizeof(uint32_t);
        unsigned char *value = NULL;
        uint32 value_len = 0;
        RETURN_ON_ERROR(attrPacket->xdrDecodeString(value, value_len));
        Nfs4ACE ace;
        ace.ACEType = acetype;
        ace.ACEFlag = aceflag;
        ace.AccessMask = accessmask;
        ace.who = std::string((char*)value, value_len);
        acl.aces.push_back(ace);
        int padSize = 0;
        if (value_len%4 != 0)
          padSize = 4 - value_len%4;
        aceSize += (sizeof(uint32_t) + value_len + padSize);
        aclSize += aceSize;
      }
      // encode the acl again to get raw bytes
      // we decoded first because we never knew what was the size
      RpcPacketPtr aclPacket = new RpcPacket(aclSize);
      RETURN_ON_ERROR(aclPacket->xdrEncodeUint32(acl.no_of_aces));
      for (auto &ace : acl.aces)
      {
        RETURN_ON_ERROR(aclPacket->xdrEncodeUint32(ace.ACEType));
        RETURN_ON_ERROR(aclPacket->xdrEncodeUint32(ace.ACEFlag));
        RETURN_ON_ERROR(aclPacket->xdrEncodeUint32(ace.AccessMask));
        RETURN_ON_ERROR(aclPacket->xdrEncodeString((unsigned char*)ace.who.c_str(), ace.who.length()));
      }
      attr.acl = std::string((char*)aclPacket->getBuffer(), aclPacket->getSize());
    }
    if (mask1 & (1 << FATTR4_ACLSUPPORT))
    {
    }
    if (mask1 & (1 << FATTR4_ARCHIVE))
    {
    }
    if (mask1 & (1 << FATTR4_CANSETTIME))
    {
    }
    if (mask1 & (1 << FATTR4_CASE_INSENSITIVE))
    {
    }
    if (mask1 & (1 << FATTR4_CASE_PRESERVING))
    {
    }
    if (mask1 & (1 << FATTR4_CHOWN_RESTRICTED))
    {
    }
    if (mask1 & (1 << FATTR4_FILEID))
    {
      uint64 fid = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&fid));
      attr.fid = fid;
    }
    if (mask1 & (1 << FATTR4_FILES_AVAIL))
    {
      uint64 filesAvail = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&filesAvail));
      attr.files_avail = filesAvail;
    }
    if (mask1 & (1 << FATTR4_FILES_FREE))
    {
      uint64 filesFree = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&filesFree));
      attr.files_free = filesFree;
    }
    if (mask1 & (1 << FATTR4_FILES_TOTAL))
    {
      uint64 filesTotal = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&filesTotal));
      attr.files_total = filesTotal;
    }
    if (mask1 & (1 << FATTR4_FS_LOCATIONS))
    {
    }
    if (mask1 & (1 << FATTR4_HIDDEN))
    {
    }
    if (mask1 & (1 << FATTR4_HOMOGENEOUS))
    {
    }
    if (mask1 & (1 << FATTR4_MAXFILESIZE))
    {
    }
    if (mask1 & (1 << FATTR4_MAXLINK))
    {
    }
    if (mask1 & (1 << FATTR4_MAXNAME))
    {
      uint32 maxName = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&maxName));
      attr.name_max = maxName;
    }
    if (mask1 & (1 << FATTR4_MAXREAD))
    {
    }
    if (mask1 & (1 << FATTR4_MAXWRITE))
    {
    }
  }
  if (mask2 !=0)
  {
    attr.mask[1] = mask2;
    if (mask2 & (1 << (FATTR4_MIMETYPE - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_MODE - 32)))
    {
      uint32 mode = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&mode));
      attr.fmode = mode;
    }
    if (mask2 & (1 << (FATTR4_NO_TRUNC - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_NUMLINKS - 32)))
    {
      uint32 nlinks = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&nlinks));
      attr.nlinks = nlinks;
    }
    if (mask2 & (1 << (FATTR4_OWNER - 32)))
    {
      unsigned char *value = NULL;
      uint32 value_len = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeString(value, value_len));
      attr.owner = std::string((char*)value, value_len);
    }
    if (mask2 & (1 << (FATTR4_OWNER_GROUP - 32)))
    {
      unsigned char* value = NULL;
      uint32 value_len = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeString(value, value_len));
      attr.group = std::string((char*)value, value_len);
    }
    if (mask2 & (1 << (FATTR4_QUOTA_AVAIL_HARD - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_QUOTA_AVAIL_SOFT - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_QUOTA_USED - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_RAWDEV - 32)))
    {
      uint64 rdev = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&rdev));
      attr.rawDevice = rdev;
    }
    if (mask2 & (1 << (FATTR4_SPACE_AVAIL - 32)))
    {
      uint64 spcAvail = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&spcAvail));
      attr.bytes_avail = spcAvail;
    }
    if (mask2 & (1 << (FATTR4_SPACE_FREE - 32)))
    {
      uint64 spcFree = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&spcFree));
      attr.bytes_free = spcFree;
    }
    if (mask2 & (1 << (FATTR4_SPACE_TOTAL - 32)))
    {
      uint64 spcTotal = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&spcTotal));
      attr.bytes_total = spcTotal;
    }
    if (mask2 & (1 << (FATTR4_SPACE_USED - 32)))
    {
      uint64 spcusd = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&spcusd));
      attr.bytes_used = spcusd;
    }
    if (mask2 & (1 << (FATTR4_SYSTEM - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_ACCESS - 32)))
    {
      uint64 seconds = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&seconds));
      attr.time_access.seconds = seconds;
      uint32 nanosecs = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&nanosecs));
      attr.time_access.nanosecs = nanosecs;
    }
    if (mask2 & (1 << (FATTR4_TIME_ACCESS_SET - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_BACKUP - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_CREATE - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_DELTA - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_TIME_METADATA - 32)))
    {
      uint64 seconds = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&seconds));
      attr.time_metadata.seconds = seconds;
      uint32 nanosecs = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&nanosecs));
      attr.time_metadata.nanosecs = nanosecs;
    }
    if (mask2 & (1 << (FATTR4_TIME_MODIFY - 32)))
    {
      uint64 seconds = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint64(&seconds));
      attr.time_modify.seconds = seconds;
      uint32 nanosecs = 0;
      RETURN_ON_ERROR(attrPacket->xdrDecodeUint32(&nanosecs));
      attr.time_modify.nanosecs = nanosecs;
    }
    if (mask2 & (1 << (FATTR4_TIME_MODIFY_SET - 32)))
    {
    }
    if (mask2 & (1 << (FATTR4_MOUNTED_ON_FILEID - 32)))
    {
    }
  }

  return 0;
}

}} // end of namespace
