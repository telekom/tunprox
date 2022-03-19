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





#include "errors.h"
#include "textop.h"
#include "config.h"
#include "slog.h"
#include "strcase.h"
#include "repcfg.h"

static int split_var_type (char*, char**, int*);


int
rep_save_conv (var, val)
	char	*var, *val;
{
	return rep_save_conv_string (var, val);
}

int
rep_save_conv_string (var, val)
	char	*var, *val;
{
	int	ret, type, ival;

	if (!var || !val) return RERR_PARAM;
	ret = split_var_type (var, &var, &type);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error in split_var_type(): %s", rerr_getstr3(ret));
		return ret;
	}
	switch (type) {
	case REP_TYPE_STRING:
		return rep_save_string (var, val);
	case REP_TYPE_INT:
		ival = cf_atoi (val);
		return rep_save_int (var, ival);
	default:
		FRLOGF (LOG_ERR, "invalid type %d", type);
		return RERR_INVALID_TYPE;
	}
	return RERR_INTERNAL;
}

int
rep_save_conv_int (var, val)
	char	*var;
	int	val;
{
	int	ret, type;
	char	sval[32];

	if (!var) return RERR_PARAM;
	ret = split_var_type (var, &var, &type);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error in split_var_type(): %s", rerr_getstr3(ret));
		return ret;
	}
	switch (type) {
	case REP_TYPE_STRING:
		sprintf (sval, "%d", val);
		return rep_save_string (var, sval);
	case REP_TYPE_INT:
		return rep_save_int (var, val);
	default:
		FRLOGF (LOG_ERR, "invalid type %d", type);
		return RERR_INVALID_TYPE;
	}
	return RERR_INTERNAL;
}


static
int
split_var_type (path, var, type)
	char	*path, **var;
	int	*type;
{
	char	*s, *s2;

	if (!path) return RERR_PARAM;
	if (var) *var=path;
	if (type) *type=REP_TYPE_NONE;

	path = top_skipwhite (path);
	s = index (path, ':');
	if (s) {
		s2 = index (path, '/');
		if (s2>s) s2=NULL;
		if (!s2) s2 = index (path, '\\');
		if (s2>s) s2=NULL;
		if (s2) s=NULL;
	}
	if (!s) {
		if (type) *type = REP_TYPE_STRING;	/* default */
		return RERR_OK;
	}
	if (var) *var = s+1;
	sswitch (path) {
	sincase ("string")
	sincase ("str")
		if (type) *type = REP_TYPE_STRING;
		break;
	sincase ("int")
	sincase ("dword")
		if (type) *type = REP_TYPE_INT;
		break;
	sdefault
		FRLOGF (LOG_ERR, "invalid type %.*s", (int)(s-path-1), path);
		return RERR_INVALID_TYPE;
	} esac;
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
