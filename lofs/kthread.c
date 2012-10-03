/**
 * lofs: Linux filesystem loopback layer
 * Kernel declarations.
 *
 * Copyright (C) 1997-2003 Erez Zadok
 * Copyright (C) 2001-2003 Stony Brook University
 * Copyright (C) 2004-2008 International Business Machines Corp.
 * Copyright (C) 2011-2012 Electric Cloud, Inc.
 *   Author(s): Eric Melski <ericm@electric-cloud.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "lofs_kernel.h"
#include <linux/dcache.h>
#ifdef HAVE_MANAGED_DENTRIES
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/mount.h>
#include <linux/slab.h>
#ifdef HAVE_FREEZER_H
#include <linux/freezer.h>
#define SET_FREEZABLE()                 set_freezable()
#define TRY_TO_FREEZE()
#define WAIT_EVENT_FREEZABLE            wait_event_freezable
#else
#define SET_FREEZABLE()
#define TRY_TO_FREEZE()                 try_to_freeze()
#define WAIT_EVENT_FREEZABLE            wait_event_interruptible
#endif

struct kmem_cache *lofs_lookup_req_cache;

static struct lofs_kthread_ctl {
#define LOFS_KTHREAD_ZOMBIE 0x1
    u32 flags;
    struct mutex mux;
    struct list_head req_list;
    wait_queue_head_t wait;
} lofs_kthread_ctl;

static struct task_struct *lofs_kthread;

/**
 * lofs_threadfn
 * @ignored: ignored
 *
 * The lofs kernel thread that has the responsibility of doing lookups of
 * automounted filesystems so that lofs can traverse into them.
 *
 * Returns zero on success; non-zero otherwise
 */
static int lofs_threadfn(void *ignored)
{
    static const int LOFS_LOOKUP_FLAGS = LOOKUP_FOLLOW | LOOKUP_DIRECTORY;

    SET_FREEZABLE();
    while (1)  {
        struct lofs_lookup_req *req;

        WAIT_EVENT_FREEZABLE(lofs_kthread_ctl.wait,
                (!list_empty(&lofs_kthread_ctl.req_list)
                        || kthread_should_stop()));
        TRY_TO_FREEZE();
        mutex_lock(&lofs_kthread_ctl.mux);
        if (lofs_kthread_ctl.flags & LOFS_KTHREAD_ZOMBIE) {
            mutex_unlock(&lofs_kthread_ctl.mux);
            goto out;
        }
        while (!list_empty(&lofs_kthread_ctl.req_list)) {
            req = list_first_entry(&lofs_kthread_ctl.req_list,
                    struct lofs_lookup_req, kthread_ctl_list);
            mutex_lock(&req->mux);
            list_del(&req->kthread_ctl_list);
            if (!(req->flags & LOFS_REQ_ZOMBIE)) {
                int rc = 0;
#if defined(HAVE_KERN_PATH)
                rc = kern_path(req->name, LOFS_LOOKUP_FLAGS, req->p);
#else
                struct nameidata nd;
                nd.dentry = req->p->dentry;
                nd.mnt    = req->p->mnt;
                rc = path_lookup(req->name, LOFS_LOOKUP_FLAGS, &nd);
                req->p->dentry = nd.dentry;
                req->p->mnt    = nd.mnt;
#endif
                req->flags |= LOFS_REQ_PROCESSED;
                if (rc) {
                    req->flags |= LOFS_REQ_ERROR;
                }
            }
            wake_up(&req->wait);
            mutex_unlock(&req->mux);
        }
        mutex_unlock(&lofs_kthread_ctl.mux);
    }
out:
    return 0;
}

int lofs_init_kthread(void)
{
    int rc = 0;
    mutex_init(&lofs_kthread_ctl.mux);
    init_waitqueue_head(&lofs_kthread_ctl.wait);
    INIT_LIST_HEAD(&lofs_kthread_ctl.req_list);
    lofs_kthread = kthread_run(&lofs_threadfn, NULL, "lofs-kthread");
    if (IS_ERR(lofs_kthread)) {
        rc = PTR_ERR(lofs_kthread);
        printk(KERN_ERR "%s: Failed to create kernel thread; rc = [%d]"
                "\n", __func__, rc);
    }
    return rc;
}

void lofs_destroy_kthread(void)
{
    struct lofs_lookup_req *req;

    mutex_lock(&lofs_kthread_ctl.mux);
    lofs_kthread_ctl.flags |= LOFS_KTHREAD_ZOMBIE;
    list_for_each_entry(req, &lofs_kthread_ctl.req_list, kthread_ctl_list) {
        mutex_lock(&req->mux);
        req->flags |= LOFS_REQ_ZOMBIE;
        wake_up(&req->wait);
        mutex_unlock(&req->mux);
    }
    mutex_unlock(&lofs_kthread_ctl.mux);
    kthread_stop(lofs_kthread);
    wake_up(&lofs_kthread_ctl.wait);
}

/**
 * lofs_lookup_managed
 *
 * This function does a lookup on a managed dentry, to try to provoke the
 * kernel into automounting the filesystem there. Returns zero on success;
 * non-zero otherwise.
 */

int lofs_lookup_managed(
    char *name,                         /* Abs. path to lookup. */
    struct path *path)                  /* Resulting lookup goes here. */
{
    struct lofs_lookup_req *req;
    int rc = 0;

    req = kmem_cache_alloc(lofs_lookup_req_cache, GFP_KERNEL);
    if (!req) {
        rc = -ENOMEM;
        goto out;
    }
    mutex_init(&req->mux);
    req->name = name;
    req->p    = path;
    init_waitqueue_head(&req->wait);
    req->flags = 0;
    mutex_lock(&lofs_kthread_ctl.mux);
    if (lofs_kthread_ctl.flags & LOFS_KTHREAD_ZOMBIE) {
        rc = -EIO;
        mutex_unlock(&lofs_kthread_ctl.mux);
        printk(KERN_ERR "%s: We are in the middle of shutting down; "
                "aborting request to lookup managed path %s\n",
                __func__, name);
        goto out_free;
    }
    list_add_tail(&req->kthread_ctl_list, &lofs_kthread_ctl.req_list);
    mutex_unlock(&lofs_kthread_ctl.mux);
    wake_up(&lofs_kthread_ctl.wait);
    wait_event(req->wait, (req->flags != 0));
    mutex_lock(&req->mux);
    BUG_ON(req->flags == 0);
    if (req->flags & LOFS_REQ_ZOMBIE) {
        rc = -EIO;
        printk(KERN_WARNING "%s: Managed lookup request dropped\n", __func__);
    } else if (req->flags & LOFS_REQ_ERROR) {
        rc = -ENOENT;
    }
    mutex_unlock(&req->mux);

out_free:
    kmem_cache_free(lofs_lookup_req_cache, req);
out:
    return rc;
}
#else
/* Stubs for older versions of Linux, simplifies the code structure. */

#include "lofs_kernel.h"
struct kmem_cache *lofs_lookup_req_cache;
int lofs_init_kthread(void) { return 0; }
void lofs_destroy_kthread(void) { }

#endif
