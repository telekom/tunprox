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

#ifndef _R__FRLIB_LIB_CC_BUFFER_BUFFER_H
#define _R__FRLIB_LIB_CC_BUFFER_BUFFER_H


#include <fr/buffer/buffer.h>
#include <fr/base/errors.h>


class Buffer {
public:
	Buffer ()
	{
		buf = bufopen (BUF_TYPE_NULL, BUF_F_NOLINK);
	};
	Buffer (Buffer &obuf)
	{
		obuf.setFlags (BUF_F_NOLINK);
		if (bufdup (&buf, obuf.buf) != RERR_OK) buf=NULL;
	};
	Buffer (int type, uint32_t flags)
	{
		buf = bufopen (type, flags|BUF_F_NOLINK);
	};
	Buffer (int type)
	{
		buf = bufopen (type, BUF_F_NOLINK);
	};
	~Buffer ()
	{
		bufclose (buf);
	};

	int setFileName (char *filename)
	{
		return bufsetfilename (buf, filename);
	};
	int setFlags (uint32_t flags)
	{
		return bufsetflags (buf, flags);
	};
	int unsetFlags (uint32_t flags)
	{
		return bufunsetflags (buf, flags);
	};
	int setMaxSize (uint32_t max_size)
	{
		return bufsetmaxsize (buf, max_size);
	};
	int setMaxMemSize (uint32_t max_mem_size)
	{
		return bufsetmaxmemsize (buf, max_mem_size);
	};
	int resetOverflow ()
	{
		return bufresetoverflow (buf);
	};

	char *getFilename ()
	{
		return bufgetfilename (buf);
	};
	uint32_t getFlags ()
	{
		return bufgetflags (buf);
	};
	uint32_t getMaxSize ()
	{
		return bufgetmaxsize (buf);
	};
	uint32_t getMaxMemSize ()
	{
		return bufgetmaxmemsize (buf);
	};
	int getType ()
	{
		return bufgettype (buf);
	};
	int getOrigType ()
	{
		return bufgetorigtype (buf);
	};
	int getOverflow ()
	{
		return bufoverflow (buf);
	};
	uint32_t getCount ()
	{
		return bufcount (buf);
	};
	uint32_t getLen ()
	{
		return buflen (buf);
	};
	uint32_t len ()
	{
		return buflen (buf);
	};

	int print (const char *str)
	{
		return bufprint (buf, str);
	};
	int nprint (const char *str, uint32_t len)
	{
		return bufnprint (buf, str, len);
	};
	int putStr (char *str)
	{
		return bufputstr (buf, str);
	};
	int nputStr (char *str, uint32_t len)
	{
		return bufnputstr (buf, str, len);
	};
	int putChar (const char c)
	{
		return bufputc (buf, c);
	};
	int putUniChar (uint32_t c)
	{
		return bufputunic (buf, c);
	};

	int printf (const char *fmt, ...) __attribute__((format(printf, 2, 3)))
	{
		va_list	ap;
		int		ret;
		va_start (ap, fmt);
		ret = bufvprintf (buf, fmt, ap);
		va_end (ap);
		return ret;
	};
	int vprintf (const char *fmt, va_list ap)
	{
		return bufvprintf (buf, fmt, ap);
	};
	int nprintf (uint32_t num, const char *fmt, ...) __attribute__((format(printf, 3, 4)))
	{
		va_list	ap;
		int		ret;
		va_start (ap, fmt);
		ret = bufvnprintf (buf, num, fmt, ap);
		va_end (ap);
		return ret;
	};
	int vnprintf (uint32_t num, const char *fmt, va_list ap)
	{
		return bufvnprintf (buf, num, fmt, ap);
	};
	int tprintf (const char *fmt, ...) __attribute__((format(printf, 2, 3)))
	{
		va_list	ap;
		int		ret;
		va_start (ap, fmt);
		ret = bufvtprintf (buf, fmt, ap);
		va_end (ap);
		return ret;
	};
	int vtprintf (const char *fmt, va_list ap)
	{
		return bufvtprintf (buf, fmt, ap);
	};

	char *getStr ()
	{
		return bufgetstr (buf);
	};
	int getStr2 (char **dest)
	{
		return bufgetstr2 (buf, dest);
	};
	int printToStr (char *dest)
	{
		return bufprints (buf, dest);
	};
	int nprintToStr (char *dest, uint32_t len)
	{
		return bufnprints (buf, dest, len);
	};
	int printToFile (FILE *f)
	{
		return bufprintout (buf, f);
	};
	int nprintToFile (FILE *f, uint32_t len)
	{
		return bufnprintout (buf, f, len);
	};
	
	int apend (Buffer &src)
	{
		return bufcp (buf, src.buf);
	};
	int cp (Buffer &src)
	{
		return bufcp (buf, src.buf);
	};
	int apendClean (Buffer &src)
	{
		return bufmv (buf, src.buf);
	};
	int mv (Buffer &src)
	{
		return bufmv (buf, src.buf);
	};
	int convert (int newtype)
	{
		return bufconvert (buf, newtype);
	};

	int trunc (uint32_t len)
	{
		return buftrunc (buf, len);
	};
	int clean ()
	{
		return bufclean (buf);
	};

	int evalLen ()
	{
		return bufevallen (buf);
	};
	void* getRef ()
	{
		return bufgetref (buf);
	};
	int openFile ()
	{
		return buffileopen (buf);
	};
	int closeFile (int force)
	{
		return buffileclose (buf, force);
	};

protected:
	BUFFER	buf;
};






#endif	/* _R__FRLIB_LIB_CC_BUFFER_BUFFER_H */


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
