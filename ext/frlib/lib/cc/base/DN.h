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

#ifndef _R__FRLIB_LIB_CC_BASE_DN_H
#define _R__FRLIB_LIB_CC_BASE_DN_H




#include <fr/base/dn.h>
#include <strings.h>


#if 0 /* defined in fr/base/dn.h */
struct dn_part {
	char	*var, *val;
};

struct dn {
	char			*buf;
	struct dn_part	*list;
	int				listnum;
};
#endif

class DN {
public:
	DN ()
	{
		bzero (this, sizeof (DN));
	};
	DN (DN &src)
	{
		bzero (this, sizeof (DN));
		dn_cpy (&dn, &(src.dn));
	};
	DN (const char *str)
	{
		bzero (this, sizeof (DN));
		dn_split (&dn, (char *)str);
	};
	~DN ()
	{
		dn_hfree (&dn);
	};
	int cmp (const DN &odn) const
	{
		return dn_cmp_struct ((struct dn*)&dn, (struct dn*)&(odn.dn));
	};
	int cmp (const char *str) const
	{
		DN odn(str);
		return cmp (odn);
	};
	bool operator== (const DN &odn) const 
	{
		return cmp (odn) == 0;
	};
	bool operator== (const char * str) const 
	{
		return cmp (str) == 0;
	};
	const char *getPart (const char *var)
	{
		return dn_getpart (&dn, var);
	};
protected:
	struct dn	dn;
};
















#endif	/* _R__FRLIB_LIB_CC_BASE_DN_H */

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
