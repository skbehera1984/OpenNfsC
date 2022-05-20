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

#include "NfsCall.h"
#include "RpcPacket.h"
#include "RpcConnection.h"
#include "NfsUtil.h"
#include "RpcDefs.h"
#include <syslog.h>
#include <iostream>

namespace OpenNfsC {
namespace NFSv3{

#define DECODE_STATUS() \
   \
    uint32 status; \
    RpcPacketPtr reply = getReply(); \
    if (reply == NULL) \
        return -1; \
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&status)); \
    res.status = (nfsstat3)status; \
   \

GetAttrCall::GetAttrCall(const GETATTR3args& arg):
  RemoteCall(NFS, NFS_V3_GETATTR),args(arg),res()
{
}

GetAttrCall::~GetAttrCall()
{
}

int GetAttrCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.getattr3_object;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    return 0;
  }
  return -1;
}

int GetAttrCall::decodeResults()
{
  uint32 status;
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  RETURN_ON_ERROR(reply->xdrDecodeUint32(&status));
  res.status = (nfsstat3)status;
  if (status == 0)
  {
    RETURN_ON_ERROR(NfsUtil::decodeFattr3(reply,&res.GETATTR3res_u.getattr3ok.getattr3_obj_attributes));
    return 0;
  }
  return -1;
}

SetAttrCall::SetAttrCall( const SETATTR3args& arg):
 RemoteCall(NFS, NFS_V3_SETATTR),args(arg),res()
{
}

SetAttrCall::~SetAttrCall()
{
}

int SetAttrCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.setattr3_object;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    RETURN_ON_ERROR(NfsUtil::encodeSattr3(request, &args.setattr3_new_attributes));
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.setattr3_guard.check));
    if (args.setattr3_guard.check)
      RETURN_ON_ERROR(ENCODEVAR(request,args.setattr3_guard.sattrguard3_u.sattr3_obj_ctime));
    return 0;
  }
  return -1;
}

int SetAttrCall::decodeResults()
{
  DECODE_STATUS();
  return 0;
}

// lookup
LookUpCall::LookUpCall(const LOOKUP3args&  arg)
 :RemoteCall(NFS, NFS_V3_LOOKUP),args(arg), res()
{
}

LookUpCall::~LookUpCall()
{
}

int LookUpCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.lookup3_what.dirop3_dir;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    char * filename = args.lookup3_what.dirop3_name;
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));
    return 0;
  }
  return -1;
}

int LookUpCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    RETURN_ON_ERROR(NfsUtil::decodeFH3(reply, &res.LOOKUP3res_u.lookup3ok.lookup3_object));
    struct post_op_attr* postAttr = &res.LOOKUP3res_u.lookup3ok.lookup3_obj_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    postAttr = &res.LOOKUP3res_u.lookup3ok.lookup3_dir_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    return 0;
  }
  else
  {
    struct post_op_attr* postAttr = &res.LOOKUP3res_u.lookup3fail.lookup3fail_dir_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    return 0;
  }
  return -1;
}

// access call
AccessCall::AccessCall(const ACCESS3args&  arg)
 :RemoteCall(NFS, NFS_V3_ACCESS),args(arg), res()
{
}

AccessCall::~AccessCall()
{
}

int AccessCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.access3_object;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    RETURN_ON_ERROR(request->xdrEncodeUint32((args.access3_access)));
    return 0;
  }
  return -1;
}

int AccessCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    struct post_op_attr* postAttr = &res.ACCESS3res_u.access3ok.access3_obj_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    uint32 access;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&access));
    res.ACCESS3res_u.access3ok.access3_res = access;
    return 0;
  }
  else
  {
    struct post_op_attr* postAttr = &res.ACCESS3res_u.access3fail.access3fail_obj_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    return 0;
  }
  return -1;
}

// readlink call
ReadLinkCall::ReadLinkCall(const READLINK3args&  arg)
 :RemoteCall(NFS, NFS_V3_READLINK),args(arg), res()
{
}

ReadLinkCall::~ReadLinkCall()
{
  if (res.READLINK3res_u.readlink3ok.readlink3_data)
    delete [] res.READLINK3res_u.readlink3ok.readlink3_data;
}

int ReadLinkCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.readlink3_symlink;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    return 0;
  }
  return -1;
}

int ReadLinkCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    struct post_op_attr* postAttr = &res.READLINK3res_u.readlink3ok.readlink3_symlink_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    RETURN_ON_ERROR(NfsUtil::decodeString(reply, &res.READLINK3res_u.readlink3ok.readlink3_data));
    return 0;
  }
  else
  {
    struct post_op_attr* postAttr = &res.READLINK3res_u.readlink3fail.readlink3fail_symlink_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    return 0;
  }
  return -1;
}

// read call
ReadCall::ReadCall(const READ3args&  arg)
 :RemoteCall(NFS, NFS_V3_READ),args(arg), res()
{
}

ReadCall::~ReadCall()
{
}

int ReadCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.read3_file;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    RETURN_ON_ERROR(request->xdrEncodeUint64((args.read3_offset)));
    RETURN_ON_ERROR(request->xdrEncodeUint32((args.read3_count)));
    return 0;
  }
  return -1;
}

int ReadCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    struct post_op_attr* postAttr = &res.READ3res_u.read3ok.read3_file_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    uint32 readtmp;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&readtmp));
    res.READ3res_u.read3ok.read3_count_res = readtmp;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&readtmp));
    res.READ3res_u.read3ok.read3_eof = readtmp;

    unsigned char* dataPtr = NULL;
    uint32 dataLen;
    RETURN_ON_ERROR(reply->xdrDecodeString(dataPtr, dataLen));

    if (res.READ3res_u.read3ok.read3_count_res != dataLen)
      return -1;

    res.READ3res_u.read3ok.read3_data.read3_data_len = dataLen;
    res.READ3res_u.read3ok.read3_data.read3_data_val = (char*)dataPtr;

    return 0;
  }
  else
  {
    struct post_op_attr* postAttr = &res.READ3res_u.read3fail.read3fail_file_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    return 0;
  }
  return -1;
}

// write call
WriteCall::WriteCall(const WRITE3args&  arg)
 :RemoteCall(NFS, NFS_V3_WRITE),args(arg), res()
{
}

WriteCall::~WriteCall()
{
}

int WriteCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.write3_file;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    RETURN_ON_ERROR(request->xdrEncodeUint64((args.write3_offset)));
    RETURN_ON_ERROR(request->xdrEncodeUint32((args.write3_count)));
    RETURN_ON_ERROR(request->xdrEncodeUint32((args.write3_stable)));
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(args.write3_data.write3_data_val, args.write3_data.write3_data_len));
    return 0;
  }
  return -1;
}

int WriteCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    struct wcc_data* wccData = &res.WRITE3res_u.write3ok.write3_file_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));

    uint32 writeInt;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&writeInt));
    res.WRITE3res_u.write3ok.write3_count_res = writeInt;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&writeInt));
    res.WRITE3res_u.write3ok.write3_committed = (enum stable_how)writeInt;

    RETURN_ON_ERROR(DECODEVAR(reply, res.WRITE3res_u.write3ok.write3_verf));
    return 0;
  }
  else
  {
    struct wcc_data* wccData = &res.WRITE3res_u.write3fail.write3fail_file_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  return -1;
}

//crate call
CreateCall::CreateCall(const CREATE3args&  arg)
 :RemoteCall(NFS, NFS_V3_CREATE),args(arg), res()
{
}

CreateCall::~CreateCall()
{
}

int CreateCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.create3_where.dirop3_dir;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    char * filename = args.create3_where.dirop3_name;
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.create3_how.mode));

    switch (args.create3_how.mode)
    {
      case CREATE_UNCHECKED:
      case CREATE_GUARDED:
        RETURN_ON_ERROR(NfsUtil::encodeSattr3(request, &args.create3_how.createhow3_u.create3_obj_attributes));
      break;
      case CREATE_EXCLUSIVE:
        RETURN_ON_ERROR(ENCODEVAR(request, args.create3_how.createhow3_u.create3_verf));
      default:
      break;
    }
    return 0;
  }
  return -1;
}

int CreateCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    RETURN_ON_ERROR(NfsUtil::decodePostOpFH3(reply, &res.CREATE3res_u.create3_ok.create3_obj));
    struct post_op_attr* postAttr = &res.CREATE3res_u.create3_ok.create3_obj_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    struct wcc_data* wccData = &res.CREATE3res_u.create3_ok.create3_dir_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  else
  {
    struct wcc_data* wccData = &res.CREATE3res_u.create3fail.create3fail_dir_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  return -1;
}


// mkdir call
MkdirCall::MkdirCall(const MKDIR3args&  arg)
 :RemoteCall(NFS, NFS_V3_MKDIR),args(arg), res()
{
}

MkdirCall::~MkdirCall()
{
}

int MkdirCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.mkdir3_where.dirop3_dir;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    char * filename = args.mkdir3_where.dirop3_name;
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));
    RETURN_ON_ERROR(NfsUtil::encodeSattr3(request, &args.mkdir3_attributes));
    return 0;
  }
  return -1;
}

int MkdirCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    RETURN_ON_ERROR(NfsUtil::decodePostOpFH3(reply, &res.MKDIR3res_u.mkdir3ok.mkdir3_obj));
    struct post_op_attr* postAttr = &res.MKDIR3res_u.mkdir3ok.mkdir3_obj_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    struct wcc_data* wccData = &res.MKDIR3res_u.mkdir3ok.mkdir3_dir_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  else
  {
    struct wcc_data* wccData = &res.MKDIR3res_u.mkdir3fail.mkdir3fail_dir_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  return -1;
}

// symlink
SymLinkCall::SymLinkCall(const SYMLINK3args&  arg)
 :RemoteCall(NFS, NFS_V3_SYMLINK),args(arg), res()
{
}

SymLinkCall::~SymLinkCall()
{
}

int SymLinkCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.symlink3_where.dirop3_dir;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    char * filename = args.symlink3_where.dirop3_name;
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));
    RETURN_ON_ERROR(NfsUtil::encodeSattr3(request, &args.symlink3_symlink.symlink3_attributes));
    char *symname = args.symlink3_symlink.symlink3_data;
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)symname, strlen((const char*)symname)));
    return 0;
  }
  return -1;
}

int SymLinkCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    RETURN_ON_ERROR(NfsUtil::decodePostOpFH3(reply, &res.SYMLINK3res_u.symlink3ok.symlink3_obj));
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, &res.SYMLINK3res_u.symlink3ok.symlink3_obj_attributes));
    struct wcc_data* wccData = &res.SYMLINK3res_u.symlink3ok.symlink3_dir_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  else
  {
    struct wcc_data* wccData = &res.SYMLINK3res_u.symlink3fail.symlink3fail_dir_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  return -1;
}

// mknod
MknodCall::MknodCall(const MKNOD3args&  arg)
 :RemoteCall(NFS, NFS_V3_MKNOD),args(arg), res()
{
}

MknodCall::~MknodCall()
{
}

int MknodCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.mknod3_where.dirop3_dir;
    char * filename = args.mknod3_where.dirop3_name;

    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));

    RETURN_ON_ERROR(request->xdrEncodeUint32(args.mknod3_what.type));

    switch (args.mknod3_what.type)
    {
      case NF3CHR:
      case NF3BLK:
        RETURN_ON_ERROR(NfsUtil::encodeSattr3(request, &args.mknod3_what.mknoddata3_u.mknod3_device.dev3_attributes));
        RETURN_ON_ERROR(request->xdrEncodeUint32(args.mknod3_what.mknoddata3_u.mknod3_device.dev3_spec.specdata1));
        RETURN_ON_ERROR(request->xdrEncodeUint32(args.mknod3_what.mknoddata3_u.mknod3_device.dev3_spec.specdata2));
      break;
      case NF3SOCK:
      case NF3FIFO:
        RETURN_ON_ERROR(NfsUtil::encodeSattr3(request, &args.mknod3_what.mknoddata3_u.mknod3_pipe_attributes));
      default:
      break;
    }
    return 0;
  }
  return -1;
}

int MknodCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    struct post_op_fh3 * postfh = &res.MKNOD3res_u.mknod3ok.mknod3_obj;
    struct post_op_attr* postattr = &res.MKNOD3res_u.mknod3ok.mknod3_obj_attributes;
    struct wcc_data* wccData = &res.MKNOD3res_u.mknod3ok.mknod3_dir_wcc;
    RETURN_ON_ERROR(NfsUtil::decodePostOpFH3(reply, postfh));
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postattr));
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  else
  {
    struct wcc_data* wccData = &res.MKNOD3res_u.mknod3fail.mknod3fail_dir_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  return -1;
}

// remove
RemoveCall::RemoveCall(const REMOVE3args&  arg)
 :RemoteCall(NFS, NFS_V3_REMOVE),args(arg), res()
{
}

RemoveCall::~RemoveCall()
{
}

int RemoveCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.remove3_object.dirop3_dir;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    char * filename = args.remove3_object.dirop3_name;
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));
    return 0;
  }
  return -1;
}

int RemoveCall::decodeResults()
{
  DECODE_STATUS();
  struct wcc_data* wccData = &res.REMOVE3res_u.remove3ok.remove3_dir_wcc;
  RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
  return 0;
}

// rmdir
RmdirCall::RmdirCall(const RMDIR3args&  arg)
 :RemoteCall(NFS, NFS_V3_RMDIR),args(arg), res()
{
}

RmdirCall::~RmdirCall()
{
}

int RmdirCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.rmdir3_object.dirop3_dir;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    char * filename = args.rmdir3_object.dirop3_name;
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));
    return 0;
  }
  return -1;
}

int RmdirCall::decodeResults()
{
  DECODE_STATUS();
  struct wcc_data* wccData = &res.RMDIR3res_u.rmdir3ok.rmdir3_dir_wcc;
  RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
  return 0;
}

// rename
RenameCall::RenameCall(const RENAME3args&  arg)
 :RemoteCall(NFS, NFS_V3_RENAME),args(arg), res()
{
}

RenameCall::~RenameCall()
{
}

int RenameCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.rename3_from.dirop3_dir;
    char * filename = args.rename3_from.dirop3_name;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));

    fh = &args.rename3_to.dirop3_dir;
    filename = args.rename3_to.dirop3_name;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));

    return 0;
  }
  return -1;
}

int RenameCall::decodeResults()
{
  DECODE_STATUS();
  struct wcc_data* wccData = &res.RENAME3res_u.rename3ok.rename3_fromdir_wcc;
  RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
  wccData = &res.RENAME3res_u.rename3ok.rename3_todir_wcc;
  RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
  return 0;
}


// link
LinkCall::LinkCall(const LINK3args&  arg)
 :RemoteCall(NFS, NFS_V3_LINK),args(arg), res()
{
}

LinkCall::~LinkCall()
{
}

int LinkCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.link3_file;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));

    fh = &args.link3_link.dirop3_dir;
    char* filename = args.link3_link.dirop3_name;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)filename, strlen((const char*)filename)));

    return 0;
  }
  return -1;
}

int LinkCall::decodeResults()
{
  DECODE_STATUS();
  struct post_op_attr* postAttr = &res.LINK3res_u.link3ok.link3_file_attributes;
  RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
  struct wcc_data* wccData = &res.LINK3res_u.link3ok.link3_linkdir_wcc;
  RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
  return 0;
}

// Readdir
ReaddirCall::ReaddirCall(const READDIR3args& arg):
  RemoteCall(NFS, NFS_V3_READDIR),args(arg),res(),m_decodedStringBuffer(NULL)
{
}

ReaddirCall::~ReaddirCall()
{
  entry3* entry = res.READDIR3res_u.readdir3ok.readdir3_reply.dirlist3_entries;
  while ( entry != NULL )
  {
    entry3* nextEntry = entry->entry3_nextentry;
    delete entry;
    entry = nextEntry;
  }

  if (m_decodedStringBuffer)
    delete m_decodedStringBuffer;
}

int ReaddirCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.readdir3_dir;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    uint64 cookie = args.readdir3_cookie;
    RETURN_ON_ERROR((request->xdrEncodeUint64(cookie)));
    RETURN_ON_ERROR(ENCODEVAR(request, args.readdir3_cookieverf));

    uint32 count = args.readdir3_count;
    if (count > 32768) count = 32768;
    RETURN_ON_ERROR(request->xdrEncodeUint32(count));
    return 0;
  }
  else
    return -1;
}

int ReaddirCall::decodeResults()
{
  uint32 status;
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  RETURN_ON_ERROR(reply->xdrDecodeUint32(&status));
  res.status = (nfsstat3)status;
  if (status == 0)
  {
    struct post_op_attr* postAttr = &res.READDIR3res_u.readdir3ok.readdir3_dir_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));

    cookieverf3* verf = &res.READDIR3res_u.readdir3ok.readdir3_cookieverf_res;
    RETURN_ON_ERROR(reply->xdrDecodeFixedOpaque((unsigned char*)verf, sizeof(cookieverf3)));

    res.READDIR3res_u.readdir3ok.readdir3_reply.dirlist3_entries = NULL;

    uint32 entryFollow = 0;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&entryFollow));
    int i = 0;
    entry3* lastEntry = NULL;
    m_decodedStringBuffer = new Buffer(reply->getCapacity());
    if (m_decodedStringBuffer == NULL)
      return -1;

    while (entryFollow)
    {
      entry3* newEntry = new entry3;
      DECODE_UQUAD(reply, newEntry->entry3_fileid);;
      char* filename = reinterpret_cast<char*>(m_decodedStringBuffer->end());
      RETURN_ON_ERROR(NfsUtil::decodeStringToBuffer(reply, m_decodedStringBuffer));
      newEntry->entry3_name = filename;

      DECODE_UQUAD(reply, newEntry->entry3_cookie);;

      newEntry->entry3_nextentry = NULL;
      ++i;
      RETURN_ON_ERROR(reply->xdrDecodeUint32(&entryFollow));

      if (lastEntry == NULL)
      {
        lastEntry = newEntry;
        res.READDIR3res_u.readdir3ok.readdir3_reply.dirlist3_entries = newEntry;
      }
      else
      {
        lastEntry->entry3_nextentry = newEntry;
      }
      lastEntry = newEntry;
    }
    uint32 listEOF = 0;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&listEOF));
    res.READDIR3res_u.readdir3ok.readdir3_reply.dirlist3_eof = (listEOF == 0);
    return 0;
  }
  return -1;
}


// ReaddirPlus
ReaddirPlusCall::ReaddirPlusCall(const READDIRPLUS3args& arg):
  RemoteCall(NFS, NFS_V3_READDIRPLUS),args(arg),res(),m_decodedStringBuffer(NULL)
{
}

ReaddirPlusCall::~ReaddirPlusCall()
{
  entryplus3* entry = res.READDIRPLUS3res_u.readdirplus3ok.readdirplus3_reply.dirlistplus3_entries;
  while ( entry != NULL )
  {
    entryplus3* nextEntry = entry->entryplus3_nextentry;
    delete entry;
    entry = nextEntry;
  }

  if (m_decodedStringBuffer)
    delete m_decodedStringBuffer;
}

int ReaddirPlusCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.readdirplus3_dir;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    uint64 cookie = args.readdirplus3_cookie;
    RETURN_ON_ERROR(request->xdrEncodeUint64(cookie));
    RETURN_ON_ERROR(ENCODEVAR(request, args.readdirplus3_cookieverf));

    uint32 count = args.readdirplus3_dircount;
    uint32 maxcount = args.readdirplus3_maxcount;
    if (maxcount > 32768) maxcount = 32768;
    RETURN_ON_ERROR(request->xdrEncodeUint32(count));
    RETURN_ON_ERROR(request->xdrEncodeUint32(maxcount));
    return 0;
  }
  else
    return -1;
}

int ReaddirPlusCall::decodeResults()
{
  uint32 status;
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  RETURN_ON_ERROR(reply->xdrDecodeUint32(&status));
  res.status = (nfsstat3)status;
  if (status == 0)
  {
    struct post_op_attr* postAttr = &res.READDIRPLUS3res_u.readdirplus3ok.readdirplus3_dir_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));

    cookieverf3* verf = &res.READDIRPLUS3res_u.readdirplus3ok.readdirplus3_cookieverf_res;
    RETURN_ON_ERROR(reply->xdrDecodeFixedOpaque((unsigned char*)verf, sizeof(cookieverf3)));

    res.READDIRPLUS3res_u.readdirplus3ok.readdirplus3_reply.dirlistplus3_entries = NULL;

    uint32 entryFollow = 0;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&entryFollow));
    int i = 0;
    entryplus3* lastEntry = NULL;
    m_decodedStringBuffer = new Buffer(reply->getCapacity());
    if (m_decodedStringBuffer == NULL)
      return -1;

    while (entryFollow)
    {
      entryplus3* newEntry = new entryplus3;
      DECODE_UQUAD(reply, newEntry->entryplus3_fileid);

      char* filename = reinterpret_cast<char*>(m_decodedStringBuffer->end());
      RETURN_ON_ERROR(NfsUtil::decodeStringToBuffer(reply, m_decodedStringBuffer));

      newEntry->entryplus3_name = filename;

      DECODE_UQUAD(reply, newEntry->entryplus3_cookie);;
      RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, &newEntry->entryplus3_name_attributes));
      RETURN_ON_ERROR(NfsUtil::decodePostOpFH3(reply, &newEntry->entryplus3_name_handle));

      newEntry->entryplus3_nextentry = NULL;
      ++i;
      RETURN_ON_ERROR(reply->xdrDecodeUint32(&entryFollow));

      if (lastEntry == NULL)
      {
        lastEntry = newEntry;
        res.READDIRPLUS3res_u.readdirplus3ok.readdirplus3_reply.dirlistplus3_entries = newEntry;
      }
      else
      {
        lastEntry->entryplus3_nextentry = newEntry;
      }
      lastEntry = newEntry;
    }

    uint32 listEOF = 0;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&listEOF));
    res.READDIRPLUS3res_u.readdirplus3ok.readdirplus3_reply.dirlistplus3_eof  = listEOF;
    syslog(LOG_DEBUG, "Readdirplus got %d entries, EOF = %d\n", i, listEOF);
    return 0;
  }
  return -1;
}

// fsstat call
FsstatCall::FsstatCall(const FSSTAT3args&  arg)
 :RemoteCall(NFS, NFS_V3_FSSTAT),args(arg), res()
{
}

FsstatCall::~FsstatCall()
{
}

int FsstatCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.fsstat3_fsroot;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    return 0;
  }
  return -1;
}

int FsstatCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    struct post_op_attr* postAttr = &res.FSSTAT3res_u.fsstat3ok.fsstat3_obj_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));

#define DECODE_FSSTAT(VAR) DECODE_UQUAD(reply, res.FSSTAT3res_u.fsstat3ok.fsstat3_fsstat3.fsstat3_##VAR);
    DECODE_FSSTAT(tbytes);
    DECODE_FSSTAT(fbytes);
    DECODE_FSSTAT(abytes);
    DECODE_FSSTAT(tfiles);
    DECODE_FSSTAT(ffiles);
    DECODE_FSSTAT(afiles);
#undef DECODE_FSSTAT
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&res.FSSTAT3res_u.fsstat3ok.fsstat3_invarsec));

    return 0;
  }
  else
  {
    struct post_op_attr* postAttr = &res.FSSTAT3res_u.fsstat3fail.fsstat3fail_obj_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    return 0;
  }
  return -1;
}

// fsinfo call
FsinfoCall::FsinfoCall(const FSINFO3args&  arg)
 :RemoteCall(NFS, NFS_V3_FSINFO),args(arg), res()
{
}

FsinfoCall::~FsinfoCall()
{
}

int FsinfoCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.fsinfo3_fsroot;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    return 0;
  }
  return -1;
}

int FsinfoCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    struct post_op_attr* postAttr = &res.FSINFO3res_u.fsinfo3ok.fsinfo3_obj_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));

#define FSINFO_RESULT res.FSINFO3res_u.fsinfo3ok.fsinfo3_fsinfo3
#define DECODE_FSINFO(VAR) RETURN_ON_ERROR(reply->xdrDecodeUint32(&res.FSINFO3res_u.fsinfo3ok.fsinfo3_fsinfo3.fsinfo3_##VAR))
    DECODE_FSINFO(rtmax);
    DECODE_FSINFO(rtpref);
    DECODE_FSINFO(rtmult);
    DECODE_FSINFO(wtmax);
    DECODE_FSINFO(wtpref);
    DECODE_FSINFO(wtmult);
    DECODE_FSINFO(dtpref);
    DECODE_UQUAD(reply, res.FSINFO3res_u.fsinfo3ok.fsinfo3_fsinfo3.fsinfo3_maxfilesize);;
    DECODE_FSINFO(time_delta.time3_seconds);
    DECODE_FSINFO(time_delta.time3_nseconds);
    DECODE_FSINFO(properties);
#undef DECODE_FSINFO
#undef FSINFO_RESULT
    return 0;
  }
  else
  {
    struct post_op_attr* postAttr = &res.FSINFO3res_u.fsinfo3fail.fsinfo3fail_obj_attributes;
    RETURN_ON_ERROR(NfsUtil::decodePostOpAttr(reply, postAttr));
    return 0;
  }
  return -1;
}

// commit call
CommitCall::CommitCall(const COMMIT3args&  arg)
 :RemoteCall(NFS, NFS_V3_COMMIT),args(arg), res()
{
}

CommitCall::~CommitCall()
{
}

int CommitCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    nfs_fh3* fh = &args.commit3_file;
    RETURN_ON_ERROR(request->xdrEncodeVarOpaque(fh->fh3_data.fh3_data_val, fh->fh3_data.fh3_data_len));
    RETURN_ON_ERROR(request->xdrEncodeUint64((args.commit3_offset)));
    RETURN_ON_ERROR(request->xdrEncodeUint32((args.commit3_count)));
    return 0;
  }
  return -1;
}

int CommitCall::decodeResults()
{
  DECODE_STATUS();
  if (status == 0)
  {
    struct wcc_data* wccData = &res.COMMIT3res_u.commit3ok.commit3_file_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    RETURN_ON_ERROR(DECODEVAR(reply, res.COMMIT3res_u.commit3ok.commit3_verf));
    return 0;
  }
  else
  {
    struct wcc_data* wccData = &res.COMMIT3res_u.commit3fail.commit3fail_file_wcc;
    RETURN_ON_ERROR(NfsUtil::decodeWccData(reply, wccData));
    return 0;
  }
  return -1;
}

} // namespace NFSv3
} // namespace OpenNfsC
