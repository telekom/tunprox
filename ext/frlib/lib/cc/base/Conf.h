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

#ifndef _R__FRLIB_LIB_CC_BASE_CONFIG_H
#define _R__FRLIB_LIB_CC_BASE_CONFIG_H


#include <fr/base/config.h>
#include <fr/base/secureconfig.h>
#include <fr/base/errors.h>
#include <stdlib.h>


class Conf {
public:
	int setDefaultCFname (const char *cfname, int flags)
	{
		return cf_default_cfname (cfname, flags);
	};
	int setCFname (const char *cfname)
	{
		return cf_set_cfname (cfname);
	};
	const char *getCFname ()
	{
		return cf_get_cfname ();
	};
	int read()
	{
		return cf_read ();
	};
	int mayRead ()
	{
		return cf_mayread ();
	};
	int reRead ()
	{
		return cf_reread ();
	};
	int HUPreRead ()
	{
		return cf_hup_reread ();
	};
	int beginRead ()
	{
		return cf_begin_read ();
	};
	int endRead ()
	{
		return endRead ();
	};
	int endReadCB (cf_reread_t func)
	{
		return cf_end_read_cb (func);
	};


	const char *getVal (const char *var)
	{
		return cf_getval (var);
	};
	const char *getVal2 (const char *var, const char *defvalue)
	{
		return cf_getval2 (var, defvalue);
	};

	int getNumArr (const char *var)
	{
		return cf_getnumarr (var);
	};
	const char * getArr (const char *var, const char *idx)
	{
		return cf_getarr (var, idx);
	};
	const char *getArr2 (const char *var, const char *idx, const char *defvalue)
	{
		return cf_getarr2 (var, idx, defvalue);
	};
	
	const char *getArrInt (const char *var, int idx)
	{
		return cf_getarri (var, idx);
	};
	const char *getArrInt2 (const char *var, int idx, const char *defvalue)
	{
		return cf_getarri2 (var, idx, defvalue);
	};

	const char *getArrX (const char *var, int x)
	{
		return cf_getarrx (var, x);
	};
	const char *getArrXwIdx (const char *var, int x, const char **idx)
	{
		return cf_getarrxwi (var, x, idx);
	};
	const char *getMultArr (const char *var, int numidx, ...) 
	{
		va_list 		ap;
		const char	*str;

		va_start (ap, numidx);
		str = cf_vgetmarr (var, numidx, ap);
		va_end (ap);
		return str;
	};
	const char *getMultArr2 (const char *var, const char *defval, int numidx, ...) 
	{
		va_list 		ap;
		const char	*str;

		va_start (ap, numidx);
		str = cf_vgetmarr2 (var, defval, numidx, ap);
		va_end (ap);
		return str;
	};
	const char *getVMultArr (const char *var, int numidx, va_list ap)
	{
		return cf_vgetmarr (var, numidx, ap);
	};
	const char *getVMultArr2 (const char *var, const char *defval, int numidx, va_list ap)
	{
		return cf_vgetmarr2 (var, defval, numidx, ap);
	};
	const char *getBiArr (const char *var, const char *idx1, const char *idx2)
	{
		return cf_get2arr (var, idx1, idx2);
	};
	const char *getBiArr2 (const char *var, const char *idx1, const char *idx2, const char *defval)
	{
		return cf_get2arr2 (var, idx1, idx2, defval);
	};
	

	int isyes (const char *val)
	{
		return cf_isyes (val);
	};
	int atoi (const char *val)
	{
		return cf_atoi (val);
	};


	/* Secure Config Vars */
	int readSCF (const char *passwd)
	{
		return scf_read (passwd);
	};
	int readSCFwFD (int fd)
	{
		return scf_fdread (fd);
	};
	int askReadSCF ()
	{
		return scf_askread ();
	};
	int reReadSCF ()
	{
		return scf_reread ();
	};
	void forgetPass ()
	{
		scf_forgetpass ();
	};
	const char * getPass ()
	{
		return scf_getpass ();
	};
	int haveSCF ()
	{
		return scf_havefile ();
	};


protected:
	static Conf	* gconf;

	Conf () {};
	~Conf () 
	{
		scf_free ();
		cf_free_config ();
		gconf = NULL;
	};

public:
	static Conf *getConf ()
	{
		if (gconf) return gconf;
		gconf = new Conf();
		if (!gconf) return NULL;
		if (gconf->read () != RERR_OK) return NULL;
		return gconf;
	};
};




















#endif	/* _R__FRLIB_LIB_CC_BASE_CONFIG_H */


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
