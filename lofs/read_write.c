/**
 * lofs: Linux loopback filesystem
 *
 * Copyright (C) 2007 International Business Machines Corp.
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

#include <linux/fs.h>
#include <linux/pagemap.h>
#include "lofs_kernel.h"

/**
 * lofs_write_lower
 * @lofs_inode: The lofs inode
 * @data: Data to write
 * @offset: Byte offset in the lower file to which to write the data
 * @size: Number of bytes from @data to write at @offset in the lower
 *        file
 *
 * Write data to the lower file.
 *
 * Returns bytes written on success; less than zero on error
 */
int lofs_write_lower(struct inode *lofs_inode, char *data,
                         loff_t offset, size_t size)
{
        struct lofs_inode_info *inode_info;
        mm_segment_t fs_save;
        ssize_t rc;

        inode_info = lofs_inode_to_private(lofs_inode);
        mutex_lock(&inode_info->lower_file_mutex);
        BUG_ON(!inode_info->lower_file);
        inode_info->lower_file->f_pos = offset;
        fs_save = get_fs();
        set_fs(get_ds());
        rc = vfs_write(inode_info->lower_file, data, size,
                       &inode_info->lower_file->f_pos);
        set_fs(fs_save);
        mutex_unlock(&inode_info->lower_file_mutex);
        mark_inode_dirty_sync(lofs_inode);
        return rc;
}

/**
 * lofs_write_lower_page_segment
 * @lofs_inode: The lofs inode
 * @page_for_lower: The page containing the data to be written to the
 *                  lower file
 * @offset_in_page: The offset in the @page_for_lower from which to
 *                  start writing the data
 * @size: The amount of data from @page_for_lower to write to the
 *        lower file
 *
 * Determines the byte offset in the file for the given page and
 * offset within the page, maps the page, and makes the call to write
 * the contents of @page_for_lower to the lower inode.
 *
 * Returns zero on success; non-zero otherwise
 */
int lofs_write_lower_page_segment(struct inode *lofs_inode,
                                      struct page *page_for_lower,
                                      size_t offset_in_page, size_t size)
{
        char *virt;
        loff_t offset;
        int rc;

        offset = ((((loff_t)page_for_lower->index) << PAGE_CACHE_SHIFT)
                  + offset_in_page);
        virt = kmap(page_for_lower);
        rc = lofs_write_lower(lofs_inode, virt, offset, size);
        if (rc > 0)
                rc = 0;
        kunmap(page_for_lower);
        return rc;
}

/**
 * lofs_read_lower_page
 *
 * Determines the byte offset in the file for the given page and
 * offset within the page, maps the page, and makes the call to read
 * the contents of @page_for_lofs from the lower inode.
 *
 * Returns zero on success; non-zero otherwise
 */
int lofs_read_lower_page(
    struct page *page_for_lofs,         /* Page into which data for lofs will
                                         * be read. */
    pgoff_t page_index,                 /* Index of the page within the file,
                                         * used to determine the byte offset
                                         * of the page. */
    struct inode *lofs_inode)           /* The lofs inode for the file. */
{
    char *data;
    loff_t offset;
    struct lofs_inode_info *inode_info = lofs_inode_to_private(lofs_inode);
    mm_segment_t fs_save;
    ssize_t rc;

    offset = (((loff_t)page_index) << PAGE_CACHE_SHIFT);
    data = kmap(page_for_lofs);

    mutex_lock(&inode_info->lower_file_mutex);
    BUG_ON(!inode_info->lower_file);

    fs_save = get_fs();
    set_fs(get_ds());
    rc = vfs_read(inode_info->lower_file, data, PAGE_CACHE_SIZE, &offset);
    set_fs(fs_save);

    mutex_unlock(&inode_info->lower_file_mutex);

    if (rc >= 0) {
        /* Zero out the remainder of the page, just to be safe. */

        memset(data + rc, 0, PAGE_CACHE_SIZE - rc);
        rc = 0;
    }
    kunmap(page_for_lofs);
    flush_dcache_page(page_for_lofs);
    return rc;
}
