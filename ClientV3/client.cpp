#include "ConnectionMgr.h"
#include "NfsConnectionGroup.h"
#include "RpcConnection.h"
#include "MountCall.h"
#include "NfsCall.h"
#include "NlmCall.h"
#include "RpcDefs.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

using namespace std;
using namespace OpenNfsC;

int main(int argc, char* argv[])
{
  std::string server, exp_path, res_path;
  int count = 2;

  if ( argc >= 2 )
    server = argv[1];
  if ( argc >=3 )
    exp_path = argv[2];
  if ( argc >=4 )
    res_path = argv[3];

  if ( argc >=5 )
  {
    try { count = atoi(argv[4]); } catch (...) {}
    if ( count < 1 ) count = 1;
  }

  if ( server.empty() || exp_path.empty() )
  {
    cerr << "Usage: " << argv[0] << " <serverOrIP> <export_path> <resource_path> [COUNT]" << endl;
    return -1;
  }

  NfsConnectionGroupPtr svrPtr = NfsConnectionGroup::create(server/*ip.s_addr*/, TRANSP_TCP);

  Mount::ExportCall exportCall;
  enum clnt_stat clientState = exportCall.call(svrPtr);

  if (clientState == ::RPC_SUCCESS)
  {
    exports exportList = exportCall.getResult();
    while(exportList != NULL)
    {
      cout << "got dir name " << exportList->ex_dir << ":";
      groupnode * groupPtr = exportList->ex_groups;
      while ( groupPtr != NULL)
      {
        cout << groupPtr->gr_name << ",";
        groupPtr = groupPtr->gr_next;
      }
      cout << ";" << endl;
      exportList = exportList->ex_next;
    }
  }

  NfsError err;

  /* Test for readDir */
  NfsFiles files;
  if (svrPtr->readDir(exp_path, res_path, files, err) != false)
  {
    cout << "NFSV4 READDIR successful" << endl;
  }
  for (const NfsFile &file : files)
  {
    std::string type;
    if (file.type == FILE_TYPE_REG)
      type = "file";
    else if (file.type == FILE_TYPE_DIR)
      type = "directory";
    cout << file.name << ":"<< type <<endl;
  }
  cout << "No of files - " << files.size() <<endl;

  /* Test for RENAME using PATH */
  cout <<"Test RENAME using PATH\n"<<endl;
/*
  err.clear();
  if (svrPtr->rename("/ctafs1", "dir1", "dir2", err) != false)
  {
    cout << "NFSV3 RENAME successful" << endl;
  }
*/

  /* Test for MKDIR using ParentFH */

  cout <<"Test for MKDIR using ParentFH"<<endl;
  NfsFh parentFH, rootFh, dirFH;
  std::string path ="dir2/rabi";
  err.clear();
  if(svrPtr->getRootFH(exp_path, rootFh, err))
  {
    cout <<"Got the RootFH for exp_path=" <<exp_path<<endl;
  }
  if(svrPtr->getDirFh(rootFh, path, parentFH, err))
  {
    cout <<"Got the DirFh for path="<<path<<endl;
  }
  if(svrPtr->mkdir(parentFH, "newdir", 077, dirFH, err))
  {
    cout <<"NFSV3 mkdir successful Created a dir newdir under path="<<path<<endl;
  }

  return 0;
}
