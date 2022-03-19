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
 * Portions created by the Initial Developer are Copyright (C) 2003-2020
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef _R__FRLIB_LIB_BASE_BUFREF_H
#define _R__FRLIB_LIB_BASE_BUFREF_H


#include <stdint.h>
#include <fr/base/tlst.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bufref {
	int			isinit;
	struct tlst	lst;
};

#define BUFREF_SINIT		{ .isinit = 0 }
#define BUFREF_INIT	((struct bufref) BUFREF_SINIT)

void bufref_free (struct bufref *bufref);
void bufref_reset (struct bufref *bufref);
void bufref_clear (struct bufref *bufref);
int bufref_add (struct bufref *bufref, char *buf, size_t blen);
int bufref_add2 (struct bufref *bufref, char *buf);
int bufref_rmbuf (struct bufref *bufref, char *buf);
int bufref_ref (struct bufref *bufref, const char *buf);
int bufref_unref (struct bufref *bufref, const char *buf);
int bufref_addref (struct bufref *bufref, char *buf);
int bufref_addcpy (struct bufref *bufref, char **obuf, const char *buf, size_t blen);
char *bufref_strcpy (struct bufref *bufref, const char *str);
int bufref_reforcpy (struct bufref*, char **obuf, const char *buf, size_t blen);
const char *bufref_strreforcpy (struct bufref*, const char *str);
int bufref_numref (struct bufref*, const char *buf);

int bufref_cpy (struct bufref *dsttmp, const struct bufref *src);
int bufref_cpyfinish (struct bufref *dst, struct bufref *dsttmp);
const char *bufref_refcpy (struct bufref *dsttmp, const char *buf);



#ifdef __cplusplus
}	/* extern "C" */
#endif






























#endif	/* _R__FRLIB_LIB_BASE_BUFREF_H */

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
