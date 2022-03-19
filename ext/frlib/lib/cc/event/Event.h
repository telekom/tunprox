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

#ifndef _R__FRLIB_LIB_CC_EVENT_EVENT_H
#define _R__FRLIB_LIB_CC_EVENT_EVENT_H




#include <fr/event/parseevent.h>
#include <stdlib.h>
#include <stdarg.h>


#if 0	/* for information only, defined in fr/event/parseevent.h */
#define EVP_F_NONE		0x00
#define EVP_F_FREE		0x01
#define EVP_F_CPY			0x02
#define EVP_F_INSERT		0x04
#define EVP_F_FORCE		0x08
#define EVP_F_WRNL		0x10
#endif





class FREvent {
public:
	struct event	ev;

	FREvent() { ev_new (&ev); };
	~FREvent() { ev_free (&ev); };

	/* parse event string */
	int parse (char *evstr, int flags=0)
	{
		return ev_parse (&ev, evstr, flags);
	};

	/* set or modify parts of the event */
	int setName (const char *name, int flags=0)
	{
		return ev_setname (&ev,name, flags);
	};
	int setTName (const char *name, int flags=0) 
	{
		return ev_settname (&ev,name, flags);
	};
	/* set arguments */
	int setArg (const char *arg, int pos, int flags=0)
	{
		return ev_setarg_s (&ev, arg, pos, flags);
	};
	int setArg (int64_t arg, int pos, int flags=0)
	{
		return ev_setarg_i (&ev, arg, pos, flags);
	};
	int setArg (double arg, int pos, int flags=0)
	{
		return ev_setarg_f (&ev, arg, pos, flags);
	};
	int setArgDate (tmo_t arg, int pos, int flags=0)
	{
		return ev_setarg_d (&ev, arg, pos, flags);
	};
	int setArgTime (tmo_t arg, int pos, int flags=0)
	{
		return ev_setarg_t (&ev, arg, pos, flags);
	};
	/* set target arguments */
	int setTArg (const char *arg, int pos, int flags=0)
	{
		return ev_settarg_s (&ev, arg, pos, flags);
	};
	int setTArg (int64_t arg, int pos, int flags=0)
	{
		return ev_settarg_i (&ev, arg, pos, flags);
	};
	int setTArg (double arg, int pos, int flags=0)
	{
		return ev_settarg_f (&ev, arg, pos, flags);
	};
	int setTArgDate (tmo_t arg, int pos, int flags=0)
	{
		return ev_settarg_d (&ev, arg, pos, flags);
	};
	int setTArgTime (tmo_t arg, int pos, int flags=0)
	{
		return ev_settarg_t (&ev, arg, pos, flags);
	};
	/* set attributes */
	int addAttr (const char *var, char *val, int flags=0)
	{
		return ev_addattr_s (&ev, var, val, flags);
	};
	int addAttr (const char *var, int64_t val, int flags=0)
	{
		return ev_addattr_i (&ev, var, val, flags);
	};
	int addAttr (const char *var, double val, int flags=0)
	{
		return ev_addattr_f (&ev, var, val, flags);
	};
	int addAttrDate (const char *var, tmo_t val, int flags=0)
	{
		return ev_addattr_d (&ev, var, val, flags);
	};
	int addAttrTime (const char *var, tmo_t val, int flags=0)
	{
		return ev_addattr_t (&ev, var, val, flags);
	};

	/* set variables */
	int addVar (const char *var, int idx1, int idx2, const char *val, int flags=0)
	{
		return ev_addvar_s (&ev, var, idx1, idx2, val, flags);
	};
	int addVar (const char *var, int idx1, int idx2, int64_t val, int flags=0)
	{
		return ev_addvar_i (&ev, var, idx1, idx2, val, flags);
	};
	int addVar (const char *var, int idx1, int idx2, double val, int flags=0)
	{
		return ev_addvar_f (&ev, var, idx1, idx2, val, flags);
	};
	int addVarDate (const char *var, int idx1, int idx2, tmo_t val, int flags=0)
	{
		return ev_addvar_d (&ev, var, idx1, idx2, val, flags);
	};
	int addVarTime (const char *var, int idx1, int idx2, tmo_t val, int flags=0)
	{
		return ev_addvar_t (&ev, var, idx1, idx2, val, flags);
	};

	int addVar (const char *var, int idx1, char *val, int flags=0)
	{
		return ev_addvar_s (&ev, var, idx1, -1, val, flags);
	};
	int addVar (const char *var, int idx1, int64_t val, int flags=0)
	{
		return ev_addvar_i (&ev, var, idx1, -1, val, flags);
	};
	int addVar (const char *var, int idx1, double val, int flags=0)
	{
		return ev_addvar_f (&ev, var, idx1, -1, val, flags);
	};
	int addVarDate (const char *var, int idx1, tmo_t val, int flags=0)
	{
		return ev_addvar_d (&ev, var, idx1, -1, val, flags);
	};
	int addVarTime (const char *var, int idx1, tmo_t val, int flags=0)
	{
		return ev_addvar_t (&ev, var, idx1, -1, val, flags);
	};

	int addVar (const char *var, char *val, int flags=0)
	{
		return ev_addvar_s (&ev, var, -1, -1, val, flags);
	};
	int addVar (const char *var, int64_t val, int flags=0)
	{
		return ev_addvar_i (&ev, var, -1, -1, val, flags);
	};
	int addVar (const char *var, double val, int flags=0)
	{
		return ev_addvar_f (&ev, var, -1, -1, val, flags);
	};
	int addVarDate (const char *var, tmo_t val, int flags=0)
	{
		return ev_addvar_d (&ev, var, -1, -1, val, flags);
	};
	int addVarTime (const char *var, tmo_t val, int flags=0)
	{
		return ev_addvar_t (&ev, var, -1, -1, val, flags);
	};

	/* setf functions */
	int vSetArgf (int pos, int flags, const char *argfmt, va_list ap)
	{
		return ev_vsetargf (&ev, pos, flags, argfmt, ap);
	};
	int vSetTArgf (int pos, int flags, const char *argfmt, va_list ap)
	{
		return ev_vsettargf (&ev, pos, flags, argfmt, ap);
	};
	int vAddAttrf (int flags, const char *var, const char *valfmt, va_list ap)
	{
		return ev_vaddattrf (&ev, flags, var, valfmt, ap);
	};
	int setArgf (int pos, int flags, const char *argfmt, ...)
						__attribute__((format(printf, 4, 5)))
	{
		int		ret;
		va_list	ap;

		va_start (ap, argfmt);
		ret = ev_vsetargf (&ev, pos, flags, argfmt, ap);
		va_end (ap);
		return ret;
	};
	int setTArgf (int pos, int flags, const char *argfmt, ...)
						__attribute__((format(printf, 4, 5)))
	{
		int		ret;
		va_list	ap;

		va_start (ap, argfmt);
		ret = ev_vsettargf (&ev, pos, flags, argfmt, ap);
		va_end (ap);
		return ret;
	};
	int addAttrf (int flags, const char *var, const char *valfmt, ...)
						__attribute__((format(printf, 4, 5)))
	{
		int		ret;
		va_list	ap;

		va_start (ap, valfmt);
		ret = ev_vaddattrf (&ev, flags, var, valfmt, ap);
		va_end (ap);
		return ret;
	};


	/* get info */
	int getName (const char **oName)
	{
		return ev_getname (oName, &ev);
	};
	int getTName (const char **oName)
	{
		return ev_gettname (oName, &ev);
	};

	int numArg ()
	{
		return ev_numarg (&ev);
	};
	int numTArg ()
	{
		return ev_numtarg (&ev);
	};
	int numAttr ()
	{
		return ev_numattr (&ev);
	};
	int numVar ()
	{
		return ev_numvar (&ev);
	};

	int typeofArg (int pos)
	{
		return ev_typeofarg (&ev, pos);
	};
	int typeofTArg (int pos)
	{
		return ev_typeoftarg (&ev, pos);
	};
	int typeofAttr (char *var)
	{
		return ev_typeofattr (&ev, var);
	};
	int typeofAttrPos (int pos)
	{
		return ev_typeofattrpos (&ev, pos);
	};
	int typeofVar (const char *var, int idx1=-1, int idx2=-1)
	{
		return ev_typeofvar (&ev, var, idx1, idx2);
	};
	int typeofVarPos (int pos)
	{
		return ev_typeofvarpos (&ev, pos);
	};

	/* the following differentiate between EVP_T_DATE and EVP_T_NDATE
	 * as well es EVP_T_TIME and EVP_T_NTIME
	 */
	int nTypeofArg (int pos)
	{
		return ev_ntypeofarg (&ev, pos);
	};
	int nTypeofTArg (int pos)
	{
		return ev_ntypeoftarg (&ev, pos);
	};
	int nTypeofAttr (const char *var)
	{
		return ev_ntypeofattr (&ev, var);
	};
	int nTypeofAttrPos (int pos)
	{
		return ev_ntypeofattrpos (&ev, pos);
	};
	int nTypeofVar (const char *var, int idx1=-1, int idx2=-1)
	{
		return ev_ntypeofvar (&ev, var, idx1, idx2);
	};
	int nTypeofVarPos (int pos)
	{
		return ev_ntypeofvarpos (&ev, pos);
	};

	/* the following do return 1 if date/time is in nano sec, otherwise 0 */
	int isNanoArg (int pos)
	{
		return ev_isnanoarg (&ev, pos);
	};
	int isNanoTArg (int pos)
	{
		return ev_isnanotarg (&ev, pos);
	};
	int isNanoAttr (const char *var)
	{
		return ev_isnanoattr (&ev, var);
	};
	int isNanoAttrPos (int pos)
	{
		return ev_isnanoattrpos (&ev, pos);
	};
	int isNanoVar (const char *var, int idx1=-1, int idx2=-1)
	{
		return ev_isnanovar (&ev, var, idx1, idx2);
	};
	int isNanoVarPos (int pos)
	{
		return ev_isnanovarpos (&ev, pos);
	};

	/* get argument functions */
	int getArg (const char **arg, int pos)
	{
		return ev_getarg_s (arg, &ev, pos);
	};
	int getArg (int64_t *arg, int pos)
	{
		return ev_getarg_i (arg, &ev, pos);
	};
	int getArg (double *arg, int pos)
	{
		return ev_getarg_f (arg, &ev, pos);
	};
	int getArgDate (tmo_t *arg, int pos)
	{
		return ev_getarg_d (arg, &ev, pos);
	};
	int getArgTime (tmo_t *arg, int pos)
	{
		return ev_getarg_t (arg, &ev, pos);
	};

	/* get target argument */
	int getTArg (const char **arg, int pos)
	{
		return ev_gettarg_s (arg, &ev, pos);
	};
	int getTArg (int64_t *arg, int pos)
	{
		return ev_gettarg_i (arg, &ev, pos);
	};
	int getTArg (double *arg, int pos)
	{
		return ev_gettarg_f (arg, &ev, pos);
	};
	int getTArgDate (tmo_t *arg, int pos)
	{
		return ev_gettarg_d (arg, &ev, pos);
	};
	int getTArgTime (tmo_t *arg, int pos)
	{
		return ev_gettarg_t (arg, &ev, pos);
	};

	/* get attribute functions */
	int getAttr (const char **val, const char *var)
	{
		return ev_getattr_s (val, &ev, var);
	};
	int getAttr (int64_t *val, const char *var)
	{
		return ev_getattr_i (val, &ev, var);
	};
	int getAttr (double *val, const char *var)
	{
		return ev_getattr_f (val, &ev, var);
	};
	int getAttrDate (tmo_t *val, const char *var)
	{
		return ev_getattr_d (val, &ev, var);
	};
	int getAttrTime (tmo_t *val, const char *var)
	{
		return ev_getattr_t (val, &ev, var);
	};

	/* get attribute at position */
	int getAttrPos (const char **var, const char **val, int pos)
	{
		return ev_getattrpos_s (var, val, &ev, pos);
	};
	int getAttrPos (const char **var, int64_t *val, int pos)
	{
		return ev_getattrpos_i (var, val, &ev, pos);
	};
	int getAttrPos (const char **var, double *val, int pos)
	{
		return ev_getattrpos_f (var, val, &ev, pos);
	};
	int getAttrPosDate (const char **var, tmo_t *val, int pos)
	{
		return ev_getattrpos_d (var, val, &ev, pos);
	};
	int getAttrPosTime (const char **var, tmo_t *val, int pos)
	{
		return ev_getattrpos_t (var, val, &ev, pos);
	};

	/* get variable functions */
	int getVar (const char **val, const char *var, int idx1=-1, int idx2=-1)
	{
		return ev_getvar_s (val, &ev, var, idx1, idx2);
	};
	int getVar (int64_t *val, const char *var, int idx1=-1, int idx2=-1)
	{
		return ev_getvar_i (val, &ev, var, idx1, idx2);
	};
	int getVar (double *val, const char *var, int idx1=-1, int idx2=-1)
	{
		return ev_getvar_f (val, &ev, var, idx1, idx2);
	};
	int getVarDate (tmo_t *val, const char *var, int idx1=-1, int idx2=-1)
	{
		return ev_getvar_d (val, &ev, var, idx1, idx2);
	};
	int getVarTime (tmo_t *val, const char *var, int idx1=-1, int idx2=-1)
	{
		return ev_getvar_t (val, &ev, var, idx1, idx2);
	};

	/* get variable at position */
	int getVarPos (const char **var, int *idx1, int *idx2, const char **val, int pos)
	{
		return ev_getvarpos_s (var, idx1, idx2, val, &ev, pos);
	};
	int getVarPos (const char **var, int *idx1, int *idx2, int64_t *val, int pos)
	{
		return ev_getvarpos_i (var, idx1, idx2, val, &ev, pos);
	};
	int getVarPos (const char **var, int *idx1, int *idx2, double *val, int pos)
	{
		return ev_getvarpos_f (var, idx1, idx2, val, &ev, pos);
	};
	int getVarPosDate (const char **var, int *idx1, int *idx2, tmo_t *val, int pos)
	{
		return ev_getvarpos_d (var, idx1, idx2, val, &ev, pos);
	};
	int getVarPosTime (const char **var, int *idx1, int *idx2, tmo_t *val, int pos)
	{
		return ev_getvarpos_t (var, idx1, idx2, val, &ev, pos);
	};

	int getVarPos (const char **var, int *idx1, const char **val, int pos)
	{
		return ev_getvarpos_s (var, idx1, NULL, val, &ev, pos);
	};
	int getVarPos (const char **var, int *idx1, int64_t *val, int pos)
	{
		return ev_getvarpos_i (var, idx1, NULL, val, &ev, pos);
	};
	int getVarPos (const char **var, int *idx1, double *val, int pos)
	{
		return ev_getvarpos_f (var, idx1, NULL, val, &ev, pos);
	};
	int getVarPosDate (const char **var, int *idx1, tmo_t *val, int pos)
	{
		return ev_getvarpos_d (var, idx1, NULL, val, &ev, pos);
	};
	int getVarPosTime (const char **var, int *idx1, tmo_t *val, int pos)
	{
		return ev_getvarpos_t (var, idx1, NULL, val, &ev, pos);
	};

	int getVarPos (const char **var, const char **val, int pos)
	{
		return ev_getvarpos_s (var, NULL, NULL, val, &ev, pos);
	};
	int getVarPos (const char **var, int64_t *val, int pos)
	{
		return ev_getvarpos_i (var, NULL, NULL, val, &ev, pos);
	};
	int getVarPos (const char **var, double *val, int pos)
	{
		return ev_getvarpos_f (var, NULL, NULL, val, &ev, pos);
	};
	int getVarPosDate (const char **var, tmo_t *val, int pos)
	{
		return ev_getvarpos_d (var, NULL, NULL, val, &ev, pos);
	};
	int getVarPosTime (const char **var, tmo_t *val, int pos)
	{
		return ev_getvarpos_t (var, NULL, NULL, val, &ev, pos);
	};

	/* remove parts */
	int rmArg (int pos)
	{
		return ev_rmarg (&ev, pos);
	};
	int rmTArg (int pos)
	{
		return ev_rmtarg (&ev, pos);
	};
	int rmAttr (const char *var)
	{
		return ev_rmattr (&ev, var);
	};
	int rmAttrPos (int pos)
	{
		return ev_rmattrpos (&ev, pos);
	};
	int rmVar (const char *var, int idx1=-1, int idx2=-1)
	{
		return ev_rmvar (&ev, var, idx1, idx2);
	};
	int rmVarPos (int pos)
	{
		return ev_rmvarpos (&ev, pos);
	};

	/* create event string */
	int getWriteSize (int flags=0)
	{
		return ev_getoutsize (&ev, flags);
	};
	int writeOut (char *outbuf, int flags=0)
	{
		return ev_writeout (outbuf, &ev, flags);
	};
	int create (char **outstr, int flags=0)
	{
		return ev_create (outstr, &ev, flags);
	};

	/* for debugging purpose only */
	int prtParsed (int fd=1, int flags=0)
	{
		return ev_prtparsed (&ev, fd, flags);
	};
};





































#endif	/* _R__FRLIB_LIB_CC_EVENT_EVENT_H */

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
