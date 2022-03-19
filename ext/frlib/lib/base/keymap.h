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

#ifndef _R__FRLIB_LIB_BASE_KEYMAP_H
#define _R__FRLIB_LIB_BASE_KEYMAP_H


#ifdef __cplusplus
extern "C" {
#endif


#define KEYMAP_F_NONE			0x00
#define KEYMAP_F_CPY				0x01
#define KEYMAP_F_DEL				0x02
#define KEYMAP_F_DELKEY			0x04
#define KEYMAP_F_RO				0x08
#define KEYMAP_F_READONLY		0x08
#define KEYMAP_F_IGNORE_CASE	0x10
#define KEYMAP_F_TOLOWER		0x20
#define KEYMAP_F_TOUPPER		0x40



struct keyval {
	char	*key,
			*val;
	int	flags;
};
#define KEYVAL_NULL	((struct keyval) {NULL, NULL, 0})

struct keymap {
	struct keyval	*list;
	int				listlen;
};
#define KEYMAP_NULL	((struct keymap) { NULL, 0})





int map_unsetflags (struct keymap*, char *key, int iflags, int flags);
int map_setflags (struct keymap*, char *key, int iflags, int flags);
int map_setval (struct keymap*, char *key, char *val, int flags);
int map_setorinsert (struct keymap*, char *key, char *val, int flags);
int map_isro (int *isro, struct keymap*, char *key, int flags);
int map_getflags (int *oflags, struct keymap*, char *key, int flags);
int map_getval (char **val, struct keymap*, char *key, int flags);
int map_getkeyval (struct keyval**, struct keymap*, char *key, int flags);
int map_getnum (struct keymap*);
int map_getithkeyval (struct keyval**, struct keymap*, int num);
int map_haskey (struct keymap*, char *key, int flags);
int map_remove (struct keymap*, char *key, int flags);
int map_cpyinsert (struct keymap*, char *key, char *val, int flags);
int map_insert (struct keymap*, char *key, char *val, int flags);
int map_removeall (struct keymap*);
int map_copy (struct keymap *dest, struct keymap *src, int flags);
int map_allsetflags (struct keymap*, int flags);
int map_allunsetflags (struct keymap*, int flags);



struct cfmap {
	char				*buf;		/* internally used */
	struct keymap	config,
						set;
	int				flags;
};
#define CFMAP_NULL	((struct cfmap){ NULL, {NULL,0}, {NULL,0}, 0 })

#define CFMAP_F_NOT_NULL			0x100
#define CFMAP_F_ALLOW_NONCONFIG	0x200
#define CFMAP_F_LOG_ERROR			0x400
#define CFMAP_F_IS_CONFIGURED		0x800


int cfmap_readconfig (struct cfmap*, char *fname, int flags);
int cfmap_hfree (struct cfmap*);
int cfmap_init (struct cfmap*);
int cfmap_removeempty (struct cfmap*, int flags);
int cfmap_removenull (struct cfmap*, int flags);
int cfmap_reset (struct cfmap*, int flags);
int cfmap_check (struct cfmap*, int flags);
int cfmap_setflags (struct cfmap*, int flags);
int cfmap_unsetflags (struct cfmap*, int flags);
int cfmap_config (struct cfmap*, char *key, char *val, int flags);
int cfmap_set (struct cfmap*, char *key, char *val, int flags);









#ifdef __cplusplus
} /* extern "C" */
#endif













#endif	/* _R__FRLIB_LIB_BASE_KEYMAP_H */

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
