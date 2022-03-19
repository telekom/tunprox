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
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif


#include "errors.h"
#include "config.h"
#include "textop.h"
#include "xmlparser.h"
#include "xmltag.h"



int
xmltag_hasattr (tag, attr)
	struct xml_tag	*tag;
	char				*attr;
{
	return xmltag_findattr (NULL, tag, attr) == RERR_OK;
}

char *
xmltag_getattr (tag, attr)
	struct xml_tag	*tag;
	char				*attr;
{
	return xmltag_getnattr (tag, attr, 0);
}

char *
xmltag_getnattr (tag, attr, num)
	struct xml_tag	*tag;
	char				*attr;
	int				num;
{
	char	*res;

	return (xmltag_findnattr (&res, tag, attr, num) == RERR_OK) ? res : NULL;
}


int
xmltag_findattr (val, tag, attr)
	char				**val, *attr;
	struct xml_tag	*tag;
{
	return xmltag_findnattr (val, tag, attr, 0);
}


int
xmltag_findnattr (val, tag, attr, num)
	char				**val, *attr;
	struct xml_tag	*tag;
	int				num;
{
	int	i;

	if (!tag || !attr) return RERR_PARAM;
	if (!*attr) return RERR_NOT_FOUND;
	if (val) *val = NULL;
	for (i=0; i<tag->attr_list.len; i++) {
		if (!strcasecmp (tag->attr_list.list[i].var, attr)) {
			num--;
			if (num<0) {
				if (val) *val = tag->attr_list.list[i].val;
				return RERR_OK;
			}
		}
	}
	return RERR_NOT_FOUND;
}

int
xmltag_numattr (tag, attr)
	struct xml_tag	*tag;
	char				*attr;
{
	int	num, i;

	if (!tag) return RERR_PARAM;
	if (!attr) return tag->attr_list.len;
	for (i=num=0; i<tag->attr_list.len; i++) {
		if (!strcasecmp (tag->attr_list.list[i].var, attr)) num++;
	}
	return num;
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
