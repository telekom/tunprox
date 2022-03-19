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

#ifndef _R__FRLIB_LIB_SHM_SHM_H
#define _R__FRLIB_LIB_SHM_SHM_H

#include <stdint.h>
#include <fr/base/tmo.h>

#ifdef __cplusplus
extern "C" {
#endif


#define SM_MAGIC_PAGE	0x6ad083bf
#define SM_MAGIC_SPAGE	(SM_MAGIC_PAGE+1)
#define SM_MAGIC_FPAGE	(SM_MAGIC_SPAGE)

#define SM_F_NONE			0x00
#define SM_T_SHMOPEN		0x00
#define SM_T_VOLATILE	0x01
#define SM_T_PERMANENT	0x02
#define SM_T_SEARCH		0x03		/* not allowed for open */
#define SM_T_MASK			0x03
#define SM_F_CREAT		0x04
#define SM_F_RDONLY		0x08
#define SM_F_RECREAT		0x10
#define SM_F_STARTPAGE	0x20
#define SM_F_DESTROY		0x40		/* used for close */


typedef union sm_pageflags_t {
	uint32_t		cp;
	struct {
		uint32_t	type:2,
					valid:1,
					deleted:1,
					reserved:28;
	};
} sm_pageflags_t;

typedef uint64_t	sm_pt;
#define SM_PT_NULL ((sm_pt)(0xffffffff))

#define SM_PT_SMS(pt) ((uint32_t)(((pt)&0xffffffff00000000LL)>>32))
#define SM_PT_OFF(pt) ((uint32_t)((pt)&0xffffffffLL))
#define SM_PT_MK(sms,off) ((((uint64_t)(sms))<<32)|(((uint64_t)(off))&0xffffffffLL))
#define SM_PT_SETSMS(pt,sms)	(pt)=SM_PT_MK((sms),SM_PT_OFF(pt))
#define SM_PT_SETOFF(pt,off)	(pt)=SM_PT_MK(SM_PT_SMS(pt),(off))


typedef struct sm_page_t {
	uint32_t			magic;
	int32_t			id;
	uint32_t			size,
						offset,
						cpid;
	tmo_t				ctime;
	sm_pageflags_t	flags;
} sm_page_t;

typedef struct sm_fpage_t {
	uint32_t			magic;
	char				name[64];
	uint32_t			size,
						offset,
						cpid;
	tmo_t				ctime;
	sm_pageflags_t	flags;
} sm_fpage_t;


/*
 * fpage functions
 */
int sm_fopen (const char *name, int size, int flags);
int sm_fclose (const char *name, int flags);
int sm_getpname (char **outname, const char *pname);
int sm_fpage_exist (int *otype, const char *pname);
int sm_fpage_isopen (const char *name);


/*
 * startpage functions
 */
int sm_start (const char *name, int size, int flags);
int sm_delstart ();
int sm_stop ();
int sm_maydel ();


/*
 * page functions
 */
int sm_new (int *id, int size);
int sm_del (int id);
int sm_delall ();

/*
 * page pointer functions
 */
void *sm_ptmap (sm_pt);
void *sm_ptmap2 (sm_pt*);
sm_pt sm_ptrev (void *);
void sm_ptcp (sm_pt *dest, sm_pt src);
void sm_ptset (sm_pt *dest, void *src);


/*
 * startpage info functions
 */
sm_fpage_t *sm_startpage_getinfo ();
void * sm_startpage_base ();
int sm_startpage_size ();

/*
 * page info functions
 */
sm_page_t *sm_page_getinfo (int id);
void *sm_page_base (int id);
int sm_page_size (int id);

/*
 * fpage info functions
 */
sm_fpage_t *sm_fpage_getinfo (const char *name);
void *sm_fpage_base (const char *name);
int sm_fpage_size (const char *name);
const char *sm_fpage_getpage (void *addr);





#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_SHM_SHM_H */

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
