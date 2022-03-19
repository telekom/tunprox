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
 * Portions created by the Initial Developer are Copyright (C) 2003-2021
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef _R__FRLIB_LIB_BASE_DN_H
#define _R__FRLIB_LIB_BASE_DN_H

#include <fr/base/tlst.h>
#include <fr/base/bufref.h>

#ifdef __cplusplus
extern "C" {
#endif




struct dn_part {
	const char	*var, *val;
};

struct dn {
	int				hascpy;
	struct bufref	bref;
	struct tlst		list;
};

#define DN_SINIT		{ .bref = BUFREF_SINIT, .list = TLST_SINIT_T(struct dn_part) }
#define DN_INIT		((struct dn)DN_SINIT)
#define DN_NULL		DN_INIT


int dn_split (struct dn*, const char*);
int dn_split_inplace (struct dn*, char*);
int dn_cmp (const char *dn1, const char *dn2);
int dn_cmp_struct (const struct dn *dn1, const struct dn *dn2);
const char * dn_getpart (const struct dn*, const char *var);

int dn_hfree (struct dn*);
int dn_cpy (struct dn*, const struct dn*);









#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_BASE_DN_H */

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
