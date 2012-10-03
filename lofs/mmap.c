/**
 * lofs: Linux loopback filesystem
 * 
 * This file declares methods related to handling memory mapped I/O on files in
 * lofs, which is achieved by sync'ing the lofs file pages with the lower file
 * pages.
 *
 * Copyright (C) 1997-2003 Erez Zadok
 * Copyright (C) 2001-2003 Stony Brook University
 * Copyright (C) 2004-2007 International Business Machines Corp.
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

#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/page-flags.h>
#include <linux/mount.h>
#include <linux/file.h>
#include <asm/unaligned.h>
#include "lofs_kernel.h"

/**
 * lofs_get_locked_page
 *
 * Get one page from cache or lower f/s, return error otherwise.
 *
 * Returns locked and up-to-date page (if ok), with increased
 * refcnt.
 */
struct page *lofs_get_locked_page(struct file *file, loff_t index)
{
    struct dentry *dentry;
    struct inode *inode;
    struct address_space *mapping;
    struct page *page;

    dentry = FILE_TO_DENTRY(file);
    inode = dentry->d_inode;
    mapping = inode->i_mapping;
    page = read_mapping_page(mapping, index, (void *)file);
    if (!IS_ERR(page)) {
        lock_page(page);
    }
    return page;
}

/**
 * lofs_writepage
 * @page: Page that is locked before this call is made
 *
 * Returns zero on success; non-zero otherwise
 */
static int lofs_writepage(struct page *page, struct writeback_control *wbc)
{
        int rc = 0;
        loff_t offset = (page->index << PAGE_CACHE_SHIFT);
        size_t length = PAGE_CACHE_SIZE;
        size_t maxIndex = (page->mapping->host->i_size >> PAGE_CACHE_SHIFT);
        int written = 0;

        if (page->index <= maxIndex) {
            if (page->index == maxIndex) {
                length = page->mapping->host->i_size & (PAGE_CACHE_SIZE - 1);
            }
            if (length) {
                char *data = kmap(page);
                written = lofs_write_lower(page->mapping->host, data, 
                        offset, length);
                kunmap(page);
                if (written != length) {
                    rc = written;
                    lofs_printk(KERN_WARNING, "Error writing "
                            "page (upper index [0x%.16x])\n", page->index);
                    ClearPageUptodate(page);
                    goto out;
                }
            }
            SetPageUptodate(page);
        }
        unlock_page(page);
out:
        return rc;
}

/**
 * lofs_readpage
 * @file: An lofs file
 * @page: Page from lofs inode mapping into which to stick the read data
 *
 * Read in a page.
 *
 * Returns zero on success; non-zero on error.
 */
static int lofs_readpage(struct file *file, struct page *page)
{
        int rc = 0;

        rc = lofs_read_lower_page(page, page->index, page->mapping->host);
        if (rc) {
            ClearPageUptodate(page);
        } else {
            SetPageUptodate(page);
        }
        unlock_page(page);
        return rc;
}

/**
 * lofs_write_begin
 * @file: The lofs file
 * @mapping: The lofs object
 * @pos: The file offset at which to start writing
 * @len: Length of the write
 * @flags: Various flags
 * @pagep: Pointer to return the page
 * @fsdata: Pointer to return fs data (unused)
 *
 * Returns zero on success; non-zero otherwise
 */
static int lofs_write_begin(struct file *file,
                        struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned flags,
                        struct page **pagep, void **fsdata)
{
    pgoff_t index = pos >> PAGE_CACHE_SHIFT;
    struct page *page;
    int rc = 0;

    page = grab_cache_page_write_begin(mapping, index, flags);
    if (!page) {
        return -ENOMEM;
    }
    *pagep = page;

    if (!PageUptodate(page)) {
        rc = lofs_read_lower_page(page, index, mapping->host);
        if (rc) {
            printk(KERN_ERR "%s: Error attemping to read "
                    "lower page; rc = [%d]\n",
                    __func__, rc);
            ClearPageUptodate(page);
        } else {
            SetPageUptodate(page);
        }
    }
    return rc;
}

/**
 * lofs_write_end
 * @file: The lofs file object
 * @mapping: The lofs object
 * @pos: The file position
 * @len: The length of the data (unused)
 * @copied: The amount of data copied
 * @page: The lofs page
 * @fsdata: The fsdata (unused)
 *
 * Write data through to the lower filesystem.
 */
static int lofs_write_end(struct file *file,
                        struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned copied,
                        struct page *page, void *fsdata)
{
    unsigned from = pos & (PAGE_CACHE_SIZE - 1);
    unsigned to = from + copied;
    struct inode *lofs_inode = mapping->host;
    int rc = 0;

    rc = lofs_write_lower_page_segment(lofs_inode, page, 0, to);
    if (!rc) {
        rc = copied;
        fsstack_copy_inode_size(lofs_inode, lofs_inode_to_lower(lofs_inode));
    }
    unlock_page(page);
    page_cache_release(page);
    return rc;
}

static sector_t lofs_bmap(struct address_space *mapping, sector_t block)
{
        int rc = 0;
        struct inode *inode;
        struct inode *lower_inode;

        inode = (struct inode *)mapping->host;
        lower_inode = lofs_inode_to_lower(inode);
        if (lower_inode->i_mapping->a_ops->bmap)
                rc = lower_inode->i_mapping->a_ops->bmap(lower_inode->i_mapping,
                                                         block);
        return rc;
}

#ifdef HAVE_ADDRESS_SPACE_OPS_EXT
#define ORIG_AOPS .orig_aops
#else
#define ORIG_AOPS
#endif

const ADDRESS_SPACE_OPS_T lofs_aops = {
    ORIG_AOPS.writepage = lofs_writepage,
    ORIG_AOPS.readpage  = lofs_readpage,
    ORIG_AOPS.bmap      = lofs_bmap,
    .write_begin        = lofs_write_begin,
    .write_end          = lofs_write_end,
};
