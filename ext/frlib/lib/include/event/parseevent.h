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

#ifndef _R__FRLIB_EVENT_EVPARSE_H
#define _R__FRLIB_EVENT_EVPARSE_H


#include <fr/base/tlst.h>
#include <fr/base/tmo.h>
#include <fr/base/bufref.h>
#include <stdarg.h>
#include <stdint.h>



#ifdef __cplusplus
extern "C" {
#endif

#define EVP_F_NONE			0x00000
#define EVP_F_CPY				0x00001
#define EVP_F_CPYVAR			0x00002
#define EVP_F_INSERT			0x00004
#define EVP_F_FORCE			0x00008
#define EVP_F_WRNL			0x00010
#define EVP_F_PRTSTR			0x00100
#define EVP_F_PRTTYP			0x00200
#define EVP_F_EXIST			0x00400		/* was prior cleared */
#define EVP_F_NANO			0x00800		/* use nano sec. instead of micro sec */
#define EVP_F_NOPRTARG		0x01000
#define EVP_F_NOPRTATTR		0x02000
#define EVP_F_NOPRTVAR		0x04000
#define EVP_F_NOPRTTARGET	0x08000
#define EVP_F_PRTTIME		0x10000
#define EVP_F_OVERWRITE		0x20000		/* on ev_addattr* and ev_addvar overwrite existing, if any */


struct event {
	struct bufref	bufref;
	int				flags;
	int				hastarget;
	const char		*targetname;
	struct tlst		targetarg;
	const char		*name;
	struct tlst		arg;
	struct tlst		attr;
	struct tlst		var;		/* var's are attributes of form "var/" var "/" idx1 "/" idx2 */
};

#define EVP_T_NONE	0	/* internally only */
#define EVP_T_VOID	1
#define EVP_T_INT		2
#define EVP_T_FLOAT	3
#define EVP_T_STR		4
#define EVP_T_DATE	5	/* internally int */
#define EVP_T_TIME	6	/* internally int */
#define EVP_T_NDATE	7	/* like date, but in nanosec - only internally */
#define EVP_T_NTIME	8	/* like time, but in nanosec - only internally */
#define EVP_T_RM		9	/* pseudo type to remove a var from varlist */

#define EVP_T_ISDATE(d)	(((d)==EVP_T_DATE)||((d)==EVP_T_NDATE))
#define EVP_T_ISTIME(d)	(((d)==EVP_T_TIME)||((d)==EVP_T_NTIME))

struct ev_val {
	int	typ;
	union {
		int64_t		i;
		double		f;
		const char	*s;
		tmo_t			d;		/* for date & time */
	};
};

struct ev_attr {
	const char		*var;
	struct ev_val	val;
	int				flags;
};

struct ev_arg {
	struct ev_val	arg;
	int				flags;
};

struct ev_var {
	const char		*var;
	int				idx1, idx2;
	struct ev_val	val;
	int				flags;
};




int ev_addbuf (struct event*, char *buf);
int ev_addbuf2 (struct event*, char *buf, size_t blen);
int ev_addbufref (struct event*, char *buf);
int ev_rmbuf (struct event*, char *buf);

int ev_new (struct event*);
int ev_free (struct event*);
int ev_clear (struct event*);
/* note: before calling ev_copy, ev_new must be called on dest */
int ev_copy (struct event *dest, struct event *src);

int ev_parse (struct event*, char *evstr, int flags);
int ev_cparse (struct event*, const char *evstr, int flags);

int ev_setname (struct event*, const char *name, int flags);
int ev_setnamef (struct event*, const char *fmt, ...)
						__attribute__((format(printf, 2, 3)));
int ev_vsetnamef (struct event*, const char *fmt, va_list ap);
int ev_settname (struct event*, const char *name, int flags);
int ev_settnamef (struct event*, const char *fmt, ...)
						__attribute__((format(printf, 2, 3)));
int ev_vsettnamef (struct event*, const char *fmt, va_list ap);

int ev_addattr_s (struct event*, const char *var, const char *val,int flags);
int ev_addattr_i (struct event*, const char *var, int64_t val,int flags);
int ev_addattr_d (struct event*, const char *var, tmo_t val,int flags);
int ev_addattr_t (struct event*, const char *var, tmo_t val,int flags);
int ev_addattr_f (struct event*, const char *var, double val,int flags);
int ev_addattr_v (struct event*, const char *var, struct ev_val val,int flags);
int ev_addattrf (struct event*, int flags, const char *var,
						const char *valfmt, ...)
						__attribute__((format(printf, 4, 5)));
int ev_vaddattrf (struct event*, int flags, const char *var, const char *valfmt,
						va_list ap);


int ev_addvar_s (	struct event*, const char *var, int idx1, int idx2,
						const char *val, int flags);
int ev_addvar_i (	struct event*, const char *var, int idx1, int idx2,
						int64_t val, int flags);
int ev_addvar_d (	struct event*, const char *var, int idx1, int idx2,
						tmo_t val, int flags);
int ev_addvar_t (	struct event*, const char *var, int idx1, int idx2,
						tmo_t val, int flags);
int ev_addvar_f (	struct event*, const char *var, int idx1, int idx2,
						double val, int flags);
int ev_addvar_v (	struct event*, const char *var, int idx1, int idx2,
						struct ev_val val, int flags);
int ev_addvarf (	struct event*, int flags, const char *var, int idx1, int idx2,
						const char *valfmt, ...)
						__attribute__((format(printf, 6, 7)));
int ev_vaddvarf (	struct event*, int flags, const char *var, int idx1, int idx2,
						const char *valfmt, va_list ap);
int ev_addvar_rm (struct event*, const char *var, int idx1, int idx2, int flags);

int ev_setarg_s (struct event*, const char *arg, int pos, int flags);
int ev_setarg_i (struct event*, int64_t arg, int pos, int flags);
int ev_setarg_d (struct event*, tmo_t arg, int pos, int flags);
int ev_setarg_t (struct event*, tmo_t arg, int pos, int flags);
int ev_setarg_f (struct event*, double arg, int pos, int flags);
int ev_setarg_v (struct event*, struct ev_val arg, int pos, int flags);
int ev_setargf (struct event*, int pos, int flags, const char *argfmt, ...)
						__attribute__((format(printf, 4, 5)));
int ev_vsetargf (struct event*, int pos, int flags, const char *argfmt,
							va_list ap);

int ev_settarg_s (struct event*, const char *arg, int pos, int flags);
int ev_settarg_i (struct event*, int64_t arg, int pos, int flags);
int ev_settarg_d (struct event*, tmo_t arg, int pos, int flags);
int ev_settarg_t (struct event*, tmo_t arg, int pos, int flags);
int ev_settarg_f (struct event*, double arg, int pos, int flags);
int ev_settarg_v (struct event*, struct ev_val arg, int pos, int flags);
int ev_settargf (struct event*, int pos, int flags, const char *argfmt, ...)
						__attribute__((format(printf, 4, 5)));
int ev_vsettargf (struct event*, int pos, int flags, const char *argfmt,
						va_list ap);


int ev_getname (const char **out, struct event*);
int ev_gettname (const char **out, struct event*);

int ev_numarg (struct event*);
int ev_numtarg (struct event*);
int ev_numattr (struct event*);
int ev_numvar (struct event*);

int ev_typeofarg (struct event*, int pos);
int ev_typeoftarg (struct event*, int pos);
int ev_typeofattrpos (struct event*, int pos);
int ev_typeofattr (struct event*, const char *var);
int ev_typeofvarpos (struct event*, int pos);
int ev_typeofvar (struct event*, const char *var, int idx1, int idx2);

/* the following differentiate between EVP_T_DATE and EVP_T_NDATE
 * as well as EVP_T_TIME and EVP_T_NTIME
 */
int ev_ntypeofarg (struct event*, int pos);
int ev_ntypeoftarg (struct event*, int pos);
int ev_ntypeofattrpos (struct event*, int pos);
int ev_ntypeofattr (struct event*, const char *var);
int ev_ntypeofvarpos (struct event*, int pos);
int ev_ntypeofvar (struct event*, const char *var, int idx1, int idx2);

/* the following do return 1 if date/time is in nano sec, otherwise 0 */
int ev_isnanoarg (struct event*, int pos);
int ev_isnanotarg (struct event*, int pos);
int ev_isnanoattrpos (struct event*, int pos);
int ev_isnanoattr (struct event*, const char *var);
int ev_isnanovarpos (struct event*, int pos);
int ev_isnanovar (struct event*, const char *var, int idx1, int idx2);

int ev_getarg_s (const char **out, struct event*, int pos);
int ev_getarg_sr (char **out, struct event*, int pos);
int ev_getarg_i (int64_t *out, struct event*, int pos);
int ev_getarg_in (int *out, struct event*, int pos);
int ev_getarg_d (tmo_t *out, struct event*, int pos);
int ev_getarg_t (tmo_t *out, struct event*, int pos);
int ev_getarg_f (double *out, struct event*, int pos);
int ev_getarg_v (struct ev_val *out, struct event*, int pos);

int ev_gettarg_s (const char **out, struct event*, int pos);
int ev_gettarg_sr (char **out, struct event*, int pos);
int ev_gettarg_i (int64_t *out, struct event*, int pos);
int ev_gettarg_in (int *out, struct event*, int pos);
int ev_gettarg_d (tmo_t *out, struct event*, int pos);
int ev_gettarg_t (tmo_t *out, struct event*, int pos);
int ev_gettarg_f (double *out, struct event*, int pos);
int ev_gettarg_v (struct ev_val *out, struct event*, int pos);

int ev_getattrpos_s (const char **outvar, const char **outval, struct event*, int pos);
int ev_getattrpos_sr (const char **outvar, char **outval, struct event*, int pos);
int ev_getattrpos_i (const char **outvar, int64_t *outval, struct event*, int pos);
int ev_getattrpos_in (const char **outvar, int *outval, struct event*, int pos);
int ev_getattrpos_d (const char **outvar, tmo_t *outval, struct event*, int pos);
int ev_getattrpos_t (const char **outvar, tmo_t *outval, struct event*, int pos);
int ev_getattrpos_f (const char **outvar, double *outval, struct event*, int pos);
int ev_getattrpos_v (const char **outvar, struct ev_val *outval, struct event*, int pos);

int ev_getattr_s (const char **outval, struct event*, const char *var);
int ev_getattr_sr (char **outval, struct event*, const char *var);
int ev_getattr_i (int64_t *outval, struct event*, const char *var);
int ev_getattr_in (int *outval, struct event*, const char *var);
int ev_getattr_d (tmo_t *outval, struct event*, const char *var);
int ev_getattr_t (tmo_t *outval, struct event*, const char *var);
int ev_getattr_f (double *outval, struct event*, const char *var);
int ev_getattr_v (struct ev_val *outval, struct event*, const char *var);

int ev_getvarpos_s (	const char **outvar, int *idx1, int *idx2,
							const char **outval, struct event*, int pos);
int ev_getvarpos_sr (const char **outvar, int *idx1, int *idx2,
							char **outval, struct event*, int pos);
int ev_getvarpos_i (	const char **outvar, int *idx1, int *idx2,
							int64_t *outval, struct event*, int pos);
int ev_getvarpos_in (const char **outvar, int *idx1, int *idx2,
							int *outval, struct event*, int pos);
int ev_getvarpos_d (	const char **outvar, int *idx1, int *idx2,
							tmo_t *outval, struct event*, int pos);
int ev_getvarpos_t (	const char **outvar, int *idx1, int *idx2,
							tmo_t *outval, struct event*, int pos);
int ev_getvarpos_f (	const char **outvar, int *idx1, int *idx2,
							double *outval, struct event*, int pos);
int ev_getvarpos_v (	const char **outvar, int *idx1, int *idx2,
							struct ev_val *outval, struct event*, int pos);

int ev_getvar_s (const char **outval, struct event*, const char *var, int idx1, int idx2);
int ev_getvar_sr (char **outval, struct event*, const char *var, int idx1, int idx2);
int ev_getvar_i (int64_t *outval, struct event*, const char *var, int idx1, int idx2);
int ev_getvar_in (int *outval, struct event*, const char *var, int idx1, int idx2);
int ev_getvar_d (tmo_t *outval, struct event*, const char *var, int idx1, int idx2);
int ev_getvar_t (tmo_t *outval, struct event*, const char *var, int idx1, int idx2);
int ev_getvar_f (double *outval, struct event*, const char *var, int idx1, int idx2);
int ev_getvar_v (struct ev_val *outval, struct event*, const char *var, int idx1, int idx2);

int ev_getvarptr (struct ev_var **out, struct event *ev, const char *var, int idx1, int idx2);
int ev_getvarptr2 (struct ev_var**, struct event*, const char *var, int idx1, int idx2);
int ev_getvarptrpos (struct ev_var**, struct event*, int pos);

int ev_hasvar (struct event *ev, const char *var, int idx1, int idx2);

int ev_rmarg (struct event*, int pos);
int ev_rmtarg (struct event*, int pos);
int ev_rmattr (struct event*, const char *var);
int ev_rmattrpos (struct event*, int pos);
int ev_rmvar (struct event*, const char *var, int idx1, int idx2);
int ev_rmvarpos (struct event*, int pos);

int ev_getoutsize (struct event*, int flags);
int ev_writeout (char *outstr, struct event*, int flags);
int ev_create (char **outstr, struct event*, int flags);

int ev_prtparsed (struct event*, int fd, int flags);










#ifdef __cplusplus
}	/* extern "C" */
#endif



















#endif	/* _R__FRLIB_EVENT_EVPARSE_H */

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
