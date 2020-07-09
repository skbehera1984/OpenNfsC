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

#ifndef _RPC_DEFS_
#define _RPC_DEFS_

namespace OpenNfsC {

enum RpcMessageType
{
  RPC_CALL = 0,
  RPC_REPLY
};

enum RpcAuthType
{
  AUTHTYPE_NULL = 0,
  AUTHTYPE_UNIX
};

enum RpcReplyState
{
  REPLY_SUCCESS = 0,
  REPLY_ACCEPTED_ERR,
  REPLY_REJECTED
};

enum TransportType
{
  TRANSP_UDP = 0,
  TRANSP_TCP,
  MAX_TRANSP
};

enum ServiceType
{
  PORTMAP = 0,
  MOUNT,
  NFS,
  NLM,
  MAX_SERVICE
};

} // end of namespace
#endif /* _RPC_DEFS_ */
