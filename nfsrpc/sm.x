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

const	SM_MAXSTRLEN = 1024;

/* 
 * structure of the status message sent back by the status monitor
 * when monitor site status changes
 */
struct sm_status {
	string sm_mon_name<SM_MAXSTRLEN>;
	int sm_state;
	opaque sm_priv[16];		/* stored private information */
};

struct sm_name {
	string sm_name_mon_name<SM_MAXSTRLEN>;
};

struct my_id {
	string	 my_name<SM_MAXSTRLEN>;		/* name of the site iniates the monitoring request*/
	int	my_prog;			/* rpc program # of the requesting process */
	int	my_vers;			/* rpc version # of the requesting process */
	int	my_proc;			/* rpc procedure # of the requesting process */
};

struct mon_id {
	string	mon_id_name<SM_MAXSTRLEN>;		/* name of the site to be monitored */
	struct my_id my_mon_id;
};


struct mon{
	struct mon_id mon_id;
	opaque mon_priv[16]; 		/* private information to store at monitor for requesting process */
};

struct stat_chge {
	string  stat_chge_mon_name<SM_MAXSTRLEN>;         /* name of the site that had the state change */
	int stat_chge_state;
};

/*
 * state # of status monitor monitonically increases each time
 * status of the site changes:
 * an even number (>= 0) indicates the site is down and
 * an odd number (> 0) indicates the site is up;
 */
struct sm_stat {
	int sm_stat_state;		/* state # of status monitor */
};

enum sm_res {
	stat_succ = 0,		/* status monitor agrees to monitor */
	stat_fail = 1		/* status monitor cannot monitor */
};

struct sm_stat_res {
	sm_res sm_res_stat;
	int sm_res_state;
};

program RPCPROG_SM { 
	version SM_V1  {
		/* res_stat = stat_succ if status monitor agrees to monitor */
		/* res_stat = stat_fail if status monitor cannot monitor */
		/* if res_stat == stat_succ, state = state number of site sm_name */
		struct sm_stat_res			 SM_V1_STAT(struct sm_name) = 1;

		/* res_stat = stat_succ if status monitor agrees to monitor */
		/* res_stat = stat_fail if status monitor cannot monitor */
		/* stat consists of state number of local site */
		struct sm_stat_res			 SM_V1_MON(struct mon) = 2;

		/* stat consists of state number of local site */
		struct sm_stat				 SM_V1_UNMON(struct mon_id) = 3;

		/* stat consists of state number of local site */
		struct sm_stat				 SM_V1_UNMON_ALL(struct my_id) = 4;

		void					 SM_V1_SIMU_CRASH(void) = 5;
		void					 SM_V1_NOTIFY(struct stat_chge) = 6;

	} = 1;
} = 100024;

