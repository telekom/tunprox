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

#ifndef _R__FRLIB_LIB_CC_XML_VIRTXMLPARSER_H
#define _R__FRLIB_LIB_CC_XML_VIRTXMLPARSER_H




#include <fr/xml.h>






/*!\brief class to parse xml-file.
 *
 * This is a wrapper to the xmlparser.
 * It is parsed a subtree starting from a given "root"-node.
 * The class is virtual, so the real parser needs to inherit
 * this class.
 */
class VirtXmlParser {
public:
	/*!\brief the constructor makes minimal initialization */
	VirtXmlParser ()
	{
		init ();
	};
	virtual ~VirtXmlParser ()
	{
		destroy ();
	};

	/*!\brief destructor to be called mannually */
	void destroy ();

	/*!\brief The main function, which starts parsing 
	 *
	 * \return The function returns an error code (RERR_OK if everything is ok).
	 */
	int Parse ();

	/*!\brief Parses a subtree. The position is indicated by the xml_cursor.
	 *
	 * \return The function returns an error code (RERR_OK if everything is ok).
	 */
	int ParseSubTree ();

	/*!\brief virtual function which must return 1 if node has a subtree to
	 *			be parsed, 0 if it is a leaf, and a negative value for error.
	 *
	 * \note This function \b must be implemented by super-class.
	 * \return The function must return 0, 1 or a negative error code.
	 */
	virtual int isNode () = 0;

	/*!\brief virtual function to parse node with subtree.
	 *
	 * \note This function \b must be implemented by super-class.
	 * \return The function must return an error code 
	 *				(RERR_OK if everything is ok).
	 */
	virtual int ParseNode () = 0;

	/*!\brief virtual function to parse a leaf in xml-tree
	 *
	 * \note This function \b must be implemented by super-class.
	 * \return The function must return an error code 
	 *				(RERR_OK if everything is ok).
	 */
	virtual int ParseLeaf () = 0;

protected:
	/*!\brief file name of xml-file to be parsed. 
	 *
	 * \note must be set by super-class.
	 */
	char					*m_fileName;	

	/*!\brief name of root node, from where parsing starts.
	 *
	 * \note must be set by super-class.
	 */
	char					*m_rootNode;

	struct xml			m_xml;		/*!<\brief xml struct produced by xmlparser */
	struct xml_cursor	m_cursor;	/*!<\brief xml cursor to traverse xml tree */

	/*!\brief helper function for constructor */
	void init ();
};































#endif	/* _R__FRLIB_LIB_CC_XML_VIRTXMLPARSER_H */

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
