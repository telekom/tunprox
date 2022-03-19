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


#ifndef _R__FRLIB_LIB_BASE_MISC_H
#define _R__FRLIB_LIB_BASE_MISC_H


#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>


#ifdef __cplusplus
extern "C" {
#endif


int numbits32 (uint32_t a);
int numbits64 (uint64_t a);

uint64_t distributebits64 (uint64_t a, uint64_t mask);
uint32_t distributebits32 (uint32_t a, uint32_t mask);




#define tmalloc(typ,num)	((typ*)malloc(sizeof(typ)*num))

#if defined __GNUC__
#	define FR__FUNC__ __PRETTY_FUNCTION__
#	define FR__FUNC2__ __PRETTY_FUNCTION__
#elif defined __MWERKS__ && (__MWERKS__ >= 0x3000)
#	define FR__FUNC__ __PRETTY_FUNCTION__
#	define FR__FUNC2__ __PRETTY_FUNCTION__
#elif defined __ICC && (__ICC >= 600)
#	define FR__FUNC__ __PRETTY_FUNCTION__
#	define FR__FUNC2__ __PRETTY_FUNCTION__
#elif defined __DMC__ && (__DMC__ >= 0x810)
#	define FR__FUNC__ __PRETTY_FUNCTION__
#	define FR__FUNC2__ __PRETTY_FUNCTION__
#elif defined __INTEL_COMPILER && (__INTEL_COMPILER >= 600)
#	define FR__FUNC__ __FUNCTION__
#	define FR__FUNC2__ __FUNCTION__
#elif defined __IBMCPP__ && (__IBMCPP__ >= 500)
#	define FR__FUNC__ __FUNCTION__
#	define FR__FUNC2__ __FUNCTION__
#elif defined __BORLANDC__ && (__BORLANDC__ >= 0x550)
#	define FR__FUNC__ __FUNC__
#	define FR__FUNC2__ __FUNC__
#elif defined __FUNCSIG__
#	define FR__FUNC__ __FUNCSIG__
#	define FR__FUNC2__ __FUNCSIG__
#elif defined __STDC_VERSION__ && (__STDC_VERSION__ >= 199901)
#	define FR__FUNC__ __func__
#	define FR__FUNC2__ __func__
#else
#	define FR__FUNC__ NULL
#	define FR__FUNC2__ ((char*)"<unknown>")
#endif



#if defined va_copy
#  define frvacopy(new,old)	va_copy ((new), (old))
#  define frvacopyend(ap)		va_end ((ap))
#elif defined __va_copy
#  define frvacopy(new,old)	__va_copy ((new), (old))
#  define frvacopyend(ap)		va_end ((ap))
#else
#  define frvacopy(new,old)	memcpy (&(new), &(old), sizeof (va_list));
#  define frvacopyend(ap)
#endif



#ifdef __cplusplus
}	/* extern "C" */
#endif






























#endif	/* _R__FRLIB_LIB_BASE_MISC_H */

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
