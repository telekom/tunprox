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
#include "slog.h"
#include "config.h"
#include "repcfg.h"
#include "strcase.h"

#include <windef.h>
#include <winnt.h>
#include <winbase.h>
#include <winreg.h>


static int do_rep_save (char*, void*, int);
static int do_write_var (HKEY, char*, void*, int);
static int conv_slashes (char*);
static int map_hkey (HKEY*, char*);


int
rep_get_int (val, var)
	int	*val;
	char	*var;
{
	return rep_get ((void*)val, var, REP_TYPE_INT);
}

int
rep_get_string (val, var)
	char	**val;
	char	*var;
{
	return rep_get ((void*)val, var, REP_TYPE_STRING);
}


int
rep_save_int (var, val)
	char	*var;
	int	val;
{
	return rep_save (var, &val, REP_TYPE_INT);
}

int
rep_save_string (var, val)
	char	*var, *val;
{
	return rep_save (var, val, REP_TYPE_STRING);
}


int
rep_get (val, var, type)
	void	*val;
	char	*var;
	int	type;
{
	int	ret;
	char	*var2;

	if (!var || !val) return RERR_PARAM;
	var2 = strdup (var);
	if (!var2) return RERR_NOMEM;
	ret = do_rep_get (val, var2, type);
	free (var2);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error writing var >>%s<< to registry: %s", var,
									rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}


int
rep_save (var, val, type)
	char	*var;
	void	*val;
	int	type;
{
	int	ret;
	char	*var2;

	if (!var || !val) return RERR_PARAM;
	var2 = strdup (var);
	if (!var2) return RERR_NOMEM;
	ret = do_rep_save (var2, val, type);
	free (var2);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error writing var >>%s<< to registry: %s", var,
								rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}



static
int
do_rep_save (var, val, type)
	char	*var;
	void	*val;
	int	type;
{
	char	*s, *subkey;
	int	ret;
	HKEY	hkey, hkey2;

	if (!var || !val) return RERR_PARAM;
	var = top_skipwhiteplus (var, "/");
	if (strncasecmp (var, "HKEY_", 5) != 0) {
		hkey = HKEY_CURRENT_USER;
		subkey = var;
	} else {
		s = index (var, '/');
		if (!s) return RERR_INVALID_REPVAR;
		*s=0; s++;
		var = top_stripwhite (var, 0);
		ret = map_hkey (&hkey, var);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "invalid HKEY >>%s<<: %s", var,
									rerr_getstr3(ret));
			return ret;
		}
		subkey = top_skipwhiteplus (s, "/");
	}
	s = rindex (subkey, '/');
	if (!s) {
		var = subkey;
		subkey = NULL;
	} else {
		*s=0;
		var = s+1;
		subkey = top_stripwhite (subkey, 0);
		if (!*subkey) subkey=NULL;
		ret = conv_slashes (subkey);
		if (!RERR_ISOK(ret)) return ret;
	}
	var = top_stripwhite (var, 0);
	if (!*var) return RERR_INVALID_REPVAR;
	if (subkey) {
		ret = RegOpenKeyEx (hkey, subkey, 0, KEY_ALL_ACCESS, &hkey2);
		if (ret != 0) {
			ret = RegCreateKey (hkey, subkey, &hkey2);
			if (ret != 0) {
				FRLOGF (LOG_ERR, "error opening subkey >>%s<<: %d", subkey, ret);
				return RERR_REGISTRY;
			}
			RegCloseKey (hkey2);
			ret = RegOpenKeyEx (hkey, subkey, 0, KEY_ALL_ACCESS, &hkey2);
			if (ret != 0) {
				FRLOGF (LOG_ERR, "error opening subkey >>%s<<: %d", subkey, ret);
				return RERR_REGISTRY;
			}
		}
		hkey = hkey2;
	}
	ret = do_write_var (hkey, var, val, type);
	if (subkey) {
		RegCloseKey (hkey);
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error writing var >>%s<< to subkey >>%s<<: %s", var,
								subkey?subkey:"", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


static
int
do_write_var (hkey, var, val, type)
	HKEY	hkey;
	char	*var;
	void	*val;
	int	type;
{
	DWORD		dwtype;
	DWORD		dwsize;
	DWORD		dwval;
	void		*myval;
	wchar_t	*wch_val=NULL;
	int 		ret;

	switch (type) {
	case REP_TYPE_INT:
		dwtype = REG_DWORD;
		dwsize = sizeof (DWORD);
		dwval = *(DWORD*) val;
		myval = (void*)&dwval;
		break;
	case REP_TYPE_STRING:
		dwtype = REG_SZ;
		//dwtype = REG_EXPAND_SZ;
		dwsize = (utf8clen ((char*)val)+1) * sizeof (wchar_t);
		//dwsize = strlen (val)+1;
		ret = utf2wchar_str (&wch_val, (char*)val);
		if (!RERR_ISOK(ret)) return ret;
		myval = (void*)wch_val;
		//myval = (void*)val;
		break;
	default:
		FRLOGF (LOG_ERR, "invalid type (%d)", type);
		return RERR_INVALID_TYPE;
	}
	ret = RegSetValueEx (hkey, var, 0, dwtype, myval, dwsize);
	if (wch_val) free (wch_val);
	if (ret != 0) {
		FRLOGF (LOG_ERR, "error in RegSetValueEx(): %d", ret);
		return RERR_REGISTRY;
	}
	return RERR_OK;
}



static
int
do_rep_get (val, var, type)
	void	*val;
	char	*var;
	int	type;
{
	char	*s, *subkey;
	int	ret;
	HKEY	hkey, hkey2;

	if (!var || !val) return RERR_PARAM;
	var = top_skipwhiteplus (var, "/");
	if (strncasecmp (var, "HKEY_", 5) != 0) {
		hkey = HKEY_CURRENT_USER;
		subkey = var;
	} else {
		s = index (var, '/');
		if (!s) return RERR_INVALID_REPVAR;
		*s=0; s++;
		var = top_stripwhite (var, 0);
		ret = map_hkey (&hkey, var);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "invalid HKEY >>%s<<: %s", var, rerr_getstr3(ret));
			return ret;
		}
		subkey = top_skipwhiteplus (s, "/");
	}
	s = rindex (subkey, '/');
	if (!s) {
		var = subkey;
		subkey = NULL;
	} else {
		*s=0;
		var = s+1;
		subkey = top_stripwhite (subkey, 0);
		if (!*subkey) subkey=NULL;
		ret = conv_slashes (subkey);
		if (!RERR_ISOK(ret)) return ret;
	}
	var = top_stripwhite (var, 0);
	if (!*var) return RERR_INVALID_REPVAR;
	if (subkey) {
		ret = RegOpenKeyEx (hkey, subkey, 0, KEY_ALL_ACCESS, &hkey2);
		if (ret != 0) {
			ret = RegCreateKey (hkey, subkey, &hkey2);
			if (ret != 0) {
				FRLOGF (LOG_ERR, "error opening subkey >>%s<<: %d", subkey, ret);
				return RERR_REGISTRY;
			}
			RegCloseKey (hkey2);
			ret = RegOpenKeyEx (hkey, subkey, 0, KEY_ALL_ACCESS, &hkey2);
			if (ret != 0) {
				FRLOGF (LOG_ERR, "error opening subkey >>%s<<: %d", subkey, ret);
				return RERR_REGISTRY;
			}
		}
		hkey = hkey2;
	}
	ret = do_get_var (hkey, val, var, type);
	if (subkey) {
		RegCloseKey (hkey);
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error writing var >>%s<< to subkey >>%s<<: %s", var,
								subkey?subkey:"", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

int
do_get_var (hkey, val, var, type)
	HKEY	hkey;
	void	*val;
	char	*var;
	int	type;
{
	DWORD		dwtype;
	DWORD		dwsize;
	DWORD		dwval;
	void		*myval;
	wchar_t	wch_val[1024];
	int 		ret;

	switch (type) {
	case REP_TYPE_INT:
		dwtype = REG_DWORD;
		dwsize = sizeof (DWORD);
		dwval = *(DWORD*) val;
		myval = (void*)&dwval;
		break;
	case REP_TYPE_STRING:
		dwtype = REG_SZ;
		dwsize = sizeof (wch_val)-1;
		myval = (void*)wch_val;
		break;
	default:
		FRLOGF (LOG_ERR, "invalid type (%d)", type);
		return RERR_INVALID_TYPE;
	}
	ret = RegGetValueEx (hkey, var, 0, dwtype, myval, dwsize);
	if (wch_val) free (wch_val);
	if (ret != 0) {
		FRLOGF (LOG_ERR, "error in RegGetValueEx(): %d", ret);
		return RERR_REGISTRY;
	}
	if (type == REP_TYPE_STRING) {
		ret = wchar2utf_str (&((char*)val), wch_val);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}


static
int
conv_slashes (subkey)
	char	*subkey;
{
	char	*s, *s2;

	if (!subkey) return RERR_PARAM;
	for (s=s2=subkey; *s; s++, s2++) {
		if (*s=='/' || *s=='\\') {
			for (s2--; s2>=subkey && isspace (*s2); s2--); s2++;
			*s2='\\';
			s=top_skipwhiteplus (s, "/\\");
			s--;
		} else {
			*s2 = *s;
		}
	}
	*s2=0;
	return RERR_OK;
}




static
int
map_hkey (hkey, skey)
	HKEY	*hkey;
	char	*skey;
{
	if (!hkey || !skey) return RERR_PARAM;
	sswitch (skey) {
	sicase ("HKEY_CLASSES_ROOT")
		*hkey = HKEY_CLASSES_ROOT;
		break;
	sicase ("HKEY_CURRENT_USER")
		*hkey = HKEY_CURRENT_USER;
		break;
	sicase ("HKEY_LOCAL_MACHINE")
		*hkey = HKEY_LOCAL_MACHINE;
		break;
	sicase ("HKEY_USERS")
		*hkey = HKEY_USERS;
		break;
	sicase ("HKEY_PERFORMANCE_DATA")
		*hkey = HKEY_PERFORMANCE_DATA;
		break;
	sicase ("HKEY_CURRENT_CONFIG")
		*hkey = HKEY_CURRENT_CONFIG;
		break;
	sicase ("HKEY_DYN_DATA")
		*hkey = HKEY_DYN_DATA;
		break;
	sdefault
		return RERR_INVALID_REPVAR;
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
