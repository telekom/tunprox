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

#ifndef _R__FRLIB_LIB_REGISTRY_WINREG_H
#define _R__FRLIB_LIB_REGISTRY_WINREG_H




#include <stdint.h>



typedef uint32_t		DWORD;
typedef int32_t		HKEY;
typedef uint32_t		REGSAM;

typedef HKEY*			PHKEY;
typedef char*  		LPCTSTR;
typedef char*			LPTSTR;
typedef uint32_t*		LPDWORD;
typedef char*			LPBYTE;
typedef HKEY			KEY;
typedef char*			PCTSTR;
typedef char*			PTSTR;
typedef char			BYTE;
typedef int32_t		LONG;

#define CONST			const






LONG RegOpenKeyEx (
	HKEY		hKey, 		/* handle of open key */
	LPCTSTR	lpSubKey, 	/* address of name of subkey to open */
	DWORD		ulOptions, 	/* reserved */
	REGSAM	samDesired,	/* security access mask */
	PHKEY		phkResult	/* address of handle of open key */
);


LONG RegCloseKey (
	HKEY	hKey		/* handle of key to close */
);

LONG RegCreateKey (
	HKEY		hKey,			/* handle of an open key */
	LPCTSTR	lpSubKey,	/* address of name of subkey to open */
	PHKEY		phkResult	/* address of buffer for opened handle */
);

LONG RegQueryValueEx (
	HKEY		hKey,			/* handle of key to query */
	LPTSTR	lpValueName,/* address of name of value to query */
	LPDWORD	lpReserved,	/* reserved */
	LPDWORD	lpType,		/* address of buffer for value type */
	LPBYTE	lpData,		/* address of data buffer */
	LPDWORD	lpcbData		/* address of data buffer size */
);

LONG RegEnumValue (
	HKEY		hKey,				/* handle of key to query */
	DWORD		dwIndex,			/* index of value to query */
	LPTSTR	lpValueName,	/* address of buffer for value string */
	LPDWORD	lpcbValueName,	/* address for size of value buffer */
	LPDWORD	lpReserved,		/* reserved */
	LPDWORD	lpType,			/* address of buffer for type code */
	LPBYTE	lpData,			/* address of buffer for value data */
	LPDWORD	lpcbData			/* address for size of data buffer */
);


LONG RegSetValueEx (
	KEY			hKey,			/* handle of key to set value for */
	PCTSTR		lpValueName,/* address of value to set */
	DWORD			Reserved,	/* reserved */
	DWORD			dwType,		/* flag for value type */
	CONST BYTE	*lpData,		/* address of value data */
	DWORD			cbData		/* size of value data */
);








#define HKEY_BASE					(-2147483648L)
#define HKEY_CLASSES_ROOT		((HKEY)(HKEY_BASE+0))
#define HKEY_CURRENT_USER 		((HKEY)(HKEY_BASE+1))
#define HKEY_LOCAL_MACHINE		((HKEY)(HKEY_BASE+2))
#define HKEY_USERS				((HKEY)(HKEY_BASE+3))
#define HKEY_PERFORMANCE_DATA	((HKEY)(HKEY_BASE+4))
#define HKEY_CURRENT_CONFIG	((HKEY)(HKEY_BASE+5))
#define HKEY_DYN_DATA			((HKEY)(HKEY_BASE+6))

#define DELETE							(0x00010000L)
#define READ_CONTROL					(0x00020000L)
#define WRITE_DAC						(0x00040000L)
#define WRITE_OWNER					(0x00080000L)
#define SYNCHRONIZE					(0x00100000L)
#define STANDARD_RIGHTS_REQUIRED	(0x000F0000L)
#define STANDARD_RIGHTS_READ		(READ_CONTROL)
#define STANDARD_RIGHTS_WRITE		(READ_CONTROL)
#define STANDARD_RIGHTS_EXECUTE	(READ_CONTROL)
#define STANDARD_RIGHTS_ALL		(0x001F0000L)
#define SPECIFIC_RIGHTS_ALL		(0x0000FFFFL)

#define KEY_QUERY_VALUE				(0x0001)
#define KEY_SET_VALUE				(0x0002)
#define KEY_CREATE_SUB_KEY			(0x0004)
#define KEY_ENUMERATE_SUB_KEYS	(0x0008)
#define KEY_NOTIFY					(0x0010)
#define KEY_CREATE_LINK				(0x0020)
#define KEY_READ						((STANDARD_RIGHTS_READ		|\
												KEY_QUERY_VALUE			|\
												KEY_ENUMERATE_SUB_KEYS	|\
												KEY_NOTIFY)				\
												&						\
												(~SYNCHRONIZE))
#define KEY_WRITE						((STANDARD_RIGHTS_WRITE		|\
													KEY_SET_VALUE			|\
													KEY_CREATE_SUB_KEY)		\
													&						\
													(~SYNCHRONIZE))
#define KEY_EXECUTE					((KEY_READ)					\
													&						\
													(~SYNCHRONIZE))
#define KEY_ALL_ACCESS				((STANDARD_RIGHTS_ALL		|\
												KEY_QUERY_VALUE			|\
												KEY_SET_VALUE			|\
												KEY_CREATE_SUB_KEY		|\
												KEY_ENUMERATE_SUB_KEYS	|\
												KEY_NOTIFY				|\
												KEY_CREATE_LINK)		\
												&						\
												(~SYNCHRONIZE))



#define REG_DWORD	2
#define REG_SZ		3



















#endif	/* __R__FRLIB_LIB_REGISTRY_WINREG_H */

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
