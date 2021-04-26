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

#include "Nfs4Call.h"
#include "RpcPacket.h"
#include "RpcConnection.h"
#include "NfsUtil.h"
#include "RpcDefs.h"

#include <iostream>

namespace OpenNfsC {
namespace NFSv4 {

#define DECODE_STATUS()                               \
                                                      \
    uint32 status;                                    \
    RpcPacketPtr reply = getReply();                  \
    if (reply == NULL)                                \
        return -1;                                    \
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&status)); \
    res.status = (nfsstat3)status;


COMPOUNDCall::COMPOUNDCall():
  RemoteCall(NFS, NFSPROC4_COMPOUND),args(),res()
{
  args.minorversion = 0;
  args.argarray.argarray_len = 0;
  args.argarray.argarray_val = NULL;
}

COMPOUNDCall::COMPOUNDCall(const COMPOUND4args& arg):
  RemoteCall(NFS, NFSPROC4_COMPOUND),args(arg),res()
{
  args.minorversion = 0;
}

void COMPOUNDCall::clear()
{
  clearArgs();
  clearRes();
}

void COMPOUNDCall::clearArgs()
{
  // TODO sarat - free the tag
  args.minorversion = 0;
  nfs_argop4 *arg = args.argarray.argarray_val;
  for (unsigned i = 0; i < args.argarray.argarray_len; i++)
  {
    freeArg(arg);
    arg++;
  }
  free(args.argarray.argarray_val);
  args.argarray.argarray_val = NULL;
  args.argarray.argarray_len = 0;
}

void COMPOUNDCall::clearRes()
{
  // TODO sarat - free the tag
  res.status = (nfsstat4)0;
  nfs_resop4 *r = res.resarray.resarray_val;
  for (unsigned i = 0; i < res.resarray.resarray_len; i++)
  {
    freeRes(r);
    r++;
  }
  free(res.resarray.resarray_val);
  res.resarray.resarray_val = NULL;
  res.resarray.resarray_len = 0;
}

void COMPOUNDCall::freeArg(nfs_argop4 *arg)
{
  int opcode = arg->argop;
  arg->argop = (nfs_opnum4)1; // Reset to COMPOUNDCALL
  if (opcode ==  OP_ACCESS)
  {
  }
}

void COMPOUNDCall::freeRes(nfs_resop4 *res)
{
  int opcode = res->resop;
  res->resop = (nfs_opnum4)1; // Reset to COMPOUNDCALL
  if (opcode ==  OP_ACCESS)
  {
  }
}

int
COMPOUNDCall::appendCommand(const nfs_argop4 *cmd)
{
  args.argarray.argarray_val = (nfs_argop4*)realloc(args.argarray.argarray_val,
                                                    (args.argarray.argarray_len + 1 ) * sizeof(nfs_argop4));
  if (args.argarray.argarray_val == NULL)
    throw std::string("Failed to allocate memory COMPOUNDCall::appendCommand");

  nfs_argop4 *in = args.argarray.argarray_val;
  if (args.argarray.argarray_len > 0)
  {
    for (unsigned i = 0; i < (args.argarray.argarray_len); i++)
      in++;
  }
  memcpy(in, cmd, sizeof(nfs_argop4));
  args.argarray.argarray_len++;
  return 0;
}

COMPOUNDCall::~COMPOUNDCall()
{
  clear();
}

int
COMPOUNDCall::encode_OP_ACCESS(RpcPacketPtr request, const ACCESS4args *arg)
{
  RETURN_ON_ERROR(request->xdrEncodeUint32(arg->access));
  return 0;
}

int
COMPOUNDCall::decode_OP_ACCESS(RpcPacketPtr packet, ACCESS4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  uint32_t supported=0;
  uint32_t access=0;

  RETURN_ON_ERROR(packet->xdrDecodeUint32(&supported));
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&access));

  res->ACCESS4res_u.resok4.supported=supported;
  res->ACCESS4res_u.resok4.access=access;

  return 0;
}

int
COMPOUNDCall::encode_OP_CLOSE(RpcPacketPtr packet, const CLOSE4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->seqid));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->open_stateid.seqid));
  RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((void*)arg->open_stateid.other, 12));
  return 0;
}

int
COMPOUNDCall::decode_OP_CLOSE(RpcPacketPtr packet, CLOSE4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  uint32 seqid = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&seqid));
  res->CLOSE4res_u.open_stateid.seqid = seqid;
  RETURN_ON_ERROR(packet->xdrDecodeFixedOpaque((unsigned char*)res->CLOSE4res_u.open_stateid.other, 12));

  return 0;
}

int
COMPOUNDCall::encode_OP_COMMIT(RpcPacketPtr packet, const COMMIT4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->offset));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->count));
  return 0;
}

int
COMPOUNDCall::decode_OP_COMMIT(RpcPacketPtr packet, COMMIT4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  verifier4 cookieverf;
  RETURN_ON_ERROR(packet->xdrDecodeFixedOpaque((unsigned char*)&cookieverf, NFS4_VERIFIER_SIZE));
  memcpy(res->COMMIT4res_u.resok4.writeverf, cookieverf, NFS4_VERIFIER_SIZE);

  return 0;
}

int
COMPOUNDCall::encode_OP_CREATE(RpcPacketPtr packet, const CREATE4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->objtype.type));
  //TODO sarat - linkdata and devdata are to be used ??
  RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)arg->objname.utf8string_val, arg->objname.utf8string_len ));

  RETURN_ON_ERROR(NfsUtil::encode_fattr4(packet, &(arg->createattrs)));

  return 0;
}

int
COMPOUNDCall::decode_OP_CREATE(RpcPacketPtr packet, CREATE4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  uint32 atmc = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&atmc));
  res->CREATE4res_u.resok4.cinfo.atomic = atmc;

  uint64 val = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&val));
  res->CREATE4res_u.resok4.cinfo.before = val;
  val = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&val));
  res->CREATE4res_u.resok4.cinfo.after = val;

  uint32 bitmapLen = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&bitmapLen));
  res->CREATE4res_u.resok4.attrset.bitmap4_len = bitmapLen;

  res->CREATE4res_u.resok4.attrset.bitmap4_val = (uint32*)malloc(bitmapLen * sizeof(uint32));
  if (res->CREATE4res_u.resok4.attrset.bitmap4_val == NULL)
    return -1;

  for (unsigned i = 0; i < bitmapLen; i++)
  {
    uint32 mask = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&mask));
    res->CREATE4res_u.resok4.attrset.bitmap4_val[i] = mask;
  }

  return 0;
}

int
COMPOUNDCall::encode_OP_DELEGPURGE(RpcPacketPtr packet, const DELEGPURGE4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint64((uint64)arg->clientid));
  return 0;
}

int
COMPOUNDCall::decode_OP_DELEGPURGE(RpcPacketPtr packet, DELEGPURGE4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_DELEGRETURN(RpcPacketPtr packet, const DELEGRETURN4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->deleg_stateid.seqid));
  RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((void*)arg->deleg_stateid.other, 12));
  return 0;
}

int
COMPOUNDCall::decode_OP_DELEGRETURN(RpcPacketPtr packet, DELEGRETURN4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_GETATTR(RpcPacketPtr request, const GETATTR4args *arg)
{
  RETURN_ON_ERROR(request->xdrEncodeUint32(arg->attr_request.bitmap4_len));
  uint32_t *mask = arg->attr_request.bitmap4_val;
  for (unsigned i = 0; i < arg->attr_request.bitmap4_len; i++)
  {
    RETURN_ON_ERROR(request->xdrEncodeUint32(*mask));
    mask++;
  }
  return 0;
}

int
COMPOUNDCall::decode_OP_GETATTR(RpcPacketPtr packet, GETATTR4res *res)
{
  uint32 opSts;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  uint32 bitMapLength = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&bitMapLength));

  bitmap4 *bitmap = &(res->GETATTR4res_u.resok4.obj_attributes.attrmask);
  attrlist4 *attrList = &(res->GETATTR4res_u.resok4.obj_attributes.attr_vals);

  bitmap->bitmap4_len = bitMapLength;
  bitmap->bitmap4_val = (uint32_t *)malloc(bitMapLength * sizeof(uint32));
  if (bitmap->bitmap4_val == NULL)
    return -1;

  for (unsigned i = 0; i < bitMapLength; i++)
  {
    uint32 mask = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&mask));
    bitmap->bitmap4_val[i] = mask;
  }

  uint32 attrLength = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&attrLength));
  attrList->attrlist4_len = attrLength;

  if (packet->getUnparsedDataSize() < attrLength)
    return -1;

  attrList->attrlist4_val = (char *) malloc(attrLength);
  if (attrList->attrlist4_val == NULL)
    return -1;

  RETURN_ON_ERROR(packet->xdrGetRawData((unsigned char *)attrList->attrlist4_val, attrLength));

  return 0;
}

int
COMPOUNDCall::encode_OP_GETFH(RpcPacketPtr packet)
{
  return 0;
}

int
COMPOUNDCall::decode_OP_GETFH(RpcPacketPtr packet, GETFH4res *res)
{
  uint32 status;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&status));

  res->status = (nfsstat4)status;
  if (status != NFS4_OK)
    return 0;

  unsigned char* str = NULL;
  uint32 len = 0;
  RETURN_ON_ERROR(packet->xdrDecodeString(str, len, true));
  res->GETFH4res_u.resok4.object.nfs_fh4_len = len;
  res->GETFH4res_u.resok4.object.nfs_fh4_val = (char*)str;
  return 0;
}

int
COMPOUNDCall::encode_OP_LINK(RpcPacketPtr packet, const LINK4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)arg->newname.utf8string_val,
                                          arg->newname.utf8string_len));
  return 0;
}

int
COMPOUNDCall::decode_OP_LINK(RpcPacketPtr packet, LINK4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  bool_t sato;
  RETURN_ON_ERROR(packet->xdrDecodeUint32((unsigned int*)&sato));
  res->LINK4res_u.resok4.cinfo.atomic = sato;

  uint64 sbefo;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&sbefo));
  res->LINK4res_u.resok4.cinfo.before = sbefo;

  uint64 safte;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&safte));
  res->LINK4res_u.resok4.cinfo.after = safte;

  return 0;
}

int
COMPOUNDCall::encode_OP_LOCK(RpcPacketPtr packet, const LOCK4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->locktype));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->reclaim));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->offset));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->length));

  // encode the lock owner
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->locker.new_lock_owner));
  if (arg->locker.new_lock_owner)
  {
    // encode the state id and sequence id
    RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->locker.locker4_u.open_owner.open_seqid));
    RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->locker.locker4_u.open_owner.open_stateid.seqid));
    RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((void*)arg->locker.locker4_u.open_owner.open_stateid.other, 12));
    RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->locker.locker4_u.open_owner.lock_seqid));
    // encode the owner details
    RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->locker.locker4_u.open_owner.lock_owner.clientid));
    RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char *)arg->locker.locker4_u.open_owner.lock_owner.owner.owner_val,
                                            arg->locker.locker4_u.open_owner.lock_owner.owner.owner_len));
  }
  else
  {
    RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->locker.locker4_u.lock_owner.lock_stateid.seqid));
    RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((void*)arg->locker.locker4_u.lock_owner.lock_stateid.other, 12));
    RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->locker.locker4_u.lock_owner.lock_seqid));
  }

  return 0;
}

int
COMPOUNDCall::decode_OP_LOCK(RpcPacketPtr packet, LOCK4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;

  if (res->status == NFS4_OK)
  {
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&(res->LOCK4res_u.resok4.lock_stateid.seqid)));
    RETURN_ON_ERROR(packet->xdrDecodeFixedOpaque((unsigned char*)res->LOCK4res_u.resok4.lock_stateid.other, 12));
  }
  else if (res->status == NFS4ERR_DENIED)
  {
    uint64 value = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint64(&value));
    res->LOCK4res_u.denied.offset = value;

    value = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint64(&value));
    res->LOCK4res_u.denied.length = value;

    uint32 ltype = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&ltype));
    res->LOCK4res_u.denied.locktype = (nfs_lock_type4) ltype;

    value = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint64(&value));
    res->LOCK4res_u.denied.owner.clientid = value;

    unsigned char* str = NULL;
    uint32 len = 0;
    RETURN_ON_ERROR(packet->xdrDecodeString(str, len, true));
    res->LOCK4res_u.denied.owner.owner.owner_len = len;
    res->LOCK4res_u.denied.owner.owner.owner_val = (char*)str;
  }

  return 0;
}

int
COMPOUNDCall::encode_OP_LOCKT(RpcPacketPtr packet, const LOCKT4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->locktype));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->offset));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->length));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->owner.clientid));
  RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char *)arg->owner.owner.owner_val,
                                          arg->owner.owner.owner_len));
  return 0;
}

int
COMPOUNDCall::decode_OP_LOCKT(RpcPacketPtr packet, LOCKT4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;

  if (res->status == NFS4ERR_DENIED)
  {
    uint64 value = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint64(&value));
    res->LOCKT4res_u.denied.offset = value;

    value = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint64(&value));
    res->LOCKT4res_u.denied.length = value;

    uint32 ltype = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&ltype));
    res->LOCKT4res_u.denied.locktype = (nfs_lock_type4) ltype;

    value = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint64(&value));
    res->LOCKT4res_u.denied.owner.clientid = value;

    unsigned char* str = NULL;
    uint32 len = 0;
    RETURN_ON_ERROR(packet->xdrDecodeString(str, len, true));
    res->LOCKT4res_u.denied.owner.owner.owner_len = len;
    res->LOCKT4res_u.denied.owner.owner.owner_val = (char*)str;
  }

  return 0;
}

int
COMPOUNDCall::encode_OP_LOCKU(RpcPacketPtr packet, const LOCKU4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->locktype));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->seqid));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->lock_stateid.seqid));
  RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((void*)arg->lock_stateid.other, 12));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->offset));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->length));

  return 0;
}

int
COMPOUNDCall::decode_OP_LOCKU(RpcPacketPtr packet, LOCKU4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  RETURN_ON_ERROR(packet->xdrDecodeUint32(&(res->LOCKU4res_u.lock_stateid.seqid)));
  RETURN_ON_ERROR(packet->xdrDecodeFixedOpaque((unsigned char*)res->LOCKU4res_u.lock_stateid.other, 12));

  return 0;
}

int
COMPOUNDCall::encode_OP_LOOKUP(RpcPacketPtr request, const LOOKUP4args *arg)
{
  RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)arg->objname.utf8string_val,
                                           arg->objname.utf8string_len));
  return 0;
}

int
COMPOUNDCall::decode_OP_LOOKUP(RpcPacketPtr packet, LOOKUP4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  return 0;
}

int
COMPOUNDCall::encode_OP_LOOKUPP(RpcPacketPtr packet)
{
  return 0;
}

int
COMPOUNDCall::decode_OP_LOOKUPP(RpcPacketPtr packet, LOOKUPP4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_NVERIFY(RpcPacketPtr packet, const NVERIFY4args *arg)
{
  RETURN_ON_ERROR(NfsUtil::encode_fattr4(packet, &arg->obj_attributes));
  return 0;
}

int
COMPOUNDCall::decode_OP_NVERIFY(RpcPacketPtr packet, NVERIFY4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_OPEN(RpcPacketPtr packet, const OPEN4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->seqid));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->share_access));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->share_deny));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->owner.clientid));

  RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)arg->owner.owner.owner_val,
                                          arg->owner.owner.owner_len));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->openhow.opentype));

  switch(arg->openhow.opentype)
  {
    case OPEN4_CREATE:
    {
      RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->openhow.openflag4_u.how.mode));

      switch (arg->openhow.openflag4_u.how.mode)
      {
        case UNCHECKED4:
        case GUARDED4:
        {
          RETURN_ON_ERROR(NfsUtil::encode_fattr4(packet, &arg->openhow.openflag4_u.how.createhow4_u.createattrs));
        }
        break;
        case EXCLUSIVE4:
        {
          //TODO sarat nfs - exclusive not tested
          RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((void*)arg->openhow.openflag4_u.how.createhow4_u.createverf,
                                                       NFS4_VERIFIER_SIZE));
        }
        break;
        default:
        break;
      }
    }
    break;
    case OPEN4_NOCREATE:
    break;
    default:
    break;
  }

  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->claim.claim));

  switch(arg->claim.claim)
  {
    case CLAIM_NULL:
    {
      if (arg->claim.open_claim4_u.file.utf8string_len != 0)
        RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)arg->claim.open_claim4_u.file.utf8string_val,
                                                arg->claim.open_claim4_u.file.utf8string_len));
    }
    break;
    case CLAIM_PREVIOUS:
    {
#if 0
        //open_delegation_type4   delegate_type;
        RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->claim.open_claim4_u.delegate_type));
#endif
    }
    break;
    case CLAIM_DELEGATE_CUR:
    {
#if 0
      //open_claim_delegate_cur4        delegate_cur_info;
      RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->claim.open_claim4_u.delegate_cur_info.delegate_stateid.seqid));
      RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)arg->claim.open_claim4_u.delegate_cur_info.delegate_stateid.other,
                                              strlen((const char*)arg->claim.open_claim4_u.delegate_cur_info.delegate_stateid.other)));
      RETURN_ON_ERROR(packet->xdrEncodeVarOpaque(arg->claim.open_claim4_u.delegate_cur_info.file.utf8string_val,
                                                 arg->claim.open_claim4_u.delegate_cur_info.file.utf8string_len));
#endif
    }
    break;
    case CLAIM_DELEGATE_PREV:
    {
#if 0
      //component4      file_delegate_prev;
      RETURN_ON_ERROR(packet->xdrEncodeVarOpaque(arg->claim.open_claim4_u.file_delegate_prev.utf8string_val,
                                                 arg->claim.open_claim4_u.file_delegate_prev.utf8string_len));
#endif
    }
    break;
    default:
    break;
  }

  return 0;
}

int
COMPOUNDCall::decode_OP_OPEN(RpcPacketPtr packet, OPEN4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  /* get the state id */
  uint32 seqid = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&seqid));
  res->OPEN4res_u.resok4.stateid.seqid = seqid;

  RETURN_ON_ERROR(packet->xdrDecodeFixedOpaque((unsigned char*)res->OPEN4res_u.resok4.stateid.other, 12));

  /* get the change_info4 */
  bool_t sato;
  RETURN_ON_ERROR(packet->xdrDecodeUint32((uint32 *)&sato));
  res->OPEN4res_u.resok4.cinfo.atomic = sato;

  uint64 sbefo;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&sbefo));
  res->OPEN4res_u.resok4.cinfo.before = sbefo;

  uint64 safte;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&safte));
  res->OPEN4res_u.resok4.cinfo.after = safte;

  /* get the rflags */
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&res->OPEN4res_u.resok4.rflags));

  /* get the attrset */
  bitmap4 *attrset = &(res->OPEN4res_u.resok4.attrset);

  uint32 bitMapLength = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&bitMapLength));
  attrset->bitmap4_len = bitMapLength;

  attrset->bitmap4_val = (uint32_t *)malloc(bitMapLength * sizeof(uint32));
  if (attrset->bitmap4_val == NULL)
    return -1;

  for (unsigned i = 0; i < bitMapLength; i++)
  {
    uint32 mask = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&mask));
    attrset->bitmap4_val[i] = mask;
  }
  // TODO sarat - will there be any actual attribute values following this??

  /* get the delegation type */
  uint32 delegType = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&delegType));
  res->OPEN4res_u.resok4.delegation.delegation_type = (open_delegation_type4)delegType;

  if (delegType == OPEN_DELEGATE_READ)
  {
    //res->OPEN4res_u.resok4.delegation.open_delegation4_u.read
  }
  else if (delegType == OPEN_DELEGATE_WRITE)
  {
    //res->OPEN4res_u.resok4.delegation.open_delegation4_u.write
  }

  return 0;
}

int
COMPOUNDCall::encode_OP_OPENATTR(RpcPacketPtr packet, const OPENATTR4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->createdir));
  return 0;
}

int
COMPOUNDCall::decode_OP_OPENATTR(RpcPacketPtr packet, OPENATTR4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_OPEN_CONFIRM(RpcPacketPtr packet, const OPEN_CONFIRM4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->open_stateid.seqid));
  RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((void*)arg->open_stateid.other, 12));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->seqid));
  return 0;
}


int
COMPOUNDCall::decode_OP_OPEN_CONFIRM(RpcPacketPtr packet, OPEN_CONFIRM4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  uint32 seqid = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&seqid));
  res->OPEN_CONFIRM4res_u.resok4.open_stateid.seqid=seqid;

  RETURN_ON_ERROR(packet->xdrDecodeFixedOpaque((unsigned char*)res->OPEN_CONFIRM4res_u.resok4.open_stateid.other, 12));

  return 0;
}

int
COMPOUNDCall::encode_OP_OPEN_DOWNGRADE(RpcPacketPtr packet, const OPEN_DOWNGRADE4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->open_stateid.seqid));
  RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((void*)arg->open_stateid.other, 12));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->seqid));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->share_access));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->share_deny));

  return 0;
}

int
COMPOUNDCall::decode_OP_OPEN_DOWNGRADE(RpcPacketPtr packet, OPEN_DOWNGRADE4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  uint32 seqid = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&seqid));
  res->OPEN_DOWNGRADE4res_u.resok4.open_stateid.seqid = seqid;

  RETURN_ON_ERROR(packet->xdrDecodeFixedOpaque((unsigned char*)res->OPEN_DOWNGRADE4res_u.resok4.open_stateid.other, 12));

  return 0;
}

int
COMPOUNDCall::encode_OP_PUTFH(RpcPacketPtr packet, const PUTFH4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeVarOpaque(arg->object.nfs_fh4_val, arg->object.nfs_fh4_len));
  return 0;
}

int
COMPOUNDCall::decode_OP_PUTFH(RpcPacketPtr packet, PUTFH4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_PUTPUBFH(RpcPacketPtr packet)
{
  return 0;
}

int
COMPOUNDCall::decode_OP_PUTPUBFH(RpcPacketPtr packet, PUTPUBFH4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_PUTROOTFH(RpcPacketPtr packet)
{
  return 0;
}

int
COMPOUNDCall::decode_OP_PUTROOTFH(RpcPacketPtr packet, PUTROOTFH4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_READ(RpcPacketPtr packet, const READ4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->stateid.seqid));
  RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((char*)arg->stateid.other,12));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->offset));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->count));
  return 0;
}

int
COMPOUNDCall::decode_OP_READ(RpcPacketPtr packet, READ4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  uint32 iseof=false;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&iseof));
  res->READ4res_u.resok4.eof=iseof;

  unsigned char* str = NULL;
  uint32 len = 0;
  RETURN_ON_ERROR(packet->xdrDecodeString(str, len, true));
  res->READ4res_u.resok4.data.data_len = len;
  res->READ4res_u.resok4.data.data_val = (char*)str;

  return 0;
}

int
COMPOUNDCall::encode_OP_READDIR(RpcPacketPtr packet, const READDIR4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->cookie));
  RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((void*)arg->cookieverf, NFS4_VERIFIER_SIZE));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->dircount));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->maxcount));

  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->attr_request.bitmap4_len));
  uint32_t *mask = arg->attr_request.bitmap4_val;
  for (unsigned i = 0; i < arg->attr_request.bitmap4_len; i++)
  {
    RETURN_ON_ERROR(packet->xdrEncodeUint32(*mask));
    mask++;
  }

  return 0;
}

int
COMPOUNDCall::decode_OP_READDIR(RpcPacketPtr packet, READDIR4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  verifier4 cookieverf;
  RETURN_ON_ERROR(packet->xdrDecodeFixedOpaque((unsigned char*)&cookieverf,NFS4_VERIFIER_SIZE));
  memcpy(res->READDIR4res_u.resok4.cookieverf, cookieverf, NFS4_VERIFIER_SIZE);

  dirlist4 *list = &(res->READDIR4res_u.resok4.reply);

  uint32 valueFollows = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&valueFollows));
  if (valueFollows == 0)
  {
    // empty directory case
    uint32 eof = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&eof));
    list->eof = eof;
    if (list->eof == 1)
      list->entries = NULL;
    return 0;
  }

  // create place for first entry
  list->entries = (entry4 *) malloc(sizeof(entry4));
  if (list->entries == NULL)
    return -1;
  entry4 *current = list->entries;
  current->nextentry = NULL;

  do
  {
    // get the cookie
    uint64 cookie = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint64(&cookie));
    current->cookie = (nfs_cookie4) cookie;

    unsigned char* str = NULL;
    u_int len = 0;
    RETURN_ON_ERROR(packet->xdrDecodeString(str, len, true));
    current->name.utf8string_val = (char*)str;
    current->name.utf8string_len = len;

    // fill the attrs
    bitmap4 *bitmap = &(current->attrs.attrmask);
    attrlist4 *attrList = &(current->attrs.attr_vals);

    uint32 bitMapLength = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&bitMapLength));

    // get the bitmap masks
    bitmap->bitmap4_len = bitMapLength;
    bitmap->bitmap4_val = (uint32_t *)malloc(bitMapLength * sizeof(uint32));
    if (bitmap->bitmap4_val == NULL)
      return -1;

    for (unsigned i = 0; i < bitMapLength; i++)
    {
      uint32 mask = 0;
      RETURN_ON_ERROR(packet->xdrDecodeUint32(&mask));
      bitmap->bitmap4_val[i] = mask;
    }

    // get the attrs as raw data un-decoded
    uint32 attrLength = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&attrLength));
    attrList->attrlist4_len = attrLength;

    attrList->attrlist4_val = (char *) malloc(attrLength);
    if (attrList->attrlist4_val == NULL)
      return -1;

    RETURN_ON_ERROR(packet->xdrGetRawData((unsigned char *)attrList->attrlist4_val, attrLength));

    RETURN_ON_ERROR(packet->xdrDecodeUint32(&valueFollows));
    if (valueFollows == 1)
    {
      entry4 *nextEntry = (entry4 *)malloc(sizeof(entry4));
      if (nextEntry == NULL)
        return -1;
      nextEntry->nextentry = NULL;
      current->nextentry = nextEntry;
      current = nextEntry;
    }
    else
    {
      uint32 eof = 0;
      RETURN_ON_ERROR(packet->xdrDecodeUint32(&eof));
      list->eof = eof;
    }
  } while (valueFollows == 1);

  return 0;
}

int
COMPOUNDCall::encode_OP_READLINK(RpcPacketPtr packet)
{
  return 0;
}

int
COMPOUNDCall::decode_OP_READLINK(RpcPacketPtr packet, READLINK4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  unsigned char* str = NULL;
  u_int len = 0;
  RETURN_ON_ERROR(packet->xdrDecodeString(str, len, true));

  res->READLINK4res_u.resok4.link.utf8string_len = len;
  res->READLINK4res_u.resok4.link.utf8string_val = (char*)str;

  return 0;
}

int
COMPOUNDCall::encode_OP_REMOVE(RpcPacketPtr packet, const REMOVE4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)arg->target.utf8string_val, arg->target.utf8string_len));
  return 0;
}

int
COMPOUNDCall::decode_OP_REMOVE(RpcPacketPtr packet, REMOVE4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  bool_t sato;
  RETURN_ON_ERROR(packet->xdrDecodeUint32((unsigned int*)&sato));
  res->REMOVE4res_u.resok4.cinfo.atomic = sato;

  uint64 sbefo;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&sbefo));
  res->REMOVE4res_u.resok4.cinfo.before = sbefo;

  uint64 safte;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&safte));
  res->REMOVE4res_u.resok4.cinfo.after = safte;

  return 0;
}

int
COMPOUNDCall::encode_OP_RENAME(RpcPacketPtr packet, const RENAME4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)arg->oldname.utf8string_val, arg->oldname.utf8string_len));
  RETURN_ON_ERROR(packet->xdrEncodeString((unsigned char*)arg->newname.utf8string_val, arg->newname.utf8string_len));

  return 0;
}

int
COMPOUNDCall::decode_OP_RENAME(RpcPacketPtr packet, RENAME4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  bool_t sato;
  RETURN_ON_ERROR(packet->xdrDecodeUint32((unsigned int*)&sato));
  res->RENAME4res_u.resok4.source_cinfo.atomic = sato;

  uint64 sbefo;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&sbefo));
  res->RENAME4res_u.resok4.source_cinfo.before = sbefo;

  uint64 safte;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&safte));
  res->RENAME4res_u.resok4.source_cinfo.after = safte;

  bool_t tato;
  RETURN_ON_ERROR(packet->xdrDecodeUint32((unsigned int*)&tato));
  res->RENAME4res_u.resok4.target_cinfo.atomic = tato;

  uint64 tbefo;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&tbefo));
  res->RENAME4res_u.resok4.target_cinfo.before = tbefo;

  uint64 tafte;
  RETURN_ON_ERROR(packet->xdrDecodeUint64(&tafte));
  res->RENAME4res_u.resok4.target_cinfo.after = tafte;

  return 0;
}

int
COMPOUNDCall::encode_OP_RENEW(RpcPacketPtr packet, const RENEW4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->clientid));
  return 0;
}

int
COMPOUNDCall::decode_OP_RENEW(RpcPacketPtr packet, RENEW4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_RESTOREFH(RpcPacketPtr packet)
{
  return 0;
}

int
COMPOUNDCall::decode_OP_RESTOREFH(RpcPacketPtr packet, RESTOREFH4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_SAVEFH(RpcPacketPtr packet)
{
  return 0;
}

int
COMPOUNDCall::decode_OP_SAVEFH(RpcPacketPtr packet, SAVEFH4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_SECINFO(RpcPacketPtr packet)
{
  return 0;
}

int
COMPOUNDCall::decode_OP_SECINFO(RpcPacketPtr packet)
{
  return 0;
}

int
COMPOUNDCall::encode_OP_SETATTR(RpcPacketPtr packet, const SETATTR4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->stateid.seqid));
  RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((char*)arg->stateid.other, 12));
  RETURN_ON_ERROR(NfsUtil::encode_fattr4(packet, &arg->obj_attributes));

  return 0;
}

int
COMPOUNDCall::decode_OP_SETATTR(RpcPacketPtr packet, SETATTR4res *res)
{
  uint32 opSts;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;

  uint32 bitMapLength = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&bitMapLength));
  res->attrsset.bitmap4_len = bitMapLength;

  res->attrsset.bitmap4_val = (uint32_t*)malloc(bitMapLength * sizeof(uint32));
  if (res->attrsset.bitmap4_val == NULL)
    return -1;

  uint32 *ptr = res->attrsset.bitmap4_val;
  for (unsigned i = 0; i < bitMapLength; i++)
  {
    uint32 mask = 0;
    RETURN_ON_ERROR(packet->xdrDecodeUint32(&mask));
    ptr[i] = mask;
  }

  return 0;
}

int
COMPOUNDCall::encode_OP_SETCLIENTID(RpcPacketPtr     request,
                                    SETCLIENTID4args *arg)
{
  RETURN_ON_ERROR(request->xdrEncodeFixedOpaque(arg->client.verifier, NFS4_VERIFIER_SIZE));
  RETURN_ON_ERROR(request->xdrEncodeVarOpaque(arg->client.id.id_val, arg->client.id.id_len));
  RETURN_ON_ERROR(request->xdrEncodeUint32(arg->callback.cb_program));
  RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)arg->callback.cb_location.r_netid,
                                           strlen(arg->callback.cb_location.r_netid)));
  RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)arg->callback.cb_location.r_addr,
                                           strlen(arg->callback.cb_location.r_addr)));
  RETURN_ON_ERROR(request->xdrEncodeUint32(arg->callback_ident));
  return 0;
}

int
COMPOUNDCall::decode_OP_SETCLIENTID(RpcPacketPtr reply, SETCLIENTID4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(reply->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;

  if (opSts == NFS4_OK)
  {
    uint64 clientid = 0;
    RETURN_ON_ERROR(reply->xdrDecodeUint64(&clientid));
    res->SETCLIENTID4res_u.resok4.clientid = clientid;

    verifier4 verifierConfirm;
    RETURN_ON_ERROR(reply->xdrDecodeFixedOpaque((unsigned char*)&verifierConfirm,
                                                NFS4_VERIFIER_SIZE));
    memcpy(res->SETCLIENTID4res_u.resok4.setclientid_confirm,
           verifierConfirm,
           NFS4_VERIFIER_SIZE);
  }
  else if (opSts == NFS4ERR_CLID_INUSE)
  {
    //TODO sarat nfs - sectio not tested
    unsigned char* r_netid = NULL;
    uint32 r_netid_len;
    RETURN_ON_ERROR(reply->xdrDecodeString(r_netid, r_netid_len));
    unsigned char* r_addr = NULL;
    uint32 r_addr_len;
    RETURN_ON_ERROR(reply->xdrDecodeString(r_addr, r_addr_len));
    res->SETCLIENTID4res_u.client_using.r_netid = (char*)r_netid;
    res->SETCLIENTID4res_u.client_using.r_addr = (char*)r_addr;
  }

  return 0;
}

int
COMPOUNDCall::encode_OP_SETCLIENTID_CONFIRM(RpcPacketPtr             request,
                                            SETCLIENTID_CONFIRM4args *arg)
{
  RETURN_ON_ERROR(request->xdrEncodeUint64((uint64)arg->clientid));
  RETURN_ON_ERROR(request->xdrEncodeFixedOpaque(arg->setclientid_confirm, NFS4_VERIFIER_SIZE));
  return 0;
}

int
COMPOUNDCall::decode_OP_SETCLIENTID_CONFIRM(RpcPacketPtr request, SETCLIENTID_CONFIRM4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(request->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_VERIFY(RpcPacketPtr packet, const VERIFY4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->obj_attributes.attrmask.bitmap4_len));
  uint32_t *mask = arg->obj_attributes.attrmask.bitmap4_val;
  for (unsigned i = 0; i < arg->obj_attributes.attrmask.bitmap4_len; i++)
  {
    RETURN_ON_ERROR(packet->xdrEncodeUint32(*mask));
    mask++;
  }

  RETURN_ON_ERROR(NfsUtil::encode_fattr4(packet, &arg->obj_attributes));

  return 0;
}

int
COMPOUNDCall::decode_OP_VERIFY(RpcPacketPtr packet, VERIFY4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}

int
COMPOUNDCall::encode_OP_WRITE(RpcPacketPtr packet, const WRITE4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->stateid.seqid));
  RETURN_ON_ERROR(packet->xdrEncodeFixedOpaque((char*)arg->stateid.other, 12));
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->offset));
  RETURN_ON_ERROR(packet->xdrEncodeUint32(arg->stable));
  RETURN_ON_ERROR(packet->xdrEncodeVarOpaque(arg->data.data_val, arg->data.data_len));
  return 0;
}

int
COMPOUNDCall::decode_OP_WRITE(RpcPacketPtr packet, WRITE4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));

  res->status = (nfsstat4)opSts;
  if (opSts != NFS4_OK)
    return 0;

  uint32 count = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&count));
  res->WRITE4res_u.resok4.count = count;

  uint32 stable = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&stable));
  res->WRITE4res_u.resok4.committed = (stable_how4)stable;

  verifier4 verfierwrite;
  RETURN_ON_ERROR(packet->xdrDecodeFixedOpaque((unsigned char*)&verfierwrite,
                                               NFS4_VERIFIER_SIZE));
  memcpy(res->WRITE4res_u.resok4.writeverf, verfierwrite,NFS4_VERIFIER_SIZE);

  return 0;
}

int
COMPOUNDCall::encode_OP_RELEASE_LOCKOWNER(RpcPacketPtr packet, const RELEASE_LOCKOWNER4args *arg)
{
  RETURN_ON_ERROR(packet->xdrEncodeUint64(arg->lock_owner.clientid));
  RETURN_ON_ERROR(packet->xdrEncodeVarOpaque(arg->lock_owner.owner.owner_val,
                                             arg->lock_owner.owner.owner_len));
  return 0;
}

int
COMPOUNDCall::decode_OP_RELEASE_LOCKOWNER(RpcPacketPtr packet, RELEASE_LOCKOWNER4res *res)
{
  uint32 opSts = 0;
  RETURN_ON_ERROR(packet->xdrDecodeUint32(&opSts));
  res->status = (nfsstat4)opSts;
  return 0;
}


/*
 NOTE:-
 Whenever anything is encoded by RpcPacket the write index aka m_writeIndex
 moves to the proper position, so the same RpcPacket can be used to encode
 multiple commands. they will just be appended at the end.

 Similarly during decoding the read index aka m_readIndex is advanced accodringly.
 So if we give the same RpcPacket can be used to keep decoding multiple commands.
*/
int COMPOUNDCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    // set a empty tag
    RETURN_ON_ERROR(request->xdrEncodeString((unsigned char*)NULL, 0));
    // minor version is 0
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.minorversion));
    // op count
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.argarray.argarray_len));

    // Now encode each command in the compound array to request buffer
    nfs_argop4 *cmdItem = args.argarray.argarray_val;
    for (unsigned i = 0; i < args.argarray.argarray_len; i++)
    {
      // op code
      RETURN_ON_ERROR(request->xdrEncodeUint32(cmdItem->argop));
      switch (cmdItem->argop)
      {
        case OP_ACCESS:
        {
          ACCESS4args *args = &(cmdItem->nfs_argop4_u.opaccess);
          RETURN_ON_ERROR(encode_OP_ACCESS(request, args));
        }
        break;
        case OP_CLOSE:
        {
          CLOSE4args *args = &(cmdItem->nfs_argop4_u.opclose);
          RETURN_ON_ERROR(encode_OP_CLOSE(request, args));
        }
        break;
        case OP_COMMIT:
        {
          COMMIT4args *args = &(cmdItem->nfs_argop4_u.opcommit);
          RETURN_ON_ERROR(encode_OP_COMMIT(request, args));
        }
        break;
        case OP_CREATE:
        {
          CREATE4args *args = &(cmdItem->nfs_argop4_u.opcreate);
          RETURN_ON_ERROR(encode_OP_CREATE(request, args));
        }
        break;
        case OP_DELEGPURGE:
        {
          DELEGPURGE4args *args = &(cmdItem->nfs_argop4_u.opdelegpurge);
          RETURN_ON_ERROR(encode_OP_DELEGPURGE(request, args));
        }
        break;
        case OP_DELEGRETURN:
        {
          DELEGRETURN4args *args = &(cmdItem->nfs_argop4_u.opdelegreturn);
          RETURN_ON_ERROR(encode_OP_DELEGRETURN(request, args));
        }
        break;
        case OP_GETATTR:
        {
          GETATTR4args *args = &(cmdItem->nfs_argop4_u.opgetattr);
          RETURN_ON_ERROR(encode_OP_GETATTR(request, args));
        }
        break;
        case OP_GETFH:
        {
          RETURN_ON_ERROR(encode_OP_GETFH(request));
        }
        break;
        case OP_LINK:
        {
          LINK4args *args = &(cmdItem->nfs_argop4_u.oplink);
          RETURN_ON_ERROR(encode_OP_LINK(request, args));
        }
        break;
        case OP_LOCK:
        {
          LOCK4args *args = &(cmdItem->nfs_argop4_u.oplock);
          RETURN_ON_ERROR(encode_OP_LOCK(request, args));
        }
        break;
        case OP_LOCKT:
        {
          LOCKT4args *args = &(cmdItem->nfs_argop4_u.oplockt);
          RETURN_ON_ERROR(encode_OP_LOCKT(request, args));
        }
        break;
        case OP_LOCKU:
        {
          LOCKU4args *args = &(cmdItem->nfs_argop4_u.oplocku);
          RETURN_ON_ERROR(encode_OP_LOCKU(request, args));
        }
        break;
        case OP_LOOKUP:
        {
          LOOKUP4args *args = &(cmdItem->nfs_argop4_u.oplookup);
          RETURN_ON_ERROR(encode_OP_LOOKUP(request, args));
        }
        break;
        case OP_LOOKUPP:
        {
          RETURN_ON_ERROR(encode_OP_LOOKUPP(request));
        }
        break;
        case OP_NVERIFY:
        {
          NVERIFY4args *args = &(cmdItem->nfs_argop4_u.opnverify);
          RETURN_ON_ERROR(encode_OP_NVERIFY(request, args));
        }
        break;
        case OP_OPEN:
        {
          OPEN4args *args = &(cmdItem->nfs_argop4_u.opopen);
          RETURN_ON_ERROR(encode_OP_OPEN(request, args));
        }
        break;
        case OP_OPENATTR:
        {
          OPENATTR4args *args = &(cmdItem->nfs_argop4_u.opopenattr);
          RETURN_ON_ERROR(encode_OP_OPENATTR(request, args));
        }
        break;
        case OP_OPEN_CONFIRM:
        {
          OPEN_CONFIRM4args *args = &(cmdItem->nfs_argop4_u.opopen_confirm);
          RETURN_ON_ERROR(encode_OP_OPEN_CONFIRM(request, args));
        }
        break;
        case OP_OPEN_DOWNGRADE:
        {
          OPEN_DOWNGRADE4args *args = &(cmdItem->nfs_argop4_u.opopen_downgrade);
          RETURN_ON_ERROR(encode_OP_OPEN_DOWNGRADE(request, args));
        }
        break;
        case OP_PUTFH:
        {
          PUTFH4args *args = &(cmdItem->nfs_argop4_u.opputfh);
          RETURN_ON_ERROR(encode_OP_PUTFH(request, args));
        }
        break;
        case OP_PUTPUBFH:
        {
          RETURN_ON_ERROR(encode_OP_PUTPUBFH(request));
        }
        break;
        case OP_PUTROOTFH:
        {
          RETURN_ON_ERROR(encode_OP_PUTROOTFH(request));
        }
        break;
        case OP_READ:
        {
          READ4args *args = &(cmdItem->nfs_argop4_u.opread);
          RETURN_ON_ERROR(encode_OP_READ(request, args));
        }
        break;
        case OP_READDIR:
        {
          READDIR4args *args = &(cmdItem->nfs_argop4_u.opreaddir);
          RETURN_ON_ERROR(encode_OP_READDIR(request, args));
        }
        break;
        case OP_READLINK:
        {
          RETURN_ON_ERROR(encode_OP_READLINK(request));
        }
        break;
        case OP_REMOVE:
        {
          REMOVE4args *args = &(cmdItem->nfs_argop4_u.opremove);
          RETURN_ON_ERROR(encode_OP_REMOVE(request, args));
        }
        break;
        case OP_RENAME:
        {
          RENAME4args *args = &(cmdItem->nfs_argop4_u.oprename);
          RETURN_ON_ERROR(encode_OP_RENAME(request, args));
        }
        break;
        case OP_RENEW:
        {
          RENEW4args *args = &(cmdItem->nfs_argop4_u.oprenew);
          RETURN_ON_ERROR(encode_OP_RENEW(request, args));
        }
        break;
        case OP_RESTOREFH:
        {
          RETURN_ON_ERROR(encode_OP_RESTOREFH(request));
        }
        break;
        case OP_SAVEFH:
        {
          RETURN_ON_ERROR(encode_OP_SAVEFH(request));
        }
        break;
        case OP_SECINFO:
        {
          RETURN_ON_ERROR(encode_OP_SECINFO(request));
        }
        break;
        case OP_SETATTR:
        {
          SETATTR4args *args = &(cmdItem->nfs_argop4_u.opsetattr);
          RETURN_ON_ERROR(encode_OP_SETATTR(request, args));
        }
        break;
        case OP_SETCLIENTID:
        {
          SETCLIENTID4args *args = &(cmdItem->nfs_argop4_u.opsetclientid);
          RETURN_ON_ERROR(encode_OP_SETCLIENTID(request, args));
        }
        break;
        case OP_SETCLIENTID_CONFIRM:
        {
          SETCLIENTID_CONFIRM4args *args = &(cmdItem->nfs_argop4_u.opsetclientid_confirm);
          RETURN_ON_ERROR(encode_OP_SETCLIENTID_CONFIRM(request, args));
        }
        break;
        case OP_VERIFY:
        {
          VERIFY4args *args = &(cmdItem->nfs_argop4_u.opverify);
          RETURN_ON_ERROR(encode_OP_VERIFY(request, args));
        }
        break;
        case OP_WRITE:
        {
          WRITE4args *args = &(cmdItem->nfs_argop4_u.opwrite);
          RETURN_ON_ERROR(encode_OP_WRITE(request, args));
        }
        break;
        case OP_RELEASE_LOCKOWNER:
        {
          RELEASE_LOCKOWNER4args *args = &(cmdItem->nfs_argop4_u.oprelease_lockowner);
          RETURN_ON_ERROR(encode_OP_RELEASE_LOCKOWNER(request, args));
        }
        break;
        default:
        break;
      }
      cmdItem++;
    }
  }
  return 0;
}

int COMPOUNDCall::decodeResults()
{
  uint32 status;
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  RETURN_ON_ERROR(reply->xdrDecodeUint32(&status));
  res.status = (nfsstat4)status;
  if (status == NFS4_OK)
  {
    unsigned char* tag = NULL;
    uint32 tagLen;
    RETURN_ON_ERROR(reply->xdrDecodeString(tag, tagLen));

    uint32 opCount = 0;
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&opCount));
    res.resarray.resarray_len = opCount;

    res.resarray.resarray_val = NULL;
    res.resarray.resarray_val = (nfs_resop4*)calloc(opCount, sizeof(nfs_resop4));
    if (res.resarray.resarray_val == NULL)
      return -1;

    // Now decode each command in the compound array to request buffer
    nfs_resop4 *cmdReply = res.resarray.resarray_val;
    for (unsigned i = 0; i < opCount; i++)
    {
      uint32 opCode = 0;
      RETURN_ON_ERROR(reply->xdrDecodeUint32(&opCode));
      cmdReply->resop = (nfs_opnum4)opCode;
      switch (opCode)
      {
        case OP_ACCESS:
        {
          ACCESS4res *result = &(cmdReply->nfs_resop4_u.opaccess);
          RETURN_ON_ERROR(decode_OP_ACCESS(reply, result));
        }
        break;
        case OP_CLOSE:
        {
          CLOSE4res *result = &(cmdReply->nfs_resop4_u.opclose);
          RETURN_ON_ERROR(decode_OP_CLOSE(reply, result));
        }
        break;
        case OP_COMMIT:
        {
          COMMIT4res *result = &(cmdReply->nfs_resop4_u.opcommit);
          RETURN_ON_ERROR(decode_OP_COMMIT(reply, result));
        }
        break;
        case OP_CREATE:
        {
          CREATE4res *result = &(cmdReply->nfs_resop4_u.opcreate);
          RETURN_ON_ERROR(decode_OP_CREATE(reply, result));
        }
        break;
        case OP_DELEGPURGE:
        {
          DELEGPURGE4res *result = &(cmdReply->nfs_resop4_u.opdelegpurge);
          RETURN_ON_ERROR(decode_OP_DELEGPURGE(reply, result));
        }
        break;
        case OP_DELEGRETURN:
        {
          DELEGRETURN4res *result = &(cmdReply->nfs_resop4_u.opdelegreturn);
          RETURN_ON_ERROR(decode_OP_DELEGRETURN(reply, result));
        }
        break;
        case OP_GETATTR:
        {
          GETATTR4res *result = &(cmdReply->nfs_resop4_u.opgetattr);
          RETURN_ON_ERROR(decode_OP_GETATTR(reply, result));
        }
        break;
        case OP_GETFH:
        {
          GETFH4res *result = &(cmdReply->nfs_resop4_u.opgetfh);
          RETURN_ON_ERROR(decode_OP_GETFH(reply, result));
        }
        break;
        case OP_LINK:
        {
          LINK4res *result = &(cmdReply->nfs_resop4_u.oplink);
          RETURN_ON_ERROR(decode_OP_LINK(reply, result));
        }
        break;
        case OP_LOCK:
        {
          LOCK4res *result = &(cmdReply->nfs_resop4_u.oplock);
          RETURN_ON_ERROR(decode_OP_LOCK(reply, result));
        }
        break;
        case OP_LOCKT:
        {
          LOCKT4res *result = &(cmdReply->nfs_resop4_u.oplockt);
          RETURN_ON_ERROR(decode_OP_LOCKT(reply, result));
        }
        break;
        case OP_LOCKU:
        {
          LOCKU4res *result = &(cmdReply->nfs_resop4_u.oplocku);
          RETURN_ON_ERROR(decode_OP_LOCKU(reply, result));
        }
        break;
        case OP_LOOKUP:
        {
          LOOKUP4res *result = &(cmdReply->nfs_resop4_u.oplookup);
          RETURN_ON_ERROR(decode_OP_LOOKUP(reply, result));
        }
        break;
        case OP_LOOKUPP:
        {
          LOOKUPP4res *result = &(cmdReply->nfs_resop4_u.oplookupp);
          RETURN_ON_ERROR(decode_OP_LOOKUPP(reply, result));
        }
        break;
        case OP_NVERIFY:
        {
          NVERIFY4res *result = &(cmdReply->nfs_resop4_u.opnverify);
          RETURN_ON_ERROR(decode_OP_NVERIFY(reply, result));
        }
        break;
        case OP_OPEN:
        {
          OPEN4res *result = &(cmdReply->nfs_resop4_u.opopen);
          RETURN_ON_ERROR(decode_OP_OPEN(reply, result));
        }
        break;
        case OP_OPENATTR:
        {
          OPENATTR4res *result = &(cmdReply->nfs_resop4_u.opopenattr);
          RETURN_ON_ERROR(decode_OP_OPENATTR(reply, result));
        }
        break;
        case OP_OPEN_CONFIRM:
        {
          OPEN_CONFIRM4res *result = &(cmdReply->nfs_resop4_u.opopen_confirm);
          RETURN_ON_ERROR(decode_OP_OPEN_CONFIRM(reply, result));
        }
        break;
        case OP_OPEN_DOWNGRADE:
        {
          OPEN_DOWNGRADE4res *result = &(cmdReply->nfs_resop4_u.opopen_downgrade);
          RETURN_ON_ERROR(decode_OP_OPEN_DOWNGRADE(reply, result));
        }
        break;
        case OP_PUTFH:
        {
          PUTFH4res *result = &(cmdReply->nfs_resop4_u.opputfh);
          RETURN_ON_ERROR(decode_OP_PUTFH(reply, result));
        }
        break;
        case OP_PUTPUBFH:
        {
          PUTPUBFH4res *result = &(cmdReply->nfs_resop4_u.opputpubfh);
          RETURN_ON_ERROR(decode_OP_PUTPUBFH(reply, result));
        }
        break;
        case OP_PUTROOTFH:
        {
          PUTROOTFH4res *result = &(cmdReply->nfs_resop4_u.opputrootfh);
          RETURN_ON_ERROR(decode_OP_PUTROOTFH(reply, result));
        }
        break;
        case OP_READ:
        {
          READ4res *result = &(cmdReply->nfs_resop4_u.opread);
          RETURN_ON_ERROR(decode_OP_READ(reply, result));
        }
        break;
        case OP_READDIR:
        {
          READDIR4res *result = &(cmdReply->nfs_resop4_u.opreaddir);
          RETURN_ON_ERROR(decode_OP_READDIR(reply, result));
        }
        break;
        case OP_READLINK:
        {
          READLINK4res *result = &(cmdReply->nfs_resop4_u.opreadlink);
          RETURN_ON_ERROR(decode_OP_READLINK(reply, result));
        }
        break;
        case OP_REMOVE:
        {
          REMOVE4res *result = &(cmdReply->nfs_resop4_u.opremove);
          RETURN_ON_ERROR(decode_OP_REMOVE(reply, result));
        }
        break;
        case OP_RENAME:
        {
          RENAME4res *result = &(cmdReply->nfs_resop4_u.oprename);
          RETURN_ON_ERROR(decode_OP_RENAME(reply, result));
        }
        break;
        case OP_RENEW:
        {
          RENEW4res *result = &(cmdReply->nfs_resop4_u.oprenew);
          RETURN_ON_ERROR(decode_OP_RENEW(reply, result));
        }
        break;
        case OP_RESTOREFH:
        {
          RESTOREFH4res *result = &(cmdReply->nfs_resop4_u.oprestorefh);
          RETURN_ON_ERROR(decode_OP_RESTOREFH(reply, result));
        }
        break;
        case OP_SAVEFH:
        {
          SAVEFH4res *result = &(cmdReply->nfs_resop4_u.opsavefh);
          RETURN_ON_ERROR(decode_OP_SAVEFH(reply, result));
        }
        break;
        case OP_SECINFO:
        {
          RETURN_ON_ERROR(decode_OP_SECINFO(reply));
        }
        break;
        case OP_SETATTR:
        {
          SETATTR4res *result = &(cmdReply->nfs_resop4_u.opsetattr);
          RETURN_ON_ERROR(decode_OP_SETATTR(reply, result));
        }
        break;
        case OP_SETCLIENTID:
        {
          SETCLIENTID4res *result = &(cmdReply->nfs_resop4_u.opsetclientid);
          RETURN_ON_ERROR(decode_OP_SETCLIENTID(reply, result));
        }
        break;
        case OP_SETCLIENTID_CONFIRM:
        {
          SETCLIENTID_CONFIRM4res *result = &(cmdReply->nfs_resop4_u.opsetclientid_confirm);
          RETURN_ON_ERROR(decode_OP_SETCLIENTID_CONFIRM(reply, result));
        }
        break;
        case OP_VERIFY:
        {
          VERIFY4res *result = &(cmdReply->nfs_resop4_u.opverify);
          RETURN_ON_ERROR(decode_OP_VERIFY(reply, result));
        }
        break;
        case OP_WRITE:
        {
          WRITE4res *result = &(cmdReply->nfs_resop4_u.opwrite);
          RETURN_ON_ERROR(decode_OP_WRITE(reply, result));
        }
        break;
        case OP_RELEASE_LOCKOWNER:
        {
          RELEASE_LOCKOWNER4res *result = &(cmdReply->nfs_resop4_u.oprelease_lockowner);
          RETURN_ON_ERROR(decode_OP_RELEASE_LOCKOWNER(reply, result));
        }
        break;
        default:
        break;
      }
      cmdReply++;
    }
  }
  else
  {
    return -1;
  }
  return 0;
}

int COMPOUNDCall::findOPIndex(int op)
{
  bool found = false;
  nfs_resop4 *result = res.resarray.resarray_val;

  int i;

  for (i = 0; i < (int)res.resarray.resarray_len; i++)
  {
    if (result->resop == op)
    {
      found = true;
      break;
    }
    result++;
  }

  if (i == (int)res.resarray.resarray_len)
  {
    return -1;
  }

  return i;
}

} // namespace NFSv3
} // namespace OpenNfsC
