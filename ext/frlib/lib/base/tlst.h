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

#ifndef _R__FRLIB_LIB_BASE_TLST_H
#define _R__FRLIB_LIB_BASE_TLST_H

#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif


struct tlst {
	size_t	size;
	size_t	max;
	size_t	depth;
	size_t	num;
	unsigned	cnt;
	char		*data;
};

#define TLST_INIT(sz)		(struct tlst){.size = sz}
#define TLST_INIT_T(type)	TLST_INIT(sizeof(type))
#define TLST_SINIT(sz)		{.size = sz}
#define TLST_SINIT_T(type)	{.size = sizeof(type)}


#define TLST_F_NONE			0x00
#define TLST_F_CPYLAST		0x01
#define TLST_F_SHIFT			0x02
#define TLST_MASK_RM			0x03


/* initializes list */
int tlst_new (struct tlst*, size_t size);
/* puts data list into struct */
int tlst_putlst (struct tlst*, void *src, unsigned num, int flags);
/* apends element at end */
int tlst_add (struct tlst*, void *data);
/* inserts element at given position */
int tlst_set (struct tlst*, unsigned idx, void *data);
int tlst_getmax (struct tlst*);
int tlst_getnum (struct tlst*);
int tlst_reset (struct tlst*);
int tlst_get (void *outdata, struct tlst*, unsigned idx);
int tlst_getptr (void *outdata, struct tlst*, unsigned idx);
int tlst_free (struct tlst*);
int tlst_remove (struct tlst*, unsigned idx, int flags);
/* searches a free slot and inserts data there,
   data must be a (pointer to a) pointer or a struct with
   a pointer as first argument. If this pointer is NULL, the
	slot is considered to be free
 */
int tlst_addinsert (struct tlst*, void *data);
int tlst_cpy (struct tlst *dst, const struct tlst *src);

/* find searches all entrys */
int tlst_find (unsigned *idx, void *data, struct tlst*, int (*cmp)(void*,void*), void *key);
/* inserts element into sorted list */
int tlst_insert (struct tlst*, void *data, int (*cmpfunc)(void*,void*));
/* makes a binary search which is faster but assumes that the list is sorted */
int tlst_search (struct tlst*, void *data, int (*cmpfunc)(void*,void*));
/* compare functions - can be used for find, insert, search */
/* compare ints in first position */
int tlst_cmpint (void *ptr, void *val);
/* compare two ints in first position */
int tlst_cmpdblint (void *ptr, void *val);
/* compare 64 bit ints in first position */
int tlst_cmpint64 (void *ptr, void *val);
/* compare two 64 bit ints in first position */
int tlst_cmpdblint64 (void *ptr, void *val);
/* compare strings in first position */
int tlst_cmpstr (void *ptr, void *val);
/* compare strings case insensitive in first position */
int tlst_cmpistr (void *ptr, void *val);
/* compare two strings in first position */
int tlst_cmpdblstr (void *ptr, void *val);
/* compare two strings case insensitive in first position */
int tlst_cmpdblistr (void *ptr, void *val);


#define TLST_SET(list,idx,data)			tlst_set (&(list), (idx), &(data))
#define TLST_PUTLST(list,src,num)		tlst_putlst (&(list), (src), (num), 0)
#define TLST_LIMPSIZE						16
#define TLST_ADD(list,data)				tlst_add (&(list), &(data))
#define TLST_ADDINSERT(list,data)		tlst_addinsert (&(list), &(data))
#define TLST_GET(outdata,list,idx)		tlst_get (&(outdata), &(list), (idx))
#define TLST_GETPTR(outptr,list,idx)	tlst_getptr (&(outptr), &(list), (idx))
#define TLST_NEW(list,type)				tlst_new (&(list), sizeof (type))
#define TLST_FREE(list)						tlst_free (&(list))
#define TLST_GETMAX(list)					((list).max)
#define TLST_GETNUM(list)					((list).num)
#define TLST_RESET(list)					tlst_reset (&(list))
#define TLST_REMOVE(list,idx,flags)		tlst_remove (&(list),(idx),(flags))
#define TLST_FIND(idx,list,cmp,key)		tlst_find (&(idx),NULL,&(list),(cmp),&(key))
#define TLST_FINDINT(idx,list,key)		TLST_FIND((idx),(list),tlst_cmpint,(key))
#define TLST_HASINT(list,key)				tlst_find (NULL,NULL,&(list),tlst_cmpint,&(key))
#define TLST_INSERT(list,data,cmp)		tlst_insert(&(list),&(data),(cmp))
#define TLST_SEARCH(list,key,cmp)		tlst_search(&(list),&(key),(cmp))
#define TLST_CPY(dst,src)					tlst_cpy(&(dst),&(src))


#define TLST_FOREACH2(data,list,i)	\
				for ((i)=0; (size_t)(i) < (list).num && \
						RERR_ISOK (TLST_GET((data),(list),(i))); \
						(i)++)
#define TLST_FOREACH(data,list)	\
				TLST_FOREACH2((data),(list),(list).cnt)
#define TLST_FOREACHPTR2(ptr,list,i)	\
				for ((i)=0; (size_t)(i) < (list).num && \
						RERR_ISOK (TLST_GETPTR((ptr),(list),(i))); \
						(i)++)
#define TLST_FOREACHPTR(ptr,list)	\
				TLST_FOREACHPTR2((ptr),(list),(list).cnt)


/* Note, that i must be unsigned int, and if there is an underflow (i) 
   will become greater equal (list).num
 */
#define TLST_FOREACHREV2(data,list,i)	\
				for ((size_t)(i)=(list).num-1; (size_t)(i) < (list).num && \
						RERR_ISOK (TLST_GET((data),(list),(i))); \
						(i)--)
#define TLST_FOREACHREV(data,list)	\
				TLST_FOREACHREV2((data),(list),(list).cnt)
#define TLST_FOREACHPTRREV2(ptr,list,i)	\
				for ((i)=(list).num-1; (size_t)(i) < (list).num && \
						RERR_ISOK (TLST_GETPTR((ptr),(list),(i))); \
						(i)--)
#define TLST_FOREACHPTRREV(ptr,list)	\
				TLST_FOREACHPTR2((ptr),(list),(list).cnt)










#ifdef __cplusplus
}	/* extern "C" */
#endif








#endif	/* _R__FRLIB_LIB_BASE_TLST_H */
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
