//#include "imgact_aout.h"
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999, 2001  Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef FSYS_ROMFS

#include "shared.h"
#include "filesys.h"

/* include/asm-i386/types.h */
typedef __signed__ char __s8;
typedef unsigned char __u8;
typedef __signed__ short __s16;
typedef unsigned short __u16;
typedef __signed__ int __s32;
typedef unsigned int __u32;

#include "romfs_fs.h"

static inline __const__ __u32
le32 (__u32 x)
{
#if 0
        /* 386 doesn't have bswap.  */
        __asm__("bswap %0" : "=r" (x) : "0" (x));
#else
        /* This is slower but this works on all x86 architectures.  */
        __asm__("xchgb %b0, %h0" \
                "\n\troll $16, %0" \
                "\n\txchgb %b0, %h0" \
                : "=q" (x) : "0" (x));
#endif
        return x;
}


/* linux/limits.h */
#define NAME_MAX         255	/* # chars in a file name */

/* linux/posix_type.h */
typedef long linux_off_t;

/* "local" defs, not stolen from a header file */
#define MAX_LINK_COUNT 5

/* made up, these are pointers into FSYS_BUF */
/* read once, always stays there: */
#define SUPERBLOCK \
     ((struct romfs_super_block *)(FSYS_BUF))
#define INODE \
     ((struct romfs_inode *)((int)SUPERBLOCK + sizeof(struct romfs_super_block)))

int rootdir_offset;
unsigned char *pRomfsBase = (unsigned char*)ROMFS_LOAD_ADDR;


/////////////////////////////////////////////////////////////////////////////////
//                                                               romfs_devread()
// --------------------------------------------------------------------------
// Romfs devread hooks function.
//  1. if romfs_drive == ROMFS_DRIVE, we are curentlly use a memory disk loacated
//        ROMFS_LOAD_ADDR (see shared.h);
//  2. if romfs_drive != ROMFS_DRIVE, we use devread() to allow original function
//
/////////////////////////////////////////////////////////////////////////////////
static int
romfs_devread (int sector, int byte_offset, int byte_len, char *buf)
{
    unsigned int offset;

    if(romfs_drive != ROMFS_DRIVE)      // original matter
        return devread (sector, byte_offset, byte_len, buf);

    // Otherwize, we've hook this romfs in memory.
    offset = sector * SECTOR_SIZE + byte_offset;

    // !!! no range check    
    grub_memmove(buf, pRomfsBase + offset, byte_len);
 
    return true;
}

static int find_string_len(unsigned char *buf, int offset) 
{
  int i=0;
  /* all strings are nul-padded to even 16 bytes */

  do {
    romfs_devread ( (offset+i*16) / SECTOR_SIZE,
	      (offset+i*16) % SECTOR_SIZE,
	      16, buf+(i*16));
    //    printf("reading name, i=%d, buf=%s\n", i, buf);
    i++;
  } while (buf[i*16-1] != '\0');
  return i*16;
}

/* return disk offset of file data. */
static int load_inode(int offset) 
{
  // grub_printf("load_inode at %d\n", offset);

  disk_read_func = disk_read_hook;
  
  romfs_devread (offset / SECTOR_SIZE,
	   offset % SECTOR_SIZE,
	   sizeof(*INODE)+16, (unsigned char*)INODE);

  INODE->next=le32(INODE->next);
  INODE->spec=le32(INODE->spec);
  INODE->size=le32(INODE->size);
  INODE->checksum=le32(INODE->checksum);

  disk_read_func = NULL;
  {
    int namelen;
    namelen = find_string_len(((unsigned char*)INODE)+sizeof(*INODE),
			      offset+16);
        //grub_printf("name=%s\n", INODE->name);
    return offset+16+namelen;
  }
}

/* check filesystem types and read superblock into memory buffer */
int romfs_mount (void)
{
  int retval = 1;
  int vollen;

  errnum=0;
  fsmax = 0;
      
  if ((((current_drive & 0x80) || (current_slice != 0))
       && (current_slice != PC_SLICE_TYPE_EXT2FS)
       && (! IS_PC_SLICE_TYPE_BSD_WITH_FS (current_slice, FS_EXT2FS))
       && (! IS_PC_SLICE_TYPE_BSD_WITH_FS (current_slice, FS_OTHER)))) 
  {
        retval = 0;
  } 
  else if (!romfs_devread (0, 0, sizeof (struct romfs_super_block),
		       (char *) SUPERBLOCK)) 
  {
        retval = 0;
  } 
  else if (SUPERBLOCK->word0 != 0x6d6f722d       // ROMSB_WORD0 
      || SUPERBLOCK->word1 != 0x2d736631 )       // ROMSB_WORD1
  {                  
        retval = 0;
  }
  
  if(romfs_drive != current_drive )      // != ROMFS_DRIVE
      return 0;
  else          // memory hooked romfs
  {
    // Check for romfs tag
	if (grub_memcmp(pRomfsBase, ROMFS_MAGIC, 8)) 
    {
        retval = 1;
    }
    
    // move data for further check.
    grub_memmove((unsigned char *)SUPERBLOCK, pRomfsBase, sizeof (struct romfs_super_block));

  }
  //if (TODO checksum?)
  {
    vollen = find_string_len((unsigned char *)&SUPERBLOCK->name, 16);
    rootdir_offset = 16+vollen;

//    printf("rootdir_offset=%d\n", rootdir_offset);
  }
  
  return retval;
}

static int disk_offset;
static int is_eof;

int
romfs_read (char *buf, int len)
{
  int ret = 0;

  if (is_eof)
  {
    grub_printf("got eof\n");
    return 0;
  }

  disk_read_func = disk_read_hook;

  romfs_devread ((disk_offset+filepos) / SECTOR_SIZE,
	   (disk_offset+filepos) % SECTOR_SIZE,
	   len, buf);
  
  disk_read_func = NULL;

  // grub_printf(" r_filepos:%d len: %d ", filepos, len);

  filepos += len;
  ret = len;
  
  if (errnum)
    ret = 0;
  
  if (filepos > filemax)
    is_eof=1;
  return ret;
}

int
romfs_dir (char *dirname)
{
    char            *rest;
    char            ch;			        /* temp char holder */
    int             str_chk;			/* used to hold the results of a string compare */
    int             inode_ret=0;
    unsigned int    pathlen;            // for autocomplication
    int             type;

    disk_offset = rootdir_offset;

    // loop invariants:
    // ----------------------------------------------------------------------
    // current_ino = inode to lookup
    // dirname = pointer to filename component we are cur looking up within
    // the directory known pointed to by current_ino (if any)
    //
  
    while (1) 
    {
        /* look up an inode */
        inode_ret=load_inode(disk_offset);

        type = INODE->next&ROMFH_TYPE;

        if (type==ROMFH_HRD) 
        {
            disk_offset = INODE->spec;
            continue;
        } 
        else if (type==ROMFH_SYM) 
        {
            errnum = ERR_SYMLINK_LOOP;
            return 0;
        }
        else // dir or file
        {

            //printy("%s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__);
            /* if end of filename, INODE points to the file's inode */
            if (!*dirname || isspace (*dirname)) 
            {
                //printy("%s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__);
                if (type!=ROMFH_REG) 
                {
                  //printy("type=%d\n", type);
                  errnum = ERR_BAD_FILETYPE;
                  return 0;
                }
                disk_offset += rootdir_offset;
                is_eof=0;

                filemax = INODE->size;

                // grub_printf(" fmax=%d ", filemax);

                return 1;
            } 
            else 
            {
                //printy("%s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__);
                /* else we have to traverse a directory */

                /* skip over slashes */
                while (*dirname == '/')
                    dirname++;

                /* pathlen = strcspn(dirname, "/\n\t "); */
                for (pathlen = 0 ;
                    dirname[pathlen]
                    && !isspace(dirname[pathlen]) && dirname[pathlen] != '/' ;
                    pathlen++)
                        ;

                /* if this isn't a directory of sufficient size to hold our file, abort */
                if (type!=ROMFH_DIR) 
                {
                   errnum = ERR_BAD_FILETYPE;
                   return 0;
                }

                /* skip to next slash or end of filename (space) */
                for (rest = dirname;
                    (ch = *rest) && !isspace (ch) && ch != '/';
                    rest++)
                    ;

                /* look through this directory and find the next filename
                component */
                /* invariant: rest points to slash after the next filename
                component */
                *rest = 0;

                //printy("%s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__);

                int dir_offset=INODE->spec;	/* disk offset for this dir search */

                do 
                {
                    if (!dir_offset) 
                    {
                        /* not found */
                        if (print_possibilities >= 0) 
                        {
                            errnum = ERR_FILE_NOT_FOUND;
                            *rest = ch;
                        }
                        return (print_possibilities < 0);
                    }

                    //printy("%s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__);
                    load_inode(dir_offset);
                    str_chk = grub_strcmp(dirname, INODE->name);
                    //printf("strcmp(%s, %s)=%d\n", dirname, INODE->name, str_chk);    
                    // filemax = MAXINT;
#ifndef STAGE1_5
                    if (grub_strlen(INODE->name) >= pathlen && print_possibilities
                        && !memcmp(INODE->name, dirname, pathlen) && ch != '/')
                    {
                        if (print_possibilities > 0)
                            print_possibilities = -print_possibilities;

                        print_a_completion (INODE->name);

                        str_chk = -1;
                    }
#endif
                    if (str_chk)
                    {
                        dir_offset=INODE->next & ROMFH_SIZE_MASK;
                    }

	            } while (str_chk || (print_possibilities && ch != '/'));
  
                disk_offset = dir_offset;
  
                *(dirname = rest) = ch;
            }
        }
    }
    /* never get here */
    return 1;
}


#endif /* FSYS_ROMFS */
