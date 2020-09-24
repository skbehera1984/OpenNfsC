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
  std::string server, resource;
  int count = 2;

  if ( argc >= 2 )
    server = argv[1];
  if ( argc >=3 )
    resource = argv[2];
  if ( argc >=4 )
  {
    try { count = atoi(argv[3]); } catch (...) {}
    if ( count < 1 ) count = 1;
  }

  if ( server.empty() || resource.empty() )
  {
    cerr << "Usage: " << argv[0] << " <serverOrIP> <resource> [COUNT]" << endl;
    return -1;
  }

  const char *serverIp = server.c_str();
  char *path = const_cast<char*>(resource.c_str());
  struct in_addr ip = {0};
  inet_aton(server.c_str(), &ip);

  NfsConnectionGroupPtr svrPtr = NfsConnectionGroup::create(server/*ip.s_addr*/, TRANSP_TCP);

    //std::string path = "/fen_fs1";

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
 

    cout << "Mount::DumpCall" << endl;
    Mount::DumpCall dumpMount;
    clientState = dumpMount.call(svrPtr);

    mountlist mntList = dumpMount.getResult();
    while (mntList != NULL)
    {
        cout << "got host name " << mntList->ml_hostname << endl;
        cout << "got directory name " << mntList->ml_directory << endl;
        mntList = mntList->ml_next;
    }  


    cout << "Mount::MountCall" << endl;
    Mount::MntCall mountCall(path);
    clientState = mountCall.call(svrPtr);

    if (clientState != ::RPC_SUCCESS)
        cout << "failed to mount" << endl;

    mountres3& mntres = mountCall.getResult();

    READDIRPLUS3args readdirarg = {};
    readdirarg.readdirplus3_dir = mntres.mountres3_u.mount3_mountinfo.mount3_fhandle;
    readdirarg.readdirplus3_dircount = 1024;
    readdirarg.readdirplus3_maxcount = 2048;
    cout << "NFSv3::ReaddirPlus" << endl;
    NFSv3::ReaddirPlusCall rddirplusCall(readdirarg);
    clientState = rddirplusCall.call(svrPtr);

    GETATTR3args getattrarg = {};
    getattrarg.getattr3_object = mntres.mountres3_u.mount3_mountinfo.mount3_fhandle;
    NFSv3::GetAttrCall getattrCall(getattrarg);
    clientState = getattrCall.call(svrPtr);

    MKDIR3args mkdirarg = {};
    mkdirarg.mkdir3_where.dirop3_dir = mntres.mountres3_u.mount3_mountinfo.mount3_fhandle;
    mkdirarg.mkdir3_where.dirop3_name = const_cast<char*>("dir1");

    NFSv3::MkdirCall mkdircall(mkdirarg);
    clientState = mkdircall.call(svrPtr);

    LOOKUP3args lookuparg = {};
    lookuparg.lookup3_what.dirop3_dir = mntres.mountres3_u.mount3_mountinfo.mount3_fhandle;
    lookuparg.lookup3_what.dirop3_name = const_cast<char*>("dir1");
    
    NFSv3::LookUpCall lookupcall(lookuparg);
    clientState = lookupcall.call(svrPtr);
     
    LOOKUP3res& lookupres = lookupcall.getResult();

    CREATE3args createarg = {};
    createarg.create3_where.dirop3_dir = lookupres.LOOKUP3res_u.lookup3ok.lookup3_object; 
    createarg.create3_where.dirop3_name = const_cast<char*>("file1");

    NFSv3::CreateCall createcall(createarg);
    clientState = createcall.call(svrPtr);

    LOOKUP3args lookupfilearg = {};
    lookupfilearg.lookup3_what.dirop3_dir = lookupres.LOOKUP3res_u.lookup3ok.lookup3_object; 
    lookupfilearg.lookup3_what.dirop3_name = const_cast<char*>("file1");

    NFSv3::LookUpCall lookupcall2(lookupfilearg);
    clientState = lookupcall2.call(svrPtr);

    LOOKUP3res& lookupres2 = lookupcall2.getResult();

    {
            nfs_fh3* fhandler = &lookupres2.LOOKUP3res_u.lookup3ok.lookup3_object;
            nlm4_lockargs tLockArg = {};
            string tCookie( "No cookie required" );
            tLockArg.nlm4_lockargs_cookie.n_len = tCookie.size();
            tLockArg.nlm4_lockargs_cookie.n_bytes = (char *) tCookie.c_str();

            tLockArg.nlm4_lockargs_block = false;

            tLockArg.nlm4_lockargs_exclusive = true;

            tLockArg.nlm4_lockargs_alock.nlm4_lock_caller_name = (char*)serverIp; 

            tLockArg.nlm4_lockargs_alock.nlm4_lock_fh.n_len = fhandler->fh3_data.fh3_data_len;
            tLockArg.nlm4_lockargs_alock.nlm4_lock_fh.n_bytes = fhandler->fh3_data.fh3_data_val; 

            string tLockOwner = serverIp;
            tLockArg.nlm4_lockargs_alock.nlm4_lock_oh.n_len = tLockOwner.size();
            tLockArg.nlm4_lockargs_alock.nlm4_lock_oh.n_bytes = (char *) tLockOwner.c_str();

            tLockArg.nlm4_lockargs_alock.nlm4_lock_svid = pthread_self();

            tLockArg.nlm4_lockargs_alock.nlm4_lock_l_offset = 0;

            tLockArg.nlm4_lockargs_alock.nlm4_lock_l_len = 1024;

            tLockArg.nlm4_lockargs_reclaim = false;

            tLockArg.nlm4_lockargs_state = 0;
            
            NLMv4::LockCall lockcall(tLockArg);
            clientState = lockcall.call(svrPtr);

            nlm4_unlockargs tUnlockArg = {};
            tUnlockArg.nlm4_unlockargs_cookie.n_len = tCookie.size();
            tUnlockArg.nlm4_unlockargs_cookie.n_bytes = (char *) tCookie.c_str();

            tUnlockArg.nlm4_unlockargs_alock.nlm4_lock_caller_name = (char*)serverIp; 
            tUnlockArg.nlm4_unlockargs_alock.nlm4_lock_fh.n_len = fhandler->fh3_data.fh3_data_len; 
            tUnlockArg.nlm4_unlockargs_alock.nlm4_lock_fh.n_bytes = fhandler->fh3_data.fh3_data_val;

            tUnlockArg.nlm4_unlockargs_alock.nlm4_lock_oh.n_len = tLockOwner.size();
            tUnlockArg.nlm4_unlockargs_alock.nlm4_lock_oh.n_bytes = (char *) tLockOwner.c_str();

            tUnlockArg.nlm4_unlockargs_alock.nlm4_lock_svid = pthread_self();

            tUnlockArg.nlm4_unlockargs_alock.nlm4_lock_l_offset = 0;

            tUnlockArg.nlm4_unlockargs_alock.nlm4_lock_l_len = 1024;

            NLMv4::UnlockCall unlockcall(tUnlockArg);
            clientState = unlockcall.call(svrPtr);
    }

    

    Mount::UMountCall umountCall(path);
    clientState = umountCall.call(svrPtr);

    Mount::UMntAllCall umountAllCall;
    clientState = umountAllCall.call(svrPtr);


    ConnectionMgr::stopMgr();

    return 0; 
}
    
    
