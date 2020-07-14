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

#ifndef _NFS4_APIHANDLE_H_
#define _NFS4_APIHANDLE_H_

#include "NfsApiHandle.h"
#include <nfsrpc/nfs4.h>

namespace OpenNfsC {

class Nfs4ApiHandle : public NfsApiHandle
{
  public:
    Nfs4ApiHandle(NfsConnectionGroup *ptr);
    virtual ~Nfs4ApiHandle() {}

  public:
    bool connect(std::string &serverIP);
    bool getRootFH(const std::string &nfs_export);
    bool getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH);
    bool getDirFh(const std::string &dirPath, NfsFh &dirFH);
    bool open(const std::string   filePath,
              uint32_t            access,
              uint32_t            shareAccess,
              uint32_t            shareDeny,
              NfsFh              &file);
    bool read(NfsFh              &fileFH,
              uint64_t            offset,
              uint32_t            length,
              std::string        &data,
              uint32_t           &bytesRead,
              bool               &eof);
    bool write(NfsFh              &fileFH,
               uint64_t            offset,
               std::string        &data,
               uint32_t           &bytesWritten,
               NfsAttr            &postAttr);
    bool write_unstable(NfsFh              &fileFH,
                        uint64_t            offset,
                        std::string        &data,
                        uint32_t           &bytesWritten,
                        NfsAttr            &postAttr);
    bool close(NfsFh &fileFh, NfsAttr &postAttr);
    bool remove(std::string path);
    bool remove(const NfsFh &parentFH, const string &name);
    bool rename(const std::string  &nfs_export,
                const std::string  &fromPath,
                const std::string  &toPath);
    bool readDir(const std::string &dirPath, NfsFiles &files);
    bool truncate(NfsFh &fh, uint64_t size);
    bool truncate(const std::string &path, uint64_t size);
    bool access(const std::string &filePath,
                uint32_t          accessRequested,
                NfsAccess         &acc);
    bool mkdir(const NfsFh &parentFH, const std::string dirName, uint32_t mode);
    bool mkdir(const std::string &path, uint32_t mode, bool createPath = false);
    bool rmdir(const std::string &path);
    bool commit(NfsFh &fh, uint64_t offset, uint32_t bytes, char *writeverf);
    bool lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, bool reclaim = false);
    bool unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length);
    bool setattr( NfsFh &fh, NfsAttr &attr);


  private:
    bool parseReadDir(entry4 *entries, uint32_t mask1, uint32_t mask2, NfsFiles &files);
    NfsFh        m_rootFH;

};

}

#endif /* _NFS4_APIHANDLE_H_ */
