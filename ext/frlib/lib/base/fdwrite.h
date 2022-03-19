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
 * Portions created by the Initial Developer are Copyright (C) 2003-2021
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _R__FRLIB_LIB_BASE_FDWRITE_H
#define _R__FRLIB_LIB_BASE_FDWRITE_H

#define _FILE_OFFSET_BITS 64
#include <unistd.h>



#ifdef __cplusplus
extern "C" {
#endif



ssize_t fdwrite (int fd, const void *buf, size_t count, int flags);
ssize_t fdread (int fd, void *buf, size_t count, int flags);
ssize_t fdwritep (int fd, const void *buf, size_t count, off_t pos, int flags);
ssize_t fdreadp (int fd, void *buf, size_t count, off_t pos, int flags);




#ifdef __cplusplus
}	/* extern "C" */
#endif





#endif	/* _R__FRLIB_LIB_BASE_FDWRITE_H */

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
