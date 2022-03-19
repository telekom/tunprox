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

#ifndef _R__FRLIB_LIB_EVENT_GVARLIST_H
#define _R__FRLIB_LIB_EVENT_GVARLIST_H

#include <sys/types.h>
#include <regex.h>


#ifdef __cplusplus
extern "C" {
#endif

/* the following flags are passed to the functions below,
 *	but the search function
 */

#define GVL_F_NONE		0x00
#define GVL_F_CPY			0x01
#define GVL_F_FREE		0x02
#define GVL_F_INTEXT		0x04
#define GVL_F_APPEND		0x08	/* gvl_setrootpath only */


/* the following flags/types are used inside the gvar struct */

#define GVAR_F_FREEVAR			0x01
#define GVAR_F_FREENS			0x02
#define GVAR_F_FREEFULLNAME	0x04
#define GVAR_F_INTEXT			0x08

#define GVAR_T_NONE		0
#define GVAR_T_EVVAR		1
#define GVAR_T_EXT		2
#define GVAR_T_INTEXT	(GVAR_T_EVVAR|GVAR_T_EXT)


struct gvar {
	char	*ns;		/* namespace */
	char	*var;
	char	*fullname;	/* note: not always set */
	char	*noroot;		/* full name without root path */
	int	refcnt;
	int	typ;
	int	id;
	void	*arg;
	int	flags;
};



/* the following flags are used for the search function only */

#define GVL_SEARCH_F_MAYMOD		0x001
#define GVL_SEARCH_F_GLOB			0x002
#define GVL_SEARCH_F_MAYGLOB		GVL_SEARCH_F_GLOB		/* to be done ... */
#define GVL_SEARCH_F_MATCHNEXT	0x008	/* match the next in order */
#define GVL_SEARCH_F_MATCHMORE	0x010	/* more searches will follow */
#define GVL_SEARCH_F_CHECKONLY	0x020
#define GVL_SEARCH_F_UNIQ			0x040
#define GVL_SEARCH_F_CLEANUP		0x080
#define GVL_SEARCH_F_ABS			0x100	/* as if it would begin with : */


struct gvl_result {
	/* this is the result */
	struct gvar	gvar;

	/* the following is needed for further searches and should not be 
	   modified by the caller 
	 */
	int			idx;
	char			sbuf[64];
	char			*abuf;
	int			isglob;
	union {
		struct {
			struct gvar	varbuf;
			int			abs;
		};
		struct {
			regex_t	reg;
			int		regneedfree;
		};
	};
};


int gvl_new ();
int gvl_free (int lstid);

#if 0
int gvl_newrplst ();
int gvl_freerplst (int rplid);
int gvl_setrootpath (int rplid, char *rpath, int flags);
#else
int gvl_setrootpath (int lstid, char *rpath, int flags);
#endif

int gvl_addvarns (int lstid, char *var, char *ns, int typ, int id, void *arg, int flags);
int gvl_addvar (int lstid, char *var, int typ, int id, void *arg, int flags);
int gvl_addgvar (int lstid, struct gvar*);
int gvl_rmvar (int lstid, const char *var, int flags);
int gvl_rmgvar (int lstid, struct gvar*, int flags);

int gvl_search (struct gvl_result *res, int lstid, char *var, int flags);
#if 0
int gvl_search2 (struct gvl_result *res, int lstid, int rplid, char *var, int flags);
#endif
int gvl_search_cleanup (struct gvl_result *res, int lstid);

/* the following function returns RERR_OK if str is a globbing expression,
 * RERR_FAIL if not, and any other negative error code on error
 */
int gvl_isglob (const char *str);

int gvl_gvarfree (struct gvar *);





#ifdef __cplusplus
}	/* extern "C" */
#endif






#endif	/* _R__FRLIB_LIB_EVENT_GVARLIST_H */

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
