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

#ifndef _R__FRLIB_LIB_CONNECT_LNQUEUE_H
#define _R__FRLIB_LIB_CONNECT_LNQUEUE_H

#include <fr/base/tlst.h>


#ifdef __cplusplus
extern "C" {
#endif



#define LNQ_F_FREE		0x01
#define LNQ_F_CPY			0x02
#define LNQ_F_DIRECT		0x04		/* no multistream */
#define LNQ_F_NOCHECK	0x08		/* assumes ms-msg is already checked */


struct lnqentry {
	char			*data;
	int			dlen;
	int			refid;
};

struct lnqueue {
	struct tlst	squeue;
	struct tlst	bufs;
	int			mstream;		/* use multistream ? */
	int			prot;			/* multistream protocol to use */
};


int lnq_new (struct lnqueue *lnq);
int lnq_free (struct lnqueue *lnq);

/* prot can be 0 or 1 (see multistream protocol)
 * le can be 1 if little endian shall be used, otherwise 0
 */
int lnq_setmstream (struct lnqueue*, int prot);
int lnq_unsetmstream (struct lnqueue*);
int lnq_addmsg (struct lnqueue*, int stream, char* msg, int msglen, int flags);
int lnq_release (struct lnqueue*, int refid);

/* qen->data or msg needs to be released using lnq_release and refid */
int lnq_popmsg (struct lnqentry *qen, struct lnqueue *lnq, int stream);
int lnq_popmsg2 (char **msg, int *msglen, int *refid, struct lnqueue*, int stream);

/* msg needs to be freed */
int lnq_popcpy (char **msg, int *msglen, struct lnqueue*, int stream);

/* returns 1 if has, 0 if not and < 0 if error */
int lnq_has (struct lnqueue*, int stream);

/* returns the stream no. >= 0 if any, RERR_FAIL if none other value < 0
 * if an error occurred
 */
int lnq_hasany (struct lnqueue *);





#ifdef __cplusplus
}	/* extern "C" */
#endif





#endif	/* _R__FRLIB_LIB_CONNECT_LNQUEUE_H */

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
