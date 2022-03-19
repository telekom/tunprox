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

#include "VirtXmlParser.h"

#include <fr/base.h>
#include <fr/xml.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>




void
VirtXmlParser::init ()
{
	bzero (&m_xml, sizeof (struct xml));
	bzero (&m_cursor, sizeof (struct xml_cursor));
	m_fileName = m_rootNode = NULL;
}

void
VirtXmlParser::destroy ()
{
	xml_hfree (&m_xml);
	init ();
}

int
VirtXmlParser::Parse ()
{
	int	ret;
	char	*buf;

	buf = fop_read_fn (m_fileName);
	if (!buf) {
		FRLOGF (LOG_ERR, "error reading file >>%s<<: %s", 
								m_fileName?m_fileName:(char*)"<NULL>",
								rerr_getstr3(ret));
		return RERR_SYSTEM;
	}
	ret = xml_parse (&m_xml, buf, XMLPARSER_FLAGS_STANDARD);
	if (!RERR_ISOK(ret)) {
		free (buf);
		FRLOGF (LOG_ERR, "error parsing idb xml-file >>%s<<: %s",
								m_fileName?m_fileName:(char*)"<NULL>", 
								rerr_getstr3 (ret));
		return ret;
	}
	ret = xmlcurs_new (&m_cursor, &m_xml);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error creating cursor for parsing >>%s<<: %s",
								m_fileName?m_fileName:(char*)"<NULL>",
								rerr_getstr3 (ret));
		return ret;
	}
	ret = xmlcurs_searchtag (&m_cursor, m_rootNode, XML_SEARCH_NO_ATTR);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error finding tag >>%s<< in xml file >>%s<<: %s",
								m_rootNode?m_rootNode:(char*)"<NULL>", 
								m_fileName?m_fileName:(char*)"<NULL>",
								rerr_getstr3 (ret));
		return ret;
	}
	ret = ParseSubTree ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error parsing xml tree in xml file >>%s<<: %s",
								m_fileName?m_fileName:(char*)"<NULL>",
								rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}


int
VirtXmlParser::ParseSubTree ()
{
	int	ret;

	ret = xmlcurs_down (&m_cursor);
	if (ret == RERR_NOT_FOUND) {
		return RERR_OK;
	} else if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error - descending tree: %s", rerr_getstr3 (ret));
		return ret;
	}
	while (1) {
		ret = isNode ();
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error in isNode(): %s", rerr_getstr3 (ret));
			return ret;
		}
		if (ret /* isNode() */) {
			ret = ParseNode ();
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error in ParseNode(): %s", rerr_getstr3 (ret));
				return ret;
			}
		} else /* leaf */ {
			ret = ParseLeaf ();
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error in ParseNode(): %s", rerr_getstr3 (ret));
				return ret;
			}
		}
		ret = xmlcurs_next (&m_cursor);
		if (ret == RERR_NOT_FOUND) {
			break;
		} else if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error - walking tree: %s", rerr_getstr3 (ret));
			return ret;
		}
	}
	ret = xmlcurs_up (&m_cursor);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error - going up in tree: %s", rerr_getstr3 (ret));
		return ret;
	}
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
