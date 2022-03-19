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

#ifndef _R__FRLIB_TOOLS_LIB_SPOOL_H
#define _R__FRLIB_TOOLS_LIB_SPOOL_H


#ifdef __cplusplus
extern "C" {
#endif



#define ACTION_NONE				0
#define ACTION_LIST				1
#define ACTION_UNSPOOL			2
#define ACTION_CAT_DFFILE		3
#define ACTION_DELETE			4
#define ACTION_DELETE_ALL		5
#define ACTION_REINSERT			6
#define ACTION_REINSERT_ALL	7
#define ACTION_INSERT			8

#define SPOOL_F_DIRECT		0x0100000		/* don't use daemon */
#define SPOOL_F_LINK			0x0200000
#define SPOOL_F_MOVEXML		0x0400000
#define SPOOL_F_RMXML		0x0800000
#define SPOOL_F_NOSTREAM	0x1000000
#define SPOOL_F_LISTEN		0x2000000



int spool_main (int arcg, char **argv);
int spool_usage ();

int spool_server (char *spool, int flags);
int spool_action (char *spool, int action, char *options, int num, int flags);
int spool_insertfile (char *spool, char *infile, char *options, int flags);
int spool_insertbuf (char *spool, char *buf, char *options, int flags);

int spool_reinsertall (char *spool, char *options, int flags);
int spool_delwithxml (char *spool, char *cf_file, int flags);
int spool_deleteall (char *spool, int flags);

int spool_xmldel (char *spool, char *cf_name);
int spool_xmlmove2err (char *spool, char *cf_name);







#ifdef __cplusplus
}	/* extern "C" */
#endif










#endif	/* _R__FRLIB_TOOLS_LIB_SPOOL_H */

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
