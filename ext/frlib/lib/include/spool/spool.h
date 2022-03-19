/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 2.0
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for more details governing rights and limitations under the License.
 *
 * The Original Code is part of the frlib.
 *
 * The Initial Developer of the Original Code is
 * Frank Reker <frank@reker.net>.
 * Portions created by the Initial Developer are Copyright (C) 2003-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _R__FRLIB_LIB_SPOOL_SPOOL_H
#define _R__FRLIB_LIB_SPOOL_SPOOL_H

#include <fr/base/tmo.h>


#ifdef __cplusplus
extern "C" {
#endif



#define SPOOL_STATE_UNKNOWN	0
#define SPOOL_STATE_WAIT		1
#define SPOOL_STATE_ELAB		2
#define SPOOL_STATE_ERROR		3
#define SPOOL_STATE_MAX			3


struct spool_entry {
	char	*cf_name,
			*df_name;
	tmo_t	ctime, mtime;
	int	num, state;
	int	pid, error;
	int	size;
	char	*options;
};

	
struct spoollist {
	struct spool_entry	*list;
	int						listnum;
	int						next_to_elab;
	char						*spoolname;
	char						*spooldir;
	char						*subdir;
	int						has_elab;
};

#define SPOOL_ENTRY_NULL	((struct spool_entry){NULL,NULL,0,0,0,0,0,0,0,NULL})
#define SPOOL_LIST_NULL		((struct spoollist){NULL,0,0,NULL,NULL,NULL,0})


#define SPOOL_F_PRT_NONE		0x000
#define SPOOL_F_PRT_WAIT		0x001
#define SPOOL_F_PRT_ELAB		0x002
#define SPOOL_F_PRT_ERR			0x004
#define SPOOL_F_PRT_UNKNOWN	0x008
#define SPOOL_F_PRT_ERRALL		0x00c
#define SPOOL_F_PRT_ALL			0x00f

#define SPOOL_F_SORT_STATE		0x010
#define SPOOL_F_SORT_NUM		0x020
#define SPOOL_F_SORT_CTIME		0x040
#define SPOOL_F_SORT_MTIME		0x080
#define SPOOL_F_SHOW_CTIME		0x100
#define SPOOL_F_SHOW_DFFILE	0x200


/* old bindings */
#define SPOOL_FLAG_PRT_NONE		SPOOL_F_PRT_NONE
#define SPOOL_FLAG_PRT_WAIT		SPOOL_F_PRT_WAIT
#define SPOOL_FLAG_PRT_ELAB		SPOOL_F_PRT_ELAB
#define SPOOL_FLAG_PRT_ERROR		SPOOL_F_PRT_ERR
#define SPOOL_FLAG_PRT_UNKNOWN	SPOOL_F_PRT_UNKNOWN
#define SPOOL_FLAG_PRT_ERROR2		SPOOL_F_PRT_ERRALL
#define SPOOL_FLAG_PRT_ALL			SPOOL_F_PRT_ALL

#define SPOOL_FLAG_SORT_STATE		SPOOL_F_SORT_STATE
#define SPOOL_FLAG_SORT_NUM		SPOOL_F_SORT_NUM
#define SPOOL_FLAG_SORT_CTIME		SPOOL_F_SORT_CTIME
#define SPOOL_FLAG_SORT_MTIME		SPOOL_F_SORT_MTIME
#define SPOOL_FLAG_SHOW_CTIME		SPOOL_F_SHOW_CTIME
#define SPOOL_FLAG_SHOW_DFFILE	SPOOL_F_SHOW_DFFILE
/* end old bindings */

#define SPOOL_SORT_STATE	1
#define SPOOL_SORT_NUM		2
#define SPOOL_SORT_CTIME	3
#define SPOOL_SORT_MTIME	4


int spool_in (const char *spool, char *buf, int buflen, const char *options);
int spool_infile (const char *spool, const char *filename, int dolink, const char *options);
int spool_infd (const char *spool, int fd, const char *options);
int spool_out (const char *spool, char **buf, int *buflen, char **cf_name, 
					char **df_name, char **options);
int spool_outfile (const char *spool, char **cf_name, char **df_name, char **options);
int spool_outdel (const char *spool, char **buf, int *buflen);

int spool_chstate (const char *spool, const char *cf_name, int newstate, int error);
int spool_chstate2 (	const char *spool, const char *cf_name, int newstate, int error, 
							const char *options);
int spool_delentry (const char *spool, const char *cf_name);
int spool_list (const char *spool, struct spoollist*, int flag);
int spool_print (const char *spool, int flag);
int spool_num2cf (char **cf_name, const char *spool, int num);
int spool_read_df (char **buf, int *buflen, const char *spool, const char *cf_name);

int hfree_spoolentry (struct spool_entry *);
int hfree_spoollist (struct spoollist *);

int spool_get_runnum (const char *spooldir);


int spool_lockfile_create (const char *filename, tmo_t timeout);








#ifdef __cplusplus
}	/* extern "C" */
#endif















#endif	/* _R__FRLIB_LIB_SPOOL_SPOOL_H */

/*
 * Overrides for XEmacs and vim so that we get a uniform tabbing style.
 * XEmacs/vim will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 3
 * c-basic-offset: 3
 * tab-width: 3
 * End:
 * vim:tw=0:ts=3:wm=0:
 */
