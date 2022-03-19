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

#ifndef _R__FRLIB_LIB_CC_CONNECT_HOST_H
#define _R__FRLIB_LIB_CC_CONNECT_HOST_H


#include <fr/connect/host.h>
#include <fr/base/errors.h>
#include <string.h>
#include <strings.h>


class Host {
public:
	Host ()
	{
		bzero (this, sizeof (Host));
	};
	Host (const char *_name)
	{
		bzero (this, sizeof (Host));
		setName (_name);
	};
	Host (const Host &ohost)
	{
		bzero (this, sizeof (Host));
		setName (ohost.name);
	};
	Host (const ip_t ip)
	{
		char	str[64];
		bzero (this, sizeof (Host));
		if (host_ip2str (str, ip) != RERR_OK) return;
		setName (str);
	};
	int setName (const char *_name)
	{
		name = strdup (_name);
		if (!name) return RERR_NOMEM;
		return RERR_OK;
	};
	const char *getName ()
	{
		return name;
	};
	int isLocal (int &islocal)
	{
		return host_islocal (&islocal, name);
	};
	int getIP (ip_t &ip)
	{
		return host_getip (&ip, name);
	};
	int getHostName (char *hname)		/* must be a buffer of at least 64 bytes */
	{
		return host_getname (hname, name);
	};
	int getIPv4 (u_int32_t &ip)
	{
		return host_getIPv4 (&ip, name);
	};
	int getIPstr (char *str)		/* must be big enough to host ip address 
												as string */
	{
		ip_t	ip;
		int	ret;
		ret  = getIP (ip);
		if (!RERR_ISOK(ret)) return ret;
		return host_ip2str (str, ip);
	};

	static char * getLocalHostName ()
	{
		return host_GetLocalHostName ();
	};
protected:
	char	*name;
};










#endif	/* _R__FRLIB_LIB_CC_CONNECT_HOST_H */

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
