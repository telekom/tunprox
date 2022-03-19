/*
 * Copyright (C) 2015-2021 by Frank Reker, Deutsche Telekom AG
 *
 * LDT - Lightweight (MP-)DCCP Tunnel kernel module
 *
 * This is not Open Source software. 
 * This work is made available to you under a source-available license, as 
 * detailed below.
 *
 * Copyright 2022 Deutsche Telekom AG
 *
 * Permission is hereby granted, free of charge, subject to below Commons 
 * Clause, to any person obtaining a copy of this software and associated 
 * documentation files (the "Software"), to deal in the Software without 
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 * “Commons Clause” License Condition v1.0
 *
 * The Software is provided to you by the Licensor under the License, as
 * defined below, subject to the following condition.
 *
 * Without limiting other conditions in the License, the grant of rights under
 * the License will not include, and the License does not grant to you, the
 * right to Sell the Software.
 *
 * For purposes of the foregoing, “Sell” means practicing any or all of the
 * rights granted to you under the License to provide to third parties, for a
 * fee or other consideration (including without limitation fees for hosting 
 * or consulting/ support services related to the Software), a product or 
 * service whose value derives, entirely or substantially, from the
 * functionality of the Software. Any license notice or attribution required
 * by the License must also include this Commons Clause License Condition
 * notice.
 *
 * Licensor: Deutsche Telekom AG
 */

#include <linux/skbuff.h>
//#include <linux/lockdep.h>
#include "ldt_queue.h"



//static struct lock_class_key	lock_key;

struct tp_queue_ops {
	void					(*enqueue) (struct tp_queue*, struct sk_buff*);
	struct sk_buff*	(*dequeue) (struct tp_queue*);
	void					(*requeue) (struct tp_queue*, struct sk_buff*);
	int					(*isfull)  (struct tp_queue*);
};

static void tpq_inf_enqueue (struct tp_queue*, struct sk_buff*);
static struct sk_buff* tpq_inf_dequeue (struct tp_queue*);
static void tpq_inf_requeue (struct tp_queue*, struct sk_buff*);
static int tpq_limit_isfull (struct tp_queue*);
static void tpq_drop_oldest_enqueue (struct tp_queue*, struct sk_buff*);
static void tpq_drop_newest_enqueue (struct tp_queue*, struct sk_buff*);


static struct tp_queue_ops queue_tbl[TP_QUEUE_MAX+1] = {
	[TP_QUEUE_INF] = {
		.enqueue = tpq_inf_enqueue,
		.dequeue = tpq_inf_dequeue,
		.requeue = tpq_inf_requeue,
		.isfull = NULL,
	},
	[TP_QUEUE_LIMIT] = {
		.enqueue = tpq_inf_enqueue,
		.dequeue = tpq_inf_dequeue,
		.requeue = tpq_inf_requeue,
		.isfull = tpq_limit_isfull,
	},
	[TP_QUEUE_DROP_OLDEST] = {
		.enqueue = tpq_drop_oldest_enqueue,
		.dequeue = tpq_inf_dequeue,
		.requeue = tpq_inf_requeue,
		.isfull = NULL,
	},
	[TP_QUEUE_DROP_NEWEST] = {
		.enqueue = tpq_drop_newest_enqueue,
		.dequeue = tpq_inf_dequeue,
		.requeue = tpq_inf_requeue,
		.isfull = NULL,
	},
};


void
tpq_init (queue, policy, maxlen)
	struct tp_queue	*queue;
	int					policy;
	int					maxlen;
{
	if (!queue) return;
	*queue = (struct tp_queue) { .q_maxlen = 1 };
	skb_queue_head_init(&queue->queue);
#if 0
	lockdep_set_class_and_name(&queue->queue.lock,
								&lock_key, "tp_xmit_lock");
#endif
	tpq_set_policy (queue, policy);
	tpq_set_maxlen (queue, maxlen);
}

void
tpq_set_policy (queue, policy)
	struct tp_queue	*queue;
	int					policy;
{
	if (!queue) return;
	if (policy < 0 || policy > TP_QUEUE_MAX) return;
	queue->policy = policy;
}

void
tpq_set_maxlen (queue, maxlen)
	struct tp_queue	*queue;
	int					maxlen;
{
	if (!queue) return;
	if (maxlen < 1) return;
	queue->q_maxlen = maxlen;
}

void
tpq_destroy (queue)
	struct tp_queue	*queue;
{
	struct sk_buff	*skb;

	if (!queue) return;
	while ((skb = tpq_inf_dequeue (queue))) {
		kfree (skb);
	}
	*queue = (struct tp_queue) { .q_maxlen = 0 };
}
	

void
tpq_enqueue (queue, skb)
	struct tp_queue	*queue;
	struct sk_buff		*skb;
{
	if (!queue || !skb || (unsigned) queue->policy > TP_QUEUE_MAX) return;
	if (!queue_tbl[queue->policy].enqueue) return;
	queue_tbl[queue->policy].enqueue (queue, skb);
}

struct sk_buff*
tpq_dequeue (queue)
	struct tp_queue	*queue;
{
	if (!queue || (unsigned) queue->policy > TP_QUEUE_MAX) return NULL;
	if (!queue_tbl[queue->policy].dequeue) return NULL;
	return queue_tbl[queue->policy].dequeue (queue);
}

void
tpq_requeue (queue, skb)
	struct tp_queue	*queue;
	struct sk_buff		*skb;
{
	if (!queue || !skb || (unsigned) queue->policy > TP_QUEUE_MAX) return;
	if (!queue_tbl[queue->policy].requeue) return;
	queue_tbl[queue->policy].requeue (queue, skb);
}

int
tpq_isfull (queue)
	struct tp_queue	*queue;
{
	if (!queue || (unsigned) queue->policy > TP_QUEUE_MAX) return 0;
	if (!queue_tbl[queue->policy].isfull) return 0;
	return queue_tbl[queue->policy].isfull (queue);
}





/* 
 * infinity queue
 */

static
void
tpq_inf_enqueue (queue, skb)
	struct tp_queue	*queue;
	struct sk_buff		*skb;
{
	skb_queue_head (&queue->queue, skb);
}

static
struct sk_buff*
tpq_inf_dequeue (queue)
	struct tp_queue	*queue;
{
	struct sk_buff	*skb;

	skb = skb_peek_tail (&queue->queue);
	if (skb) skb_unlink (skb, &queue->queue);
	return skb;
}

static
void
tpq_inf_requeue (queue, skb)
	struct tp_queue	*queue;
	struct sk_buff		*skb;
{
	skb_queue_tail(&queue->queue, skb);
}


/*
 * limited queue
 */

static
int
tpq_limit_isfull (queue)
	struct tp_queue	*queue;
{
	return queue->queue.qlen >= queue->q_maxlen;
}


/*
 * drop oldest queue
 */

static
void
tpq_drop_oldest_enqueue (queue, skb)
	struct tp_queue	*queue;
	struct sk_buff		*skb;
{
	struct sk_buff	*oskb;

	if (tpq_limit_isfull (queue)) {
		oskb = tpq_inf_dequeue (queue);
		if (oskb) {
			kfree_skb (oskb);
		}
	}
	tpq_inf_enqueue (queue, skb);
}


/* 
 * drop newest queue
 */

static
void
tpq_drop_newest_enqueue (queue, skb)
	struct tp_queue	*queue;
	struct sk_buff		*skb;
{
	if (tpq_limit_isfull (queue)) {
		kfree_skb (skb);
	} else {
		tpq_inf_enqueue (queue, skb);
	}
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
