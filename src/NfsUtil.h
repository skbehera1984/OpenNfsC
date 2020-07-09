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

#ifndef _NFS_UTIL_
#define _NFS_UTIL_

#include "RpcPacket.h"
#include "DataTypes.h"
#include <nfsrpc/nfs.h>
#include <nfsrpc/nfs4.h>

#include <vector>
#include <string>
#include <syslog.h>

namespace OpenNfsC {

class Buffer;
class RpcPacket;

namespace NfsUtil {

//#define RETURN_ON_FALSE(x) ({ if ( !(x) ) { \
//          syslog(LOG_ERR, "TST failed at " __FILE__ ":%d in %s: " #x "\n", __LINE__, __PRETTY_FUNCTION__);  \
//          return -1; } })

#define RETURN_ON_FALSE(x) ({ if ( !(x) ) { \
          return -1; } })

#define RETURN_ON_ERROR(x) RETURN_ON_FALSE((x >= 0))

#define ENCODEVAR(req, VAR) req->xdrEncodeFixedOpaque((unsigned char*)&(VAR),sizeof(VAR))
#define DECODEVAR(req, VAR) req->xdrDecodeFixedOpaque((unsigned char*)&(VAR),sizeof(VAR))

#define DECODE_UQUAD(PKT, VAR) \
{ \
    uint64 iVal = 0; \
    RETURN_ON_ERROR(PKT->xdrDecodeUint64(&iVal)); \
    VAR = iVal; \
} \

  int decodeFH3(RpcPacketPtr pkt, struct nfs_fh3* fh);
  int decodePostOpAttr(RpcPacketPtr pkt, struct post_op_attr *post);
  int decodePreOpAttr(RpcPacketPtr pkt, struct pre_op_attr *pre);
  int decodeWccData(RpcPacketPtr pkt, struct wcc_data *pre);
  int decodePostOpFH3(RpcPacketPtr pkt, struct post_op_fh3 *postfh);
  int encodeSattr3(RpcPacketPtr pkt, sattr3 *sa);
  int decodeFattr3(RpcPacketPtr pkt, fattr3 *sa);
  int decodeString(RpcPacketPtr pkt, char**name);
  int decodeStringToBuffer(RpcPacketPtr pkt, Buffer* buf);

  void splitNfsPath(const std::string &Path,
                    std::vector<std::string> &Segments);
  void buildNfsPath(std::string &Path,
                    std::vector<std::string> &Segments);

  int encode_fattr4(RpcPacketPtr packet, const fattr4 *attr);
  int decode_fattr4(fattr4 *fattr, uint32_t mask1, uint32_t mask2, NfsAttr &attr);
  int NfsAttr_fattr4(NfsAttr &attr, fattr4 *fattr);
}}

#endif /* _NFS_UTIL_ */
