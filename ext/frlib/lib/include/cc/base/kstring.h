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

#ifndef _R__FRLIB_LIB_CC_BASE_KSTRING_H
#define _R__FRLIB_LIB_CC_BASE_KSTRING_H



/* the standard string class contains thousands of errors
	and it is easier to reimplement it rather than to 
	work around all bugs
 */

#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>


class kstring {
public:
	char	* data;
	size_t	buflen;

	kstring () { data = NULL; buflen = 0; };
	kstring (kstring &str) { 
		if (!str.data) {
			data = NULL; buflen = 0; 
		} else {
			data = strdup (str.data); buflen = strlen (data);
		}
	};
	kstring (const kstring &str) {
		if (!str.data) {
			data = NULL; buflen = 0;
		} else {
			data = strdup (str.data); buflen = strlen (data);
		}
	};
	kstring (const char * str) {
		if (!str) {
			data = NULL; buflen = 0;
		} else {
			data = strdup (str); buflen = strlen (data);
		}
	};
	kstring (const char * str, size_t len) {
		data = NULL; buflen = 0;
		asign (str, len);
	};
	kstring (const kstring &str, size_t len) {
		data = NULL; buflen = 0;
		asign (str, len);
	};
	kstring (char c) {
		data = NULL; buflen = 0;
		asign (c);
	};
	kstring (int i) {
		data = NULL; buflen = 0;
		asign (i);
	};
	kstring (double f) {
		data = NULL; buflen = 0;
		asign (f);
	};
	~kstring () {
		if (data) free ((void *) data);
		data = NULL;
	}
	size_t size () const;
	bool is () const { return data && *data; }
	char * str () const;
	const char * c_str () const;

	kstring& asign (const kstring &str);
	kstring& asign (const kstring &str, size_t len);
	kstring& asign (const char * str);
	kstring& asign (const char * str, size_t len);
	kstring& asign (char c);
	kstring& asign (int i);
	kstring& asign (double f);
	kstring& operator= (const kstring &str) { return asign (str); }
	kstring& operator= (const char * str) { return asign (str); }
	kstring& operator= (char c) { return asign (c); }
	kstring& operator= (int i) { return asign (i); }
	kstring& operator= (double f) { return asign (f); }

	kstring& cat (const kstring & str);
	kstring& cat (const kstring & str, size_t len);
	kstring& cat (const char * str);
	kstring& cat (const char * str, size_t len);
	kstring& cat (char c);
	kstring& cat (int i);
	kstring& cat (double f);
	kstring& operator+= (const kstring &str) { return cat (str); }
	kstring& operator+= (const char * str) { return cat (str); }
	kstring& operator+= (char c) { return cat (c); }
	kstring& operator+= (int i) { return cat (i); }
	kstring& operator+= (double f) { return cat (f); }

	kstring operator+ (const kstring &str) const {
		kstring hstr (data);
		return hstr += str; };
	kstring operator+ (const char * str) const {
		kstring hstr (data);
		return hstr += str; };
	kstring operator+ (char c) const {
		kstring hstr (data);
		return hstr += c; };
	kstring operator+ (int i) const {
		kstring hstr (data);
		return hstr += i; };
	kstring operator+ (double f) const {
		kstring hstr (data);
		return hstr += f; };
	friend kstring operator+ (const char * str1, const kstring &str2) {
		kstring hstr (str1);
		return hstr += str2; };
	friend kstring operator+ (char c, const kstring &str2) {
		kstring hstr (c);
		return hstr += str2; };
	friend kstring operator+ (int i, const kstring &str2) {
		kstring hstr (i);
		return hstr += str2; };
	friend kstring operator+ (double f, const kstring &str2) {
		kstring hstr (f);
		return hstr += str2; };

	int cmp (const char * str) const;
	int cmp (const char * str, size_t len) const;
	int cmp (const kstring &str) const { return cmp (str.data); };
	int cmp (const kstring &str, size_t len) const {
		return cmp (str.data, len); };
	int cmp (char c) const { return cmp (kstring (c).c_str()); };
	int cmp (char c, size_t len) const { 
		return cmp (kstring (c).c_str(), len); };
	int cmp (int i) const { return cmp (kstring (i).c_str()); };
	int cmp (int i, size_t len) const { 
		return cmp (kstring (i).c_str(), len); };
	int cmp (double f) const { return cmp (kstring (f).c_str()); };
	int cmp (double f, size_t len) const {
		return cmp (kstring (f).c_str(), len); };
	int icmp (const char * str) const ;
	int icmp (const char * str, size_t len) const;
	int icmp (const kstring &str) const { return icmp (str.data); };
	int icmp (const kstring &str, size_t len) const { 
		return icmp (str.data, len); };
	int icmp (char c) const { return icmp (kstring (c).c_str()); };
	int icmp (char c, size_t len) const { 
		return icmp (kstring (c).c_str(), len); };
	bool operator== (const kstring &str) const { return cmp (str) == 0; }
	bool operator== (const char * str) const { return cmp (str) == 0; }
	bool operator== (char c) const { return cmp (c) == 0; }
	bool operator== (int i) const { return cmp (i) == 0; }
	bool operator== (double f) const { return cmp (f) == 0; }
	bool operator< (const kstring &str) const { return cmp (str) < 0; }
	bool operator< (const char * str) const { return cmp (str) < 0; }
	bool operator< (char c) const { return cmp (c) < 0; }
	bool operator< (int i) const { return cmp (i) < 0; }
	bool operator< (double f) const { return cmp (f) < 0; }
	bool operator<= (const kstring &str) const { return cmp (str) <= 0; }
	bool operator<= (const char * str) const { return cmp (str) <= 0; }
	bool operator<= (char c) const { return cmp (c) <= 0; }
	bool operator<= (int i) const { return cmp (i) <= 0; }
	bool operator<= (double f) const { return cmp (f) <= 0; }
	bool operator> (const kstring &str) const { return cmp (str) > 0; }
	bool operator> (const char * str) const { return cmp (str) > 0; }
	bool operator> (char c) const { return cmp (c) > 0; }
	bool operator> (int i) const { return cmp (i) > 0; }
	bool operator> (double f) const { return cmp (f) > 0; }
	bool operator>= (const kstring &str) const { return cmp (str) >= 0; }
	bool operator>= (const char * str) const { return cmp (str) >= 0; }
	bool operator>= (char c) const { return cmp (c) >= 0; }
	bool operator>= (int i) const { return cmp (i) >= 0; }
	bool operator>= (double f) const { return cmp (f) >= 0; }
	friend bool operator== (const char * str, const kstring &str2)
		{ return str2.cmp (str) == 0; }
	friend bool operator== (char c, const kstring &str2)
		{ return str2.cmp (c) == 0; }
	friend bool operator== (int i, const kstring &str2)
		{ return str2.cmp (i) == 0; }
	friend bool operator== (double f, const kstring &str2)
		{ return str2.cmp (f) == 0; }
	friend bool operator< (const char * str, const kstring &str2)
		{ return str2.cmp (str) > 0; }
	friend bool operator< (char c, const kstring &str2)
		{ return str2.cmp (c) > 0; }
	friend bool operator< (int i, const kstring &str2) 
		{ return str2.cmp (i) > 0; }
	friend bool operator< (double f, const kstring &str2)
		{ return str2.cmp (f) > 0; }
	friend bool operator<= (const char * str, const kstring &str2)
		{ return str2.cmp (str) >= 0; }
	friend bool operator<= (char c, const kstring &str2)
		{ return str2.cmp (c) >= 0; }
	friend bool operator<= (int i, const kstring &str2)
		{ return str2.cmp (i) >= 0; }
	friend bool operator<= (double f, const kstring &str2)
		{ return str2.cmp (f) >= 0;}
	friend bool operator> (const char * str, const kstring &str2)
		{ return str2.cmp (str) < 0; }
	friend bool operator> (char c, const kstring &str2)
		{ return str2.cmp (c) < 0; }
	friend bool operator> (int i, const kstring &str2)
		{ return str2.cmp (i) < 0; }
	friend bool operator> (double f, const kstring &str2)
		{ return str2.cmp (f) < 0;}
	friend bool operator>= (const char * str, const kstring &str2)
		{ return str2.cmp (str) <= 0; }
	friend bool operator>= (char c, const kstring &str2)
		{ return str2.cmp (c) <= 0; }
	friend bool operator>= (int i, const kstring &str2)
		{ return str2.cmp (i) <= 0; }
	friend bool operator>= (double f, const kstring &str2)
		{ return str2.cmp (f) <= 0;}
	kstring& printf (const char * fmt, ...)
					__attribute__((format(printf, 2, 3)));
	kstring& nprintf (size_t n, const char * fmt, ...)
					__attribute__((format(printf, 3, 4)));
	kstring& vprintf (const char * fmt, va_list ap);
	kstring& vnprintf (size_t n, const char * fmt, va_list ap);
	kstring& catprintf (const char * fmt, ...)
					__attribute__((format(printf, 2, 3)));
	kstring& catnprintf (size_t n, const char * fmt, ...)
					__attribute__((format(printf, 3, 4)));
	kstring& catvprintf (const char * fmt, va_list ap);
	kstring& catvnprintf (size_t n, const char * fmt, va_list ap);
};

extern const kstring KSTRING_NULL;















#endif	/* _R__FRLIB_LIB_CC_BASE_KSTRING_H */

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
