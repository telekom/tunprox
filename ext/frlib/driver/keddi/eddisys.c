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


#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>


#include "eddinl.int.h"

#define SHOW_ATTR(name) \
	static \
	ssize_t \
	eddi_sys_show_attr_##name ( \
		struct kobject				*kobj, \
		struct kobj_attribute	*attr, \
		char							*buf) \
	{ \
		return sprintf (buf, "%d\n", eddi_nl_##name); \
	}

#define STORE_ATTR(name) \
	static \
	ssize_t \
	eddi_sys_store_attr_##name ( \
		struct kobject				*kobj, \
		struct kobj_attribute	*attr, \
		const char					*buf, \
		size_t						count) \
	{ \
		sscanf (buf, "%du", &eddi_nl_##name); \
		return count; \
	}

#define MKATTR_RD(name) \
	static struct kobj_attribute name##_attribute = \
			__ATTR(name, 0444, eddi_sys_show_attr_##name, NULL);

#define MKATTR_RDWR(name) \
	static struct kobj_attribute name##_attribute = \
			__ATTR(name, 0644, eddi_sys_show_attr_##name, eddi_sys_store_attr_##name);



SHOW_ATTR(family_id)
SHOW_ATTR(rx_num)
SHOW_ATTR(rx_bytes)
SHOW_ATTR(tx_num)
SHOW_ATTR(tx_bytes)
SHOW_ATTR(users_act)
SHOW_ATTR(users_max)
SHOW_ATTR(users_total)

MKATTR_RD(family_id)
MKATTR_RD(rx_num)
MKATTR_RD(rx_bytes)
MKATTR_RD(tx_num)
MKATTR_RD(tx_bytes)
MKATTR_RD(users_act)
MKATTR_RD(users_max)
MKATTR_RD(users_total)

SHOW_ATTR(enable_debug)
STORE_ATTR(enable_debug)
MKATTR_RDWR(enable_debug)



static struct attribute *eddi_sys_attrs[] = {
	&family_id_attribute.attr,
	&rx_num_attribute.attr,
	&rx_bytes_attribute.attr,
	&tx_num_attribute.attr,
	&tx_bytes_attribute.attr,
	&users_act_attribute.attr,
	&users_max_attribute.attr,
	&users_total_attribute.attr,
	&enable_debug_attribute.attr,
	NULL,
};


static struct attribute_group eddi_sys_attr_group = {
		.attrs = eddi_sys_attrs,
};

static struct kobject *eddi_sys_kobj;


int
eddi_sys_init (void)
{
	int ret;
	
	eddi_sys_kobj = kobject_create_and_add ("keddi", kernel_kobj);
	if (!eddi_sys_kobj) return -ENOMEM;
	
	ret = sysfs_create_group(eddi_sys_kobj, &eddi_sys_attr_group);
	if (ret) {
		kobject_put(eddi_sys_kobj);
		return ret;
	}

	return 0;
}


void
eddi_sys_exit (void)
{
	kobject_put (eddi_sys_kobj);
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
