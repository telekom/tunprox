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

#ifndef _R__FRLIB_LIB_CAL_CRULE_H
#define _R__FRLIB_LIB_CAL_CRULE_H


#include <fr/cal/cjg.h>


#ifdef __cplusplus
extern "C" {
#endif


/* the following defines can be used as types AND default rules */

#define CRUL_T_NONE	0	/* dummy calendar */
#define CRUL_T_CJG	1	/* julian - gregorian calendar */
#define CRUL_T_UTC	2	/* julian - gregorian calendar with utc timezone */
#define CRUL_T_MAX	2


struct crul_t {
	int	id;
	struct {
		uint32_t	used:1,
					isdef:1;
	};
	int	caltype;
	int	tz;
	union {
		int					dummy;
		struct cjg_rule	cjg;
	};
};



int crul_new (int type);
int crul_del (int num);
int crul_get (struct crul_t **rul, int num);
int crul_tzset (int num, int tz);
int crul_tzset2 (int num, char *tzstr);





















#ifdef __cplusplus
}	/* extern "C" */
#endif






#endif	/*  _R__FRLIB_LIB_CAL_CRULE_H */

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
