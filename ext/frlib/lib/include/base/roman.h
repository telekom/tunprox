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

#ifndef _R__FRLIB_LIB_BASE_ROMAN_H
#define _R__FRLIB_LIB_BASE_ROMAN_H



#ifdef __cplusplus
extern "C" {
#endif




#define ROM_LCASE		0x01		/* lower case */
#define ROM_UCASE		0x00		/* upper case - default */
#define ROM_ADD		0x02		/* additive (old) system */
#define ROM_SUB		0x00		/* subtractive (new) system */
#define ROM_MUL		0x00		/* for large numbers use multiply rule (default) */
#define ROM_CIFRAO	0x04		/* for large numbers use cifrao $ system */
#define ROM_DOT		0x08		/* like cifrao but use a dot . instead */
#define ROM_NEG		0x10		/* accept negative numbers */



/* returns the letters processed, currently no flags accepted */
int roman2num (int *num, char *roman, int flags);

/* returns number of letters written, or would be written if
   buffer is too small - flags, see above
 */
int num2roman (char *buf, int len, int num, int flags);




















#ifdef __cplusplus
}	/* extern "C" */
#endif




#endif	/*  _R__FRLIB_LIB_BASE_ROMAN_H */


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
