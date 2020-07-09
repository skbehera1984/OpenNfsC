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

#include "NlmCall.h"
#include "RpcPacket.h"
#include "RpcConnection.h"
#include "NfsUtil.h"
#include <syslog.h>

namespace OpenNfsC {
namespace NLMv4 {

#define ENCODE_NETOBJ(PKT, VAR) RETURN_ON_ERROR(PKT->xdrEncodeVarOpaque((unsigned char*)VAR.n_bytes, VAR.n_len))

static inline int decodeNetObj(RpcPacketPtr& pkt, struct netobj* obj)
{
  uint32 len = 0;
  unsigned char* bytes = NULL;
  RETURN_ON_ERROR(pkt->xdrDecodeString(bytes, len));
  obj->n_bytes = reinterpret_cast<char*>(bytes);
  obj->n_len = len;
  return 0;
}

static inline int encodeNlmLock(RpcPacketPtr& pkt, struct nlm4_lock& lock)
{
  RETURN_ON_ERROR(pkt->xdrEncodeString((unsigned char*)lock.nlm4_lock_caller_name, strlen(lock.nlm4_lock_caller_name)));
  ENCODE_NETOBJ(pkt, lock.nlm4_lock_fh);
  ENCODE_NETOBJ(pkt, lock.nlm4_lock_oh);
  RETURN_ON_ERROR(pkt->xdrEncodeUint32(lock.nlm4_lock_svid));
  RETURN_ON_ERROR(pkt->xdrEncodeUint64(lock.nlm4_lock_l_offset));
  RETURN_ON_ERROR(pkt->xdrEncodeUint64(lock.nlm4_lock_l_len));
  return 0;
}

static inline int decodeNlmHolder(RpcPacketPtr& pkt, struct nlm4_holder* holder)
{
  RETURN_ON_ERROR(pkt->xdrDecodeUint32((uint32*)&(holder->nlm4_holder_exclusive)) );
  RETURN_ON_ERROR(pkt->xdrDecodeUint32(&(holder->nlm4_holder_svid)) );
  //DECODE_NETOBJ(pkt, holder.nlm4_holder_oh);
  RETURN_ON_ERROR(decodeNetObj(pkt, &(holder->nlm4_holder_oh)) );
  DECODE_UQUAD(pkt, holder->nlm4_holder_l_offset);
  DECODE_UQUAD(pkt, holder->nlm4_holder_l_len);
  return 0;
}

static inline int decodeNlmRes(RpcPacketPtr& pkt, struct nlm4_res& res)
{
  RETURN_ON_ERROR(decodeNetObj(pkt, &res.nlm4_res_cookie));
  //DECODE_NETOBJ(pkt, res.nlm4_res_cookie);
  uint32 stat = 0;
  RETURN_ON_ERROR(pkt->xdrDecodeUint32(&stat));
  res.nlm4_res_stat.nlm4_stat = (enum nlm4_stats)stat;
  return 0;
}


TestCall::TestCall(const nlm4_testargs& arg):RemoteCall(NLM, NLM_V4_TEST),args(arg),res()
{
}

TestCall::~TestCall()
{
  syslog(LOG_DEBUG, "enter TestCall::~TestCall\n");
}

int TestCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    ENCODE_NETOBJ(request, args.nlm4_testargs_cookie);
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.nlm4_testargs_exclusive));
    return encodeNlmLock(request, args.nlm4_testargs_alock);
  }
  else
    return -1;
}

int TestCall::decodeResults()
{
  uint32 status;
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  RETURN_ON_ERROR(decodeNetObj(reply, &res.nlm4_testres_cookie));
  RETURN_ON_ERROR(reply->xdrDecodeUint32(&status));
  res.nlm4_testres_stat.stat = (enum nlm4_stats)status;
  if (status == NLMSTAT4_DENIED)
  {
    return decodeNlmHolder(reply, &res.nlm4_testres_stat.nlm4_testrply_u.nlm4_testrply_holder);
  }
  return 0;
}

LockCall::LockCall(const nlm4_lockargs& lockargs):RemoteCall(NLM, NLM_V4_LOCK),args(lockargs)
{
}

int LockCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    ENCODE_NETOBJ(request, args.nlm4_lockargs_cookie);
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.nlm4_lockargs_block));
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.nlm4_lockargs_exclusive));
    RETURN_ON_ERROR(encodeNlmLock(request, args.nlm4_lockargs_alock));
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.nlm4_lockargs_reclaim));
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.nlm4_lockargs_state));
    return 0;
  }

  return -1;
}

int LockCall::decodeResults()
{
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  return decodeNlmRes(reply, res);
}

CancelCall::CancelCall(const nlm4_cancargs& cancargs):RemoteCall(NLM, NLM_V4_CANCEL),args(cancargs)
{
}

int CancelCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    ENCODE_NETOBJ(request, args.nlm4_cancargs_cookie);
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.nlm4_cancargs_block));
    RETURN_ON_ERROR(request->xdrEncodeUint32(args.nlm4_cancargs_exclusive));
    RETURN_ON_ERROR(encodeNlmLock(request, args.nlm4_cancargs_alock));
    return 0;
  }

  return -1;
}

int CancelCall::decodeResults()
{
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  return decodeNlmRes(reply, res);
}

UnlockCall::UnlockCall(const nlm4_unlockargs& unlockargs):RemoteCall(NLM, NLM_V4_UNLOCK),args(unlockargs)
{
}

int UnlockCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
  {
    ENCODE_NETOBJ(request, args.nlm4_unlockargs_cookie);
    RETURN_ON_ERROR(encodeNlmLock(request, args.nlm4_unlockargs_alock));
    return 0;
  }

  return -1;
}

int UnlockCall::decodeResults()
{
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  return decodeNlmRes(reply, res);
}

}}
