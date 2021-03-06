/*
 * Copyright (c) 1997-2007 Erez Zadok <ezk@cs.stonybrook.edu>
 * Copyright (c) 2001-2007 Stony Brook University
 *
 * For specific licensing information, see the COPYING file distributed with
 * this package, or get one from
 * ftp://ftp.filesystems.org/pub/fistgen/COPYING.
 *
 * This Copyright notice must be kept intact and distributed with all
 * fistgen sources INCLUDING sources generated by fistgen.
 */
/*
 * File: fistgen/templates/Linux-2.6/sca_aux.h
 */
#ifndef FISTFS_H
#define FISTFS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define DEFAULT_CHUNK_SZ 4096
#define REAL_PAGE_SZ 4096	/* The underlying machine page size */

extern int chunksize;		/* Size of an encoded chunk */
extern int do_fast_tails;	/* Use fast tail algorithm */

extern int encode_counter;	/* Number of times enocde_page called */
extern int decode_counter;	/* Number of times decode_page called */
extern int write_counter;	/* Number of writes to the underlying media */

extern int sca_encode_page(unsigned char *, int, unsigned char **, int *);
/* encode_page takes an array of un-encoded data in 'in', encodes it into
   (*out). *out is allocated inside encode_page, and should be free'd when
   you finish with it. It returns a negative value for an error, or the
   length of (*out) for success
*/

extern int sca_decode_page(unsigned char *, int, unsigned char **, int *);
/* decode_page takes an encoded byte range (as produced by encode page),
   decodes it, and fills (*out) with it. (*out) is allocated inside
   decode_page, and should be free'd when you finish with it. It returns -1
   for an error, or the length of (*out) for success
*/

struct fistfs_header {
        int num_pages;		/* Number of un-encoded pages */
        int real_size;
        off_t *offsets;		/* Ending offset of each page */
        unsigned flags;
};

extern int read_idx(char *filename, struct fistfs_header *hdr);
/* Takes a filename, reads fistfs style index info out of it and into hdr.
   returns the number of entries read on success, or a negative number on
   failure.

   Note that this mallocs an array in hdr.offsets, which must be freed.
*/

extern int write_idx(char *filename, struct fistfs_header *hdr);
/* Takes a filename, writes a fistfs style index info into it from hdr.
   Returns the number of entries written on success, or a negative number on
   failure.
*/

extern int put_page(int gzfd, struct fistfs_header *hdr, int pageno,
		    unsigned char *data, int datalen);
/* put_page writes the encoded version of the data in 'data' (of length
   'datalen') out gzfd. If pageno < 0, it appends the data to the end of the
   file. Otherwise, it replaces the data formerly in page pageno (if any)
   with the new encoded data, and afjusts the header and underlying file
   appropriately.
*/

extern int get_page(int gzfd, struct fistfs_header *hdr, int pageno,
		    unsigned char **out, int *outlen);
/* get_page returns an unencoded page of data in out, whose length is stored
   in outlen. Pass it in a file descriptor to a gzf file on gzfd, the header
   of the gzf file in hdr, and the desired page number in pageno. It returns
   the length of the page received on success, and a negative errno on an
   error.

   Note that this malloc's out for the user, which must be freed.
*/

#endif /* FISTFS_H */

/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
