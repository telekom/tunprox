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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <sys/stat.h>
#include <fcntl.h>
#if 0
#include <db1/db.h>
#endif

#include "dldb.h"
#include "ext/db.h"



#include "errors.h"
#include "slog.h"
#include "config.h"
#include "repcfg.h"



int
rep_get_int (val, var)
	int	*val;
	char	*var;
{
	char	*int_str=NULL;
	int	ret;

	ret = rep_get_string (&int_str, var);
	if (!RERR_ISOK(ret)) return ret;
	if (val) *val = cf_atoi (int_str);
	if (int_str) free (int_str);
	return RERR_OK;
}

int
rep_save_int (var, val)
	char	*var;
	int	val;
{
	char	int_str[32];

	sprintf (int_str, "%d", val);
	return rep_save_string (var, int_str);
}


int
rep_get (val, var, type)
	void	*val;
	char	*var;
	int	type;
{
	if (!var || !val) return RERR_PARAM;
	switch (type) {
	case REP_TYPE_INT:
		return rep_get_int ((int*)val, var);
	case REP_TYPE_STRING:
		return rep_get_string ((char**)val, var);
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}


int
rep_save (var, val, type)
	char	*var;
	void	*val;
	int	type;
{
	if (!var || !val) return RERR_PARAM;
	switch (type) {
	case REP_TYPE_INT:
		return rep_save_int (var, *(int*)val);
	case REP_TYPE_STRING:
		return rep_save_string (var, val);
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}




int
rep_get_string (val, var)
	char	**val, *var;
{
	const char	*rep;
	DB				*db;
	DBT			dbt, key;

	if (!var || !val) return RERR_PARAM;
	if (val) *val=NULL;
	rep = cf_getval2 ("repository", "repository");
	db = EXTCALL(dbopen) (rep, 0, O_RDONLY, DB_BTREE, NULL);
	if (!db) {
		FRLOGF (LOG_ERR, "error opening db (%s): %s", rep,
						rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	key.data = var;
	key.size = strlen (var);
	if (db->put (db, &key, &dbt, R_SETCURSOR) < 0) {
		FRLOGF (LOG_ERR, "error in db->put(): %s",
						rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	db->close (db);
	if (val) {
		*val = strdup (dbt.data);
		if (!*val) return RERR_NOMEM;
	}
	return RERR_OK;
}


#if 0	/* old version */
int
rep_save_string (var, val)
	char	*var, *val;
{
	char	*fake_rep;
	FILE	*f;

	if (!var || !val) return RERR_PARAM;
	fake_rep = cf_getval ("fake_repository");
	if (!fake_rep) fake_rep = cf_getval2 ("fake_rep", "fake_repository");
	f = fopen (fake_rep, "a");
	if (!f) {
		return RERR_SYSTEM;
	}
	fprintf (f, "\"%s\"=\"%s\"\n", var, val);
	fclose (f);
	return RERR_OK;
}
#endif




int
rep_save_string (var, val)
	char	*var, *val;
{
	const char	*rep;
	DB				*db;
	DBT			dbt, key;

	if (!var || !val) return RERR_PARAM;
	rep = cf_getval2 ("repository", "repository");
	db = EXTCALL(dbopen) (rep, O_CREAT, O_RDWR, DB_BTREE, NULL);
	if (!db) {
		FRLOGF (LOG_ERR, "error opening db (%s): %s", rep,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	dbt.data = val;
	dbt.size = strlen (val) + 1;
	key.data = var;
	key.size = strlen (var);
	if (db->put (db, &key, &dbt, R_SETCURSOR) < 0) {
		FRLOGF (LOG_ERR, "error in db->put(): %s",
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	db->close (db);
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
