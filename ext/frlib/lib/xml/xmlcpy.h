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

#ifndef _R__FRLIB_LIB_XML_XMLCPY_H
#define _R__FRLIB_LIB_XML_XMLCPY_H

#include <fr/xml/xmlparser.h>


#ifdef __cplusplus
extern "C" {
#endif



#define XMLCPY_F_NONE				0x00
#define XMLCPY_F_SAMEBUF			0x01
#define XMLCPY_F_NEWBUF				0x02
#define XMLCPY_F_TEXT_APPEND		0x04
#define XMLCPY_F_TEXT_OVERWRITE	0x08
#define XMLCPY_F_ATTR_OVERWRITE	0x10

#define XMLCPY_F_APPEND				0x04
#define XMLCPY_F_OVERWRITE			0x18




int xml_cpy (struct xml *dest, struct xml *src, int flags);
int xml_merge (struct xml *dest, struct xml *src, int flags);








#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_XML_XMLCPY_H */
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
