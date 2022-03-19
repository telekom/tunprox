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

#ifndef _R__FRLIB_LIB_BASE_STRCASE_H
#define _R__FRLIB_LIB_BASE_STRCASE_H


#include <string.h>
#include <strings.h>


#define sswitch(_refstr) { \
			const char * _r__strcase_ref = _refstr; \
			switch (0) default: { \
			if (!_r__strcase_ref) break; \
			{

#define scase(str) \
			} if (!_r__strcase_ref || (((str) != NULL) && \
					!strcmp ((str), _r__strcase_ref))) \
				{ _r__strcase_ref = NULL; 

#define sncase(str) \
			} if (!_r__strcase_ref || (((str) != NULL) && \
					!strncmp ((str), _r__strcase_ref, strlen (str)))) \
				{ _r__strcase_ref = NULL; 

#define sicase(str) \
			} if (!_r__strcase_ref || (((str) != NULL) && \
					!strcasecmp ((str), _r__strcase_ref))) \
				{ _r__strcase_ref = NULL; 

#define snicase(str) \
			} if (!_r__strcase_ref || (((str) != NULL) && \
					!strncasecmp ((str), _r__strcase_ref, strlen (str)))) \
				{ _r__strcase_ref = NULL; 

#define sincase(str) snicase(str)

#define sbreak break

#define sdefault  \
			} \
				{ _r__strcase_ref = NULL; 

#define sesac }}} 

#define esac sesac


#define once	switch(0)default:






#endif	/* _R__FRLIB_LIB_BASE_STRCASE_H */

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
