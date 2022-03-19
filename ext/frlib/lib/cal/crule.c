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

#include "crule.h"

#include "errors.h"
#include "tz.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>



static struct crul_t	*crullist = NULL;
static int 				crulnum = 0;
static int 				crulsz = 0;
static int				isinit = 0;


static int dodel (struct crul_t*);
static int doinit ();




int
crul_new (type)
	int	type;
{
	struct crul_t	*p;
	int				i,ret;

	if (!isinit) doinit ();
	if (crulnum >= crulsz) {
		p = realloc (crullist, (crulsz+16)*sizeof (struct crul_t));
		if (!p) return RERR_NOMEM;
		crullist = p;
		p+=crulsz;
		bzero (p, 16*sizeof (struct crul_t));
		crulsz += 16;
	} else {
		for (i=0, p=crullist; i<crulsz; i++, p++) {
			if (!p->used) break;
		}
		if (i>=crulsz) return RERR_INTERNAL;
	}
	bzero (p, sizeof (struct crul_t));
	p->id = p-crullist;
	if (type == CRUL_T_UTC) {
		type = CRUL_T_CJG;
		p->tz = CTZ_TZ_UTC;
	} else {
		p->tz = CTZ_TZ_DEFAULT;
	}
	p->caltype = type;
	switch (type) {
	case CRUL_T_NONE:
		break;
	case CRUL_T_CJG:
		ret = cjg_rulesetdef (&(p->cjg));
		if (!RERR_ISOK(ret)) return ret;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	p->used = 1;
	crulnum++;
	return p->id;
}


int
crul_del (num)
	int	num;
{
	struct crul_t	*p;
	int				ret;

	ret = crul_get (&p, num);
	if (!RERR_ISOK(ret)) return ret;
	if (p->isdef) return RERR_FORBIDDEN;
	ret = dodel (p);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


int
crul_get (rul, num)
	struct crul_t	**rul;
	int				num;
{
	struct crul_t	*p;

	if (!isinit) doinit ();
	if (num < 0 || num >= crulsz) return RERR_OUTOFRANGE;
	p = crullist + num;
	if (p->id != num || !p->used) return RERR_INVALID_ID;
	if (rul) *rul = p;
	return RERR_OK;
}


int
crul_tzset (num, tz)
	int	num, tz;
{
	struct crul_t	*p;
	int				ret;

	ret = crul_get (&p, num);
	if (!RERR_ISOK(ret)) return ret;
	p->tz = tz;
	return RERR_OK;
}

int
crul_tzset2 (num, tzstr)
	int	num;
	char	*tzstr;
{
	struct crul_t	*p;
	int				tz, ret;

	ret = crul_get (&p, num);
	if (!RERR_ISOK(ret)) return ret;
	tz = ctz_set (tzstr, CTZ_F_DEFAULT);
	if (tz < 0) return tz;
	p->tz = tz;
	return RERR_OK;
}



static
int
doinit ()
{
	int	i, ret;

	if (isinit) return RERR_OK;
	isinit = 1;		/* must be already here to fake crul_new */
	for (i=0; i<=CRUL_T_MAX; i++) {
		ret = crul_new (i);
		if (ret != i) {
			for (i--; i>=0; i--) dodel (crullist+i);
			isinit = 0;
			if (!RERR_ISOK(ret)) return ret;
			return RERR_INTERNAL;
		}
		crullist[i].isdef = 1;
	}
	return RERR_OK;
}






static
int
dodel (rul)
	struct crul_t	*rul;
{
	int	ret;

	if (!rul) return RERR_PARAM;
	switch (rul->caltype) {
	case CRUL_T_NONE:
		break;
	case CRUL_T_CJG:
		ret = cjg_ruledel (&(rul->cjg));
		break;
	default:
		ret = RERR_INVALID_ID;
		break;
	}
	bzero (rul, sizeof (struct crul_t));
	crulnum--;
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}
































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
