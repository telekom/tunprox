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

#ifndef _R__FRLIB_LIB_CC_SHM_SHM_H
#define _R__FRLIB_LIB_CC_SHM_SHM_H


#include <fr/shm/shm.h>
#include <fr/base/errors.h>
#include <stdlib.h>


class ShmStart {
public:
	int	ret;

	static ShmStart * getStart (char *name, int size, int flags)
	{
		if (gstart) return gstart;
		new ShmStart;
		if (!gstart) return NULL;
		gstart->ret = sm_start (name, size, flags);
		return gstart;
	};
	~ShmStart ()
	{
		sm_delstart ();
		gstart = NULL;
	};
	sm_fpage_t* getInfo ()
	{
		return sm_startpage_getinfo ();
	};
	void* getBase  ()
	{
		return sm_startpage_base ();
	};
	int getSize ()
	{
		return sm_startpage_size ();
	};
protected:
	static ShmStart	*gstart;

	ShmStart ()
	{
		ret = -1;
		if (gstart) delete gstart;
		gstart = this;
	};
};


void *sm_ptmap (sm_pt);
void *sm_ptmap2 (sm_pt*);
sm_pt sm_ptrev (void *);
void sm_ptcp (sm_pt *dest, sm_pt src);
void sm_ptset (sm_pt *dest, void *src);



class Shm {
public:
	int	id;

	Shm (int _id)
	{
		id = _id;
	};
	~Shm ()
	{
		sm_del (id);
		id=-1;
	};
	static Shm * newShm (int size)
	{
		int	_id, ret;
		ret = sm_new (&_id, size);
		if (!RERR_ISOK(ret)) return NULL;
		return new Shm (_id);
	};
	int mayDel ()
	{
		sm_page_t	*info = sm_page_getinfo (id);
		if (!info) return -1;
		return info->flags.deleted;
	};
	sm_page_t * getInfo ()
	{
		return sm_page_getinfo (id);
	};
	void * getBase ()
	{
		return sm_page_base (id);
	};
	int getSize ()
	{
		return sm_page_size (id);
	};
};














#endif	/* _R__FRLIB_LIB_CC_SHM_SHM_H */

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
