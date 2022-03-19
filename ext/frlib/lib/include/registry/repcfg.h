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

#ifndef _R__FRLIB_LIB_REGISTRY_REP_CFG_H
#define _R__FRLIB_LIB_REGISTRY_REP_CFG_H



#ifdef __cplusplus
extern "C" {
#endif



#define REP_TYPE_NONE		0
#define REP_TYPE_INT			1
#define REP_TYPE_STRING		2





int rep_get (void *val, char *var, int type);
int rep_get_int (int *val, char *var);
int rep_get_string (char **val, char *var);

int rep_save (char *var, void *val, int type);
int rep_save_int (char *var, int val);
int rep_save_string (char *var, char *val);
int rep_save_conv (char *var, char *val);
int rep_save_conv_string (char *var, char *val);
int rep_save_conv_int (char *var, int val);





#ifdef __cplusplus
}	/* extern "C" */
#endif



















#endif	/* _R__FRLIB_LIB_REGISTRY_REP_CFG_H */

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
