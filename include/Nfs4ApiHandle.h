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
    bool connect(std::string &serverIP, NfsError &status);
    bool getRootFH(const std::string &nfs_export, NfsFh &rootFh, NfsError &status);
    bool getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH, NfsError &status);
    bool getDirFh(const std::string &dirPath, NfsFh &dirFH, NfsError &status);
    bool getFileHandle(NfsFh &rootFH, const std::string path, NfsFh &fileFh, NfsAttr &attr, NfsError &status);
    bool create(NfsFh             &dirFh,
                std::string       &fileName,
                NfsAttr           *inAttr,
                NfsFh             &fileFh,
                NfsAttr           &outAttr,
                NfsError          &status);
    bool open(const std::string   filePath,
              uint32_t            access,
              uint32_t            shareAccess,
              uint32_t            shareDeny,
              NfsFh              &file,
              NfsError           &status);
    bool read(NfsFh              &fileFH,
              uint64_t            offset,
              uint32_t            length,
              std::string        &data,
              uint32_t           &bytesRead,
              bool               &eof,
              NfsAttr            &postAttr,
              NfsError           &status);
    bool write(NfsFh              &fileFH,
               uint64_t            offset,
               uint32_t            length,
               std::string        &data,
               uint32_t           &bytesWritten,
               NfsError           &status);
    bool write_unstable(NfsFh              &fileFH,
                        uint64_t            offset,
                        std::string        &data,
                        uint32_t           &bytesWritten,
                        char               *verf,
                        const bool          needverify,
                        NfsError            &status);
    bool close(NfsFh &fileFh, NfsAttr &postAttr, NfsError &status);
    bool remove(std::string &exp, std::string path, NfsError &status);
    bool remove(const NfsFh &parentFH, const string &name, NfsError &status);
    bool rename(NfsFh &fromDirFh,
                const std::string &fromName,
                NfsFh &toDirFh,
                const std::string toName,
                NfsError &status);
    bool rename(const std::string  &nfs_export,
                const std::string  &fromPath,
                const std::string  &toPath,
                NfsError           &status);
    bool readDir(const std::string &dirPath, NfsFiles &files, NfsError &status);
    bool readDir(NfsFh &dirFh, NfsFiles &files, NfsError &status);
    bool truncate(NfsFh &fh, uint64_t size, NfsError &status);
    bool truncate(const std::string &path, uint64_t size, NfsError &status);
    bool access(const std::string &filePath,
                uint32_t          accessRequested,
                NfsAccess         &acc,
                NfsError          &status);
    bool mkdir(const NfsFh &parentFH, const std::string dirName, uint32_t mode, NfsFh &dirFH, NfsError &status);
    bool mkdir(const std::string &path, uint32_t mode, NfsError &status, bool createPath = false);
    bool rmdir(std::string &exp, const std::string &path, NfsError &status);
    bool rmdir(const NfsFh &parentFH, const string &name, NfsError &status);
    bool commit(NfsFh &fh, uint64_t offset, uint32_t bytes, char *writeverf, NfsError &status);
    bool lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status, bool reclaim = false);
    bool unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status);
    bool setattr(NfsFh &fh, NfsAttr &attr, NfsError &status);
    bool getAttr(NfsFh &fh, NfsAttr &attr, NfsError &status);
    bool lookup(const std::string &path, NfsFh &lookup_fh, NfsError &status);
    bool lookup(NfsFh &dirFh, const std::string &file, NfsFh &lookup_fh, NfsAttr &attr, NfsError &status);
    bool fsstat(NfsFh &rootFh, NfsFsStat &stat, uint32 &invarSec, NfsError &status);
    bool link(NfsFh           &tgtFh,
              NfsFh           &parentFh,
              const string    &linkName,
              NfsError        &status);
    bool symlink(const string &tgtPath,
                 NfsFh        &parentFh,
                 const string &linkName,
                 NfsError     &status);

  private:
    bool readDirV4(NfsFh &dirFh, uint64_t &Cookie, verifier4 &vref, NfsFiles &files, bool &eof, NfsError &status);

};

}

#endif /* _NFS4_APIHANDLE_H_ */
