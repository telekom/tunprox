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

#ifndef _R__FRLIB_LIB_BASE_CONFIG_H
#define _R__FRLIB_LIB_BASE_CONFIG_H


#ifdef __cplusplus
extern "C" {
#endif


#include <sys/types.h>
#include <ctype.h>
#include <stdarg.h>
#include <fr/base/tmo.h>


#define CF_F_NONE			0x00
#define CF_F_USCORE		0x01
#define CF_F_NOLOCAL		0x02
#define CF_F_NOHOME		0x04
#define CF_F_NOGLOBAL	0x08
#define CF_F_NOENV		0x10


/* for backward compatibility */
#define CF_FLAG_DEF_NONE		CF_F_NONE
#define CF_FLAG_DEF_USCORE		CF_F_USCORE
#define CF_FLAG_NOLOCAL_CF		CF_F_NOLOCAL
#define CF_FLAG_NOHOME_CF		CF_F_NOHOME
#define CF_FLAG_NOGLOBAL_CF	CF_F_NOGLOBAL


#define CFENTRY_F_NONE		0x00
#define CFENTRY_F_NEEDFREE	0x01
#define CFENTRY_F_ISDIR		0x02

struct cf_cventry {
	int			father;
	const char	*var;
	const char	*idx;
	union {
		char		*val;
		int		d_num;
	};
	int			flags;
};

struct cf_cvlist {
	struct cf_cventry	*list;
	int					listsize;
	char					**bufs;
	int					numbufs;
	int					maxnum;
};

#define	CF_CVL_NULL	((struct cf_cvlist){.list = NULL})

struct cf_ns {
	struct cf_cvlist	**cv_lists;
	int					num_cv_lists;
	struct cf_cvlist	all_cvs;
	struct cf_cvlist	*early_list;
};

#define CF_NS_NULL ((struct cf_ns) {.cv_lists = NULL})


typedef	int	(*cf_reread_t) ();
typedef	int	(*cf_reread2_t) (void*);

#define CF_MAY_READ	{\
	if (!config_read) read_config(); \
	cf_mayread (); \
	}
#define CF_MAYREAD CF_MAY_READ


int cf_default_cfname (const char*, int flags);
const char* cf_get_defcfname ();
int cf_set_cfname (const char*);
const char* cf_get_cfname ();
const char* cf_get_real_cfname ();
int cf_read ();
int cf_mayread ();
int cf_reread ();
int cf_hup_reread ();
int cf_parse (char *buf, struct cf_cvlist*);
int cf_register_cv_list (struct cf_cvlist *);
int cf_deregister_cv_list (struct cf_cvlist *);
int cf_hfree_cvlist (struct cf_cvlist *list, int freebufs);
int cf_free_config ();
void cf_free ();

int cfn_parse (struct cf_ns*, char *buf, struct cf_cvlist*);
int cfn_read (struct cf_ns*, const char *fname);
int cfn_read_buf (struct cf_ns *ns, char*buf);
int cfn_register_cv_list (struct cf_ns*, struct cf_cvlist *);
int cfn_deregister_cv_list (struct cf_ns*, struct cf_cvlist *);
int cfn_hfree_cvlist (struct cf_ns*, struct cf_cvlist *list, int freebufs);
void cfn_free (struct cf_ns*);



int cf_begin_read ();
int cf_end_read ();
int cf_end_read_cb (cf_reread_t);
int cf_register_reread_callback (cf_reread_t);
int cf_deregister_reread_callback (cf_reread_t);
int cf_end_read_cb2 (cf_reread2_t, void*);
int cf_register_reread_callback2 (cf_reread2_t, void*);
int cf_deregister_reread_callback2 (cf_reread2_t, void*);



#ifdef CF_OLD_BINDING
#define get_config		cf_getval
#define get_config2		cf_getval2
#define get_confarr		cf_getarr
#define get_confarr2	cf_getarr2
#define isyes			cf_isyes
#endif	/* CF_OLD_BINDING */



const char* cf_getval (const char *var);
const char* cf_getval2 (const char *var, const char *defvalue);

const char* cf_getvar (const char *var);
const char* cf_getvar2 (const char *var, const char *defvalue);
const char* cf_getvarf (const char *fmt, ...)
			  			__attribute__((format(printf, 1, 2)));
const char* cf_vgetvarf (const char *fmt, va_list ap);
const char* cf_getvarf2 (const char *defval, const char *fmt, ...)
			  			__attribute__((format(printf, 2, 3)));
const char* cf_vgetvarf2 (const char *defval, const char *fmt, va_list ap);


int cf_getnumarr (const char *var);
const char* cf_getarr (const char *var, const char *idx);
const char* cf_getarr2 (const char *var, const char *idx, const char *defvalue);
const char* cf_getarrnodef (const char *var, const char *idx);

const char* cf_getarri (const char *var, int idx);
const char* cf_getarri2 (const char *var, int idx, const char *defvalue);

const char* cf_getarrx (const char *var, int x);
const char* cf_getarrxwi (const char *var, int x, const char **idx);
const char* cf_getmarr (const char *var, int numidx, ...);
const char* cf_getmarr2 (const char *var, const char *defval, int numidx, ...);
const char* cf_vgetmarr (const char *var, int numidx, va_list ap);
const char* cf_vgetmarr2 (const char *var, const char *defval, int numidx, va_list ap);
const char* cf_get2arr (const char *var, const char *idx1, const char *idx2);
const char* cf_get2arr2 (const char *var, const char *idx1, const char *idx2, const char *defval);

int cf_getdirnum (const char *dir);
/* val is set only when isdir == 0 */
int cf_getdirentry (const char *dir, int num, const char **var, const char **idx, int *isdir, const char **val);


/* get functions with namespace */
const char* cfn_getval (struct cf_ns*, const char *var);
const char* cfn_getval2 (struct cf_ns*, const char *var, const char *defvalue);

const char* cfn_getvar (struct cf_ns*, const char *var);
const char* cfn_getvar2 (struct cf_ns*, const char *var, const char *defvalue);
const char* cfn_getvarf (struct cf_ns*, const char *fmt, ...)
			  			__attribute__((format(printf, 2, 3)));
const char* cfn_vgetvarf (struct cf_ns*, const char *fmt, va_list ap);
const char* cfn_getvarf2 (struct cf_ns*, const char *defval, const char *fmt, ...)
			  			__attribute__((format(printf, 3, 4)));
const char* cfn_vgetvarf2 (struct cf_ns*, const char *defval, const char *fmt, va_list ap);


int cfn_getnumarr (struct cf_ns*, const char *var);
const char* cfn_getarr (struct cf_ns*, const char *var, const char *idx);
const char* cfn_getarr2 (struct cf_ns*, const char *var, const char *idx, const char *defvalue);
const char* cfn_getarrnodef (struct cf_ns*, const char *var, const char *idx);

const char* cfn_getarri (struct cf_ns*, const char *var, int idx);
const char* cfn_getarri2 (struct cf_ns*, const char *var, int idx, const char *defvalue);

const char* cfn_getarrx (struct cf_ns*, const char *var, int x);
const char* cfn_getarrxwi (struct cf_ns*, const char *var, int x, const char **idx);
const char* cfn_getmarr (struct cf_ns*, const char *var, int numidx, ...);
const char* cfn_getmarr2 (struct cf_ns*, const char *var, const char *defval, int numidx, ...);
const char* cfn_vgetmarr (struct cf_ns*, const char *var, int numidx, va_list ap);
const char* cfn_vgetmarr2 (struct cf_ns*, const char *var, const char *defval, int numidx, va_list ap);
const char* cfn_get2arr (struct cf_ns*, const char *var, const char *idx1, const char *idx2);
const char* cfn_get2arr2 (struct cf_ns*, const char *var, const char *idx1, const char *idx2, const char *defval);

int cfn_getdirnum (struct cf_ns*, const char *dir);
/* val is set only when isdir == 0 */
int cfn_getdirentry (struct cf_ns*, const char *dir, int num, const char **var, const char **idx, int *isdir, const char **val);



int cf_isyes (const char * val);
int cf_atoi (const char *str);
tmo_t cf_atotm (const char *str);


#define CFPS_T_DOUBLE		0x00	/* double quote */
#define CFPS_T_SINGLE		0x01	/* single quote */
#define CFPS_T_BRACE			0x02	/* brace quoting */
#define CFPS_T_DOUBLEZERO	0x03	/* double quote with zero (\0) */
#define CFPS_T_BBRACE		0x04	/* brace quoting with @-beginning */
#define CFPS_TYPEMASK		0x07
#define CFPS_F_DOLLAR		0x08	/* accept variable access with dollar - only for double quote */
#define CFPS_F_COPY			0x10	/* alloc outstring, don't modify original string */

int cf_parse_string (char *str, int type);
int cf_parse_string2 (char **outstr, char *str, int type);
int cfn_parse_string (struct cf_ns*, char *str, int type);
int cfn_parse_string2 (struct cf_ns*, char **outstr, char *str, int type);
int cf_find_endquote (char **endstr, const char *str, int type);
int cf_parse_table (char *str, int32_t **table);

int cf_print_table (int32_t ** tab);
int cf_print_config ();
int cfn_print_config (struct cf_ns*);
int cf_print_varlist (struct cf_cventry*);














#ifdef __cplusplus
} /* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_BASE_CONFIG_H */


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
