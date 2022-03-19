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

#ifndef _R__FRLIB_LIB_CONNECT_READLN_H
#define _R__FRLIB_LIB_CONNECT_READLN_H


#ifdef __cplusplus
extern "C" {
#endif


/* the following flags are used by the functions stripln and readln,
	read the comment on that functions to understand its meaning
 */
#define FDCOM_NONE				0x00
#define FDCOM_STRIPMIDDLE		0x01
#define FDCOM_STRIPALLSPACE	0x02
#define FDCOM_WITHBLANKLN		0x04
#define FDCOM_LOGLN				0x08
#define FDCOM_NOWAIT				0x10	/* not used any more */
#define FDCOM_SKIPCOMMENT		0x20
#define FDCOM_EOT					0x40
#define FDCOM_BLOCK				0x80

#define FDCOM_EOTCHAR			0x04


/* fdcom_readln reads a line terminated by \n or \r\n from the filedescriptor fd
	a \r\n is converted to a \n
	other then that, the line is not modified - the \n is contained in the
	output,
	with READLN_NOWAIT does not wait for input, but returns imediately
	if there are no data available returning RERR_NODATA
 */
int fdcom_readln (int fd, char **line, int flags);

/* fdcom_readln2 is like readln, but strips whitespaces at begin and end of
	line with STRIPLN_INTERN or STRIPLN_ALLSPACE even in the middle of
	the line. empty lines and lines only containing whitespaces are
	not returned except when READLN_WITHBLANKLN is given.
	with READLN_LOGLN, the line (before stripping is logged using
	myprtf (PRT_PROTIN, ...). 
 */
int fdcom_readln2 (int fd, char **line, int flags);

/* stripln strips whitespaces at beginning and end of string including newlines 
	with the flag STRIPLN_INTERN whitespaces in the middle of the string are
	reduced to a single one.
	with STRIPLN_ALLSPACE set, all whitespaces are deleted
 */
int fdcom_stripln (char *line, int flags);



/* the following 2 functions check wether fd is ready for reading/writing
	if ok, they return RERR_OK
	if not ok, they return RERR_NODATA
	on error other error conditions might be returned
 */
int fdcom_canread (int fd);
int fdcom_canwrite (int fd);




int fdcom_setreadtimeout (int timeout);









#ifdef __cplusplus
}	/* extern "C" */
#endif





#endif	/* _R__FRLIB_LIB_CONNECT_READLN_H */

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
