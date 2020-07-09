/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

%#include <nfsrpc/nfs.h>
%#include <nfsrpc/sm.h>

const NLM_MAXSTRLEN	= 1024;
const NLM_MAXNAMELEN	= 1025;

%#ifndef MAX_NETOBJ_SZ
const MAX_NETOBJ_SZ = 1024;
typedef opaque netobj<MAX_NETOBJ_SZ>;
%#endif

/*
 * status of a call to the lock manager
 */
enum nlm_stats {
	NLMSTAT_GRANTED = 0,
	NLMSTAT_DENIED = 1,
	NLMSTAT_DENIED_NOLOCKS = 2,
	NLMSTAT_BLOCKED = 3,
	NLMSTAT_DENIED_GRACE_PERIOD = 4,
	NLMSTAT_DEADLCK = 5
};

struct nlm_holder {
	bool nlm_holder_exclusive;
	int nlm_holder_svid;
	netobj nlm_holder_oh;
	unsigned nlm_holder_l_offset;
	unsigned nlm_holder_l_len;
};

union nlm_testrply switch (nlm_stats stat) {
	case NLMSTAT_DENIED:
		struct nlm_holder nlm_testrply_holder;
	default:
		void;
};

struct nlm_stat {
	nlm_stats nlm_stat;
};

struct nlm_res {
	netobj nlm_res_cookie;
	nlm_stat nlm_res_stat;
};

struct nlm_testres {
	netobj nlm_testres_cookie;
	nlm_testrply nlm_testres_stat;
};

struct nlm_lock {
	string nlm_lock_caller_name<NLM_MAXSTRLEN>;
	netobj nlm_lock_fh;		/* identify a file */
	netobj nlm_lock_oh;		/* identify owner of a lock */
	int nlm_lock_svid;		/* generated from pid for svid */
	unsigned nlm_lock_l_offset;
	unsigned nlm_lock_l_len;
};

struct nlm_lockargs {
	netobj nlm_lockargs_cookie;
	bool nlm_lockargs_block;
	bool nlm_lockargs_exclusive;
	struct nlm_lock nlm_lockargs_alock;
	bool nlm_lockargs_reclaim;		/* used for recovering locks */
	int nlm_lockargs_state;		/* specify local status monitor state */
};

struct nlm_cancargs {
	netobj nlm_cancargs_cookie;
	bool nlm_cancargs_block;
	bool nlm_cancargs_exclusive;
	struct nlm_lock nlm_cancargs_alock;
};

struct nlm_testargs {
	netobj nlm_testargs_cookie;
	bool nlm_testargs_exclusive;
	struct nlm_lock nlm_testargs_alock;
};

struct nlm_unlockargs {
	netobj nlm_unlockargs_cookie;
	struct nlm_lock nlm_unlockargs_alock;
};


enum	fsh_mode {
	fsm_DN  = 0,	/* deny none */
	fsm_DR  = 1,	/* deny read */
	fsm_DW  = 2,	/* deny write */
	fsm_DRW = 3	/* deny read/write */
};

enum	fsh_access {
	fsa_NONE = 0,	/* for completeness */
	fsa_R    = 1,	/* read only */
	fsa_W    = 2,	/* write only */
	fsa_RW   = 3	/* read/write */
};

struct	nlm_share {
	string nlm_share_caller_name<NLM_MAXSTRLEN>;
	netobj	nlm_share_fh;
	netobj	nlm_share_oh;
	fsh_mode	nlm_share_mode;
	fsh_access	nlm_share_access;
};

struct	nlm_shareargs {
	netobj	nlm_shareargs_cookie;
	nlm_share	nlm_shareargs_share;
	bool	nlm_shareargs_reclaim;
};

struct	nlm_shareres {
	netobj	nlm_shareres_cookie;
	nlm_stats	nlm_shareres_stat;
	int	nlm_shareres_sequence;
};

struct	nlm_notify {
	string nlm_notify_name<NLM_MAXNAMELEN>;
	int nlm_notify_state;
};

enum nlm4_stats {
	NLMSTAT4_GRANTED		= 0,
	NLMSTAT4_DENIED			= 1,
	NLMSTAT4_DENIED_NOLOCKS		= 2,
	NLMSTAT4_BLOCKED		= 3,
	NLMSTAT4_DENIED_GRACE_PERIOD	= 4,
	NLMSTAT4_DEADLCK		= 5,
	NLMSTAT4_ROFS			= 6,
	NLMSTAT4_STALE_FH		= 7,
	NLMSTAT4_FBIG			= 8,
	NLMSTAT4_FAILED			= 9
};

struct nlm4_stat {
	nlm4_stats nlm4_stat;
};

struct nlm4_holder {
	bool nlm4_holder_exclusive;
	unsigned int nlm4_holder_svid;
	netobj nlm4_holder_oh;
	unsigned hyper nlm4_holder_l_offset;
	unsigned hyper nlm4_holder_l_len;
};

struct nlm4_lock {
	string nlm4_lock_caller_name<NLM_MAXNAMELEN>;
	netobj nlm4_lock_fh;
	netobj nlm4_lock_oh;
	unsigned int nlm4_lock_svid;
	unsigned hyper nlm4_lock_l_offset;
	unsigned hyper nlm4_lock_l_len;
};

struct nlm4_share {
	string nlm4_share_caller_name<NLM_MAXNAMELEN>;
	netobj nlm4_share_fh;
	netobj nlm4_share_oh;
	fsh_mode nlm4_share_mode;
	fsh_access nlm4_share_access;
};

union nlm4_testrply switch (nlm4_stats stat) {
	case NLMSTAT4_DENIED:
		struct nlm4_holder nlm4_testrply_holder;
	default:
		void;
};

struct nlm4_testres {
	netobj nlm4_testres_cookie;
	nlm4_testrply nlm4_testres_stat;
};

struct nlm4_testargs {
	netobj nlm4_testargs_cookie;
	bool nlm4_testargs_exclusive;
	struct nlm4_lock nlm4_testargs_alock;
};

struct nlm4_res {
	netobj nlm4_res_cookie;
	nlm4_stat nlm4_res_stat;
};

struct nlm4_lockargs {
	netobj nlm4_lockargs_cookie;
	bool nlm4_lockargs_block;
	bool nlm4_lockargs_exclusive;
	struct nlm4_lock nlm4_lockargs_alock;
	bool nlm4_lockargs_reclaim;		/* used for recovering locks */
	int nlm4_lockargs_state;		/* specify local status monitor state */
};

struct nlm4_cancargs {
	netobj nlm4_cancargs_cookie;
	bool nlm4_cancargs_block;
	bool nlm4_cancargs_exclusive;
	struct nlm4_lock nlm4_cancargs_alock;
};

struct nlm4_unlockargs {
	netobj nlm4_unlockargs_cookie;
	struct nlm4_lock nlm4_unlockargs_alock;
};

struct	nlm4_shareargs {
	netobj	nlm4_shareargs_cookie;
	nlm4_share	nlm4_shareargs_share;
	bool	nlm4_shareargs_reclaim;
};

struct	nlm4_shareres {
	netobj	nlm4_shareres_cookie;
	nlm4_stats	nlm4_shareres_stat;
	int	nlm4_shareres_sequence;
};

struct	nlm4_notify {
	string nlm4_notify_name<NLM_MAXNAMELEN>;
	int nlm4_notify_state;
};

/*
 * Over-the-wire protocol used between the network lock managers
 */

program RPCPROG_NLM {

	version NLM_SM {
		void NLM_SM_NOTIFY(struct sm_status) = 1;
	} = 0;

	version NLM_V1 {
		void 		NLM_V1_NULL(void) = 0; /* not officially defined */

		nlm_testres	NLM_V1_TEST(struct nlm_testargs) =	1;

		nlm_res		NLM_V1_LOCK(struct nlm_lockargs) =	2;

		nlm_res		NLM_V1_CANCEL(struct nlm_cancargs) = 3;
		nlm_res		NLM_V1_UNLOCK(struct nlm_unlockargs) =	4;

		/*
		 * remote lock manager call-back to grant lock
		 */
		nlm_res		NLM_V1_GRANTED(struct nlm_testargs)= 5;
		/*
		 * message passing style of requesting lock
		 */
		void		NLM_V1_TEST_MSG(struct nlm_testargs) = 6;
		void		NLM_V1_LOCK_MSG(struct nlm_lockargs) = 7;
		void		NLM_V1_CANCEL_MSG(struct nlm_cancargs) =8;
		void		NLM_V1_UNLOCK_MSG(struct nlm_unlockargs) = 9;
		void		NLM_V1_GRANTED_MSG(struct nlm_testargs) = 10;
		void		NLM_V1_TEST_RES(nlm_testres) = 11;
		void		NLM_V1_LOCK_RES(nlm_res) = 12;
		void		NLM_V1_CANCEL_RES(nlm_res) = 13;
		void		NLM_V1_UNLOCK_RES(nlm_res) = 14;
		void		NLM_V1_GRANTED_RES(nlm_res) = 15;
	} = 1;

	version NLM_V3 {
		nlm_testres	NLM_V3_TEST(struct nlm_testargs) =	1;
		nlm_res		NLM_V3_LOCK(struct nlm_lockargs) =	2;
		nlm_res		NLM_V3_CANCEL(struct nlm_cancargs) = 3;
		nlm_res		NLM_V3_UNLOCK(struct nlm_unlockargs) =	4;
		nlm_res		NLM_V3_GRANTED(struct nlm_testargs)= 5;
		void		NLM_V3_TEST_MSG(struct nlm_testargs) = 6;
		void		NLM_V3_LOCK_MSG(struct nlm_lockargs) = 7;
		void		NLM_V3_CANCEL_MSG(struct nlm_cancargs) =8;
		void		NLM_V3_UNLOCK_MSG(struct nlm_unlockargs) = 9;
		void		NLM_V3_GRANTED_MSG(struct nlm_testargs) = 10;
		void		NLM_V3_TEST_RES(nlm_testres) = 11;
		void		NLM_V3_LOCK_RES(nlm_res) = 12;
		void		NLM_V3_CANCEL_RES(nlm_res) = 13;
		void		NLM_V3_UNLOCK_RES(nlm_res) = 14;
		void		NLM_V3_GRANTED_RES(nlm_res) = 15;

		nlm_shareres	NLM_V3_SHARE(nlm_shareargs) = 20;
		nlm_shareres	NLM_V3_UNSHARE(nlm_shareargs) = 21;
		nlm_res		NLM_V3_NM_LOCK(nlm_lockargs) = 22;
		void		NLM_V3_FREE_ALL(nlm_notify) = 23;
	} = 3;

	version NLM_V4 {
		void 		NLM_V4_NULL(void) = 0;
		nlm4_testres    NLM_V4_TEST(nlm4_testargs) = 1;
		nlm4_res        NLM_V4_LOCK(nlm4_lockargs) = 2;
		nlm4_res        NLM_V4_CANCEL(nlm4_cancargs) = 3;
		nlm4_res        NLM_V4_UNLOCK(nlm4_unlockargs) = 4;
		nlm4_res        NLM_V4_GRANTED(nlm4_testargs) = 5;
		void            NLM_V4_TEST_MSG(nlm4_testargs) = 6;
		void            NLM_V4_LOCK_MSG(nlm4_lockargs) = 7;
		void            NLM_V4_CANCEL_MSG(nlm4_cancargs) = 8;
		void            NLM_V4_UNLOCK_MSG(nlm4_unlockargs) = 9;
		void            NLM_V4_GRANTED_MSG(nlm4_testargs) = 10;
		void            NLM_V4_TEST_RES(nlm4_testres) = 11;
		void            NLM_V4_LOCK_RES(nlm4_res) = 12;
		void            NLM_V4_CANCEL_RES(nlm4_res) = 13;
		void            NLM_V4_UNLOCK_RES(nlm4_res) = 14;
		void            NLM_V4_GRANTED_RES(nlm4_res) = 15;
		nlm4_shareres   NLM_V4_SHARE(nlm4_shareargs) = 20;
		nlm4_shareres   NLM_V4_UNSHARE(nlm4_shareargs) = 21;
		nlm4_res        NLM_V4_NM_LOCK(nlm4_lockargs) = 22;
		void            NLM_V4_FREE_ALL(nlm4_notify) = 23;
	} = 4;
} = 100021;

