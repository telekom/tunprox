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

#ifndef _R__FRLIB_LIB_XML_XMLCURSOR_H
#define _R__FRLIB_LIB_XML_XMLCURSOR_H


#include <stdarg.h>
#include <fr/xml/xmlparser.h>


#ifdef __cplusplus
extern "C" {
#endif

struct xml_cursor {
	struct xml		*xml;
	struct xml_tag	*tag, *oldtag;
};




int xmlcurs_new (struct xml_cursor*, struct xml*);
int xmlcurs_newtag (struct xml_cursor*, struct xml_tag*);
struct xml_tag* xmlcurs_tag (struct xml_cursor*);
int xmlcurs_top (struct xml_cursor*);
int xmlcurs_up (struct xml_cursor*);
int xmlcurs_down (struct xml_cursor*);
int xmlcurs_next (struct xml_cursor*);
int xmlcurs_back (struct xml_cursor*);
int xmlcurs_gettext (char **text, struct xml_cursor*, int flags);
int xmlcurs_search (char **text, struct xml_cursor*, char *query, int flags);
int xmlcurs_fsearch (char **text, struct xml_cursor*, int flags, char *query, ...)
							__attribute__((format(printf, 4, 5)));
int xmlcurs_vfsearch (char **text, struct xml_cursor*, int flags, char *query, va_list ap);
int xmlcurs_searchtag (struct xml_cursor*, char *query, int flags);
int xmlcurs_fsearchtag (struct xml_cursor*, int flags, char *query, ...)
							__attribute__((format(printf, 3, 4)));
int xmlcurs_vfsearchtag (struct xml_cursor*, int flags, char *query, va_list ap);





#ifdef __cplusplus
}	/* extern "C" */
#endif









#endif	/* _R__FRLIB_LIB_XML_XMLCURSOR_H */

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
