/*
 * Eddi Message Distribution Module
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 2.0 / GPL 2.0
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
 * Portions created by the Initial Developer are Copyright (C) 2013-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only under the
 * terms of the GPL, and not to allow others to use your version of this
 * file under the terms of the MPL, indicate your decision by deleting
 * the provisions above and replace them with the notice and other provisions
 * required by the GPL. If you do not delete the provisions above, a
 * recipient may use your version of this file under the terms of any one
 * of the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _R__KERNEL_EDDI_H
#define _R__KERNEL_EDDI_H

#include <linux/types.h>
#include <linux/socket.h>


/* Note that this file is shared with user space. */

#ifdef __cplusplus
extern "C" {
#endif


/* netlink definitions */
#define EDDI_NL_NAME "eddi"
#define EDDI_NL_VERSION 1

enum eddi_nl_commands {
	EDDI_NL_CMD_UNSPEC,
	EDDI_NL_CMD_CON_CHAN,
	EDDI_NL_CMD_SEND,
	__EDDI_NL_CMD_MAX
};
#define EDDI_NL_CMD_MAX (__EDDI_NL_CMD_MAX - 1)

enum eddi_attrs_con {
	EDDI_NL_CON_ATTR_UNSPEC,
	EDDI_NL_CON_ATTR_CHAN,
	__EDDI_NL_CON_ATTR_MAX
};
#define EDDI_NL_CON_ATTR_MAX (__EDDI_NL_CON_ATTR_MAX - 1)

enum eddi_attrs_send {
	EDDI_NL_SEND_ATTR_UNSPEC,
	EDDI_NL_SEND_ATTR_EVENT,
	__EDDI_NL_SEND_ATTR_MAX
};
#define EDDI_NL_SEND_ATTR_MAX (__EDDI_NL_SEND_ATTR_MAX - 1)






#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif /* _R__KERNEL_EDDI_H */


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
