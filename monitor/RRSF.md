RRSF13 System of Files
======================

Overview
--------
The ROT13 of RRSF is EEFS, which stands for EEPROM File System, but the EEFS
acronym is already used for an unrelated EEPROM filesystem made by NASA.

RRSF also means Recycled Rubber Steel Fiber, RACF Remote Sharing Facility, Reeva
Rebecca Steenkamp Foundation, Recruitment And Retention Shared Facility, or
Reseau Rail Sans Frontieres, all of which have absolutely nothing to do with a
file system.

This filesystem is intented for use on serial EEPROM devices. Some efforts are
made to ensure some wear leveling, but it is still easy to abuse the device.
More efforts have been made to ensure that operations are still safe if the
power is lost with no previous notice.

Intended Physical devices
-------------------------
Largest I2C/SPI EEPROM is (2 Mbit, 256k x 8-bit)
Atmel/Microchip AT25M02 / AT24CM02 (1M Write cycles, 100 years retention)
ST M95M02 / M24M02 (4M Write cycles, 200 years retention)
That means 262144 bytes in 1024 256-byte pages
byte address need 18 bits for encoding.
page addresses need 10 bits for encoding.
These I2C EEPROMS have an external address line, so two of them can be used on
the same bus for a total of 512kbytes, in a kind of RAID0 fashion.

This filesystem can also be used in good old RAM/EEPROM/EPROM, speficically with
a banked memory system that allows a 256-byte view on the page, with the rest of
address bits kept in separate latches. With the current FS encoding, it is
possible to support 65536 pages of 256 bytes, for a total of 16777216 bytes.
That would be 16 Mbytes or 128 Mbits.

In the future we will extend the FS with 32-bit page pointers which will allow
for much more room.

Min Write unit in these serial chips is 4 byte (one word) (for ECC protection).
Max Write unit is 256 bytes (one page).

Reads can be of any arbitrary size, but it is a good idea to stick to one-page
reads only.

FS parameters
-------------
The only parameter is FILENAME_MAXSIZE, which is 16 bytes. This also apply to
directory names.

A FS journal could be added in the future:
1) Write transaction to journal, 
2) Write a validation flag, 
3) Do the transaction on the actual FS,
4) invalidate journal entry.

This would have a penalty on the wear leveling if the number of journal blocks
is too small.

Device organization
-------------------
Storage is organized in 256-byte pages
Each page has a 4-byte header with a type that describes its function
```
Offset Description
------------------
00h    Page type
01h    For pages with type=1, Number of valid bytes in the page. Else, FFh.
02h    Index of next page (FFFFh if none)
```

```
Type   Description
------------------
00h    Superblock
01h    File data
02h    Directory
03h    Symbolic Link
04h    Garbage list
FFh    free
```

File metadata
-------------
Files and dir have simplified metadata. They have an owner ID on one byte, and
separate read/write/execute bits for the owner and other users. There is no
notion of group.

Page allocation
---------------
When mounted, storage is scanned and the last used page is noted.
Page allocation is then strictly sequential and starts after the last used page.
When the last page of the device is reached, the sequence restarts at the first
page.

Sequential allocation is the only mechanism that attempt to minimize eeprom
wear. Defragmentation is not used since all pages can be reached with the same
delay.

A page is considered used when the first byte of the page is not FFh. This means
that releasing a page just requires writing FFh in its first word.

When it is not possible to allocate a page in a device, garbage collection (see
below) is attempted before declaring failure.

File data pages
---------------
File data is stored in pages with type=01h.

Since the file size is not stored in directory entries, getting the size of a
file requires browsing all the pages of the file.

The first bytes of the first page of a file are not user data but file metadata.
```
Offset Size             Description
-----------------------------------
00h    01h              Access rights (0-0-OX-OW-OR-X-W-R)
01h    01h              Owner ID
02h    02h              Page index of the parent directory (for file erase
                        recovery)
04h    FILENAME_MAXSIZE File name
```

As a consequence, the first page holds 232 bytes of user data, and the next
pages hold 252.

Directory pages
---------------
Directory pages have type=02h and the same structure of file pages, except that
file data is replaced by page indexes to child files and directories. Parent
directory is stored the same way as files.

Symbolic links
--------------
Links are similar to files but have type=03h and the file contents is the path
to the target file. Hard link are not supported to avoid reference counting of
data pages.

Garbage pages
-------------
Garbage pages are a doubly linked list of pages that are no longer in use by the
filesystem, and that must be freed at the next occasion. They are indicated with
a type=04h. The other bytes in the first word are not changed, since they will
indicate a valid next page that must also be erased.
The use of this list is to make sure that unused pages are really freed, since
it requires less operations to link deleted pages to garbage than to free them
effectively.

The superblock contains the index of the next garbage page, of FFFFh if there is
no garbage page.

The index of the last page added to the garbage chain is kept in RAM. It is used
when directories are deleted and files are deleted or truncated, to easily add
new pages at the end of the garbage list.

When some pages of a file/dir are not used, the first page of the chain to
delete is appended to the list of garbage pages:
1) set the page type to 04h (this is atomic)
2) set the NEXT pointer of the last garbaged page to the index of the new page
   chain to be deleted
3) Remember the new index of the last page of the resulting chain.

If nothing more is done at this point, a file/dir can point to garbaged pages.
At the time of mounting, when the directories are traversed, if any dir/file has
a page that is garbage, the file/dir is truncated by setting the next pointer of
the last non-garbage page to FFFFh.

Superblock
----------
The superblock contains meta data about the filesystem.

Offset | Description
00h    | Type 00h
01h    | FS options: 01h: read only, others bits RFU
02-03h | RFU
04-05h | Pointer to root directory
06-07h | RFU
08-0Fh | Volume label
10-13h | Format flag, 00h: being formatted, FFh: format complete
14-15h | garbage head (FFFFh if none)
16-FFh | RFU

Formatting
----------
1) Set flag in superblock as "being formatted"
2) Mark all used blocks in the device as free (just write the header)
3) Create a root directory
4) Write a superblock with a reference to the root dir
5) Set flag in superblock as "not being formatted"

Mounting
--------
1) If superblock indicates "being formatted", do a format then reset this flag
2) Traverse the directories depth first (can be done without a stack, just with
   current/parent dir pointers):
   1) if any child entry in a directory points to a page that is in
        the garbage list (type=04h), set the directory entry to FFFFh.
   2) When all entries in a directory have been traversed, cleanup this
        directory (see below).
   3) When browsing the next page of a directory or file, if that page is in
        the garbage list (type=04h), then set the "next" pointer of the current
        page to FFFFh (truncation).
3) Cleanup the garbage chain: Browse it forward to define the previous links
(stored in the first bytes after the header of each page), then browse it
backwards to free the garbage pages, starting from the last one. This requires
writing a single FFFFFFFFh word in the page header. Note: We release pages from
the last one to avoid breaking up the garbage list. If power is cut at any
moment in this process, we can just restart the cleanup at next mount.

Optional: Full garbage collection by determination of reachable pages.
Since the largest serial EEPROM device has 1024 pages, allocating one bit for
each page requires just 128 bytes per device. The maximum volume size of 65536
pages would require 8k, which is more critical but still doable. A directory
traversal can be used to determine the extensive list of reachable pages,
and set free all the unreachable pages that dont have a type set to FFh.

Directory creation
------------------
1) Allocate a named page with type 02h and no "next" pointer, having the current
   directory as parent
2) Populate dir rights, owner and name
3) Create a new child entry in the current dir
TODO analyze partial execution and recovery options

File creation
-------------
1) Allocate a page with type 01h and no "next" pointer, having the current
   directory as parent
2) Populate file rights,owner and name
3) Create a new child entry in the current dir
TODO analyze partial execution and recovery options

Child creation in current dir
-----------------------------
This is used to add a file or dir to the current directory
1) Find a FFFFh entry in the current dir (that may be between two existing
   entries, or at the end of the list of entries). The directory may span on
   multiple pages, child entries do not have to be stored by name.
2) If no entry is found,
   1) Create a new directory page. If page not erased, do a full page erase.
   2) Link this page as next page of the current directory
   3) This page will obviously have entries, so retry the first step
3) Set the entry to the index of the first page of the child
TODO analyze partial execution and recovery options

Directory deletion
------------------
Only empty directories can be deleted. This means only one page has to be freed.
The current directory has to be the parent directory of the entry being deleted.
1) If directory to delete is not empty, fail.
2) Add the directory page to the garbage list.
If power is cut here the next mount operation will find the deleted dir and
clean it up.
3) Set the entry to this directory to FFFFh
If cleanup is not done bc of power cut, it will be reattempted at next mount
4) Cleanup the directory
TODO finalize analyze partial execution and recovery options

File deletion
-------------
1) Add the first page of the file to the garbage list
If power is cut here the next mount operation will find the deleted dir and
clean it up>
2) Set the directory entry to this file to FFFFh
If cleanup is not done bc of power cut, it will be reattempted at next mount>
3) Cleanup the directory
TODO finalize analyze partial execution and recovery options

Directory cleanup
-----------------
This process has to happen after a directory or file is deleted.
1) While there are holes in the list (FFFFh entries), swap this entry and the
   last entry of the directory. This ensures that the list has no holes.
2) If this process results in removing the last entry of the last directory
   page, then add the last page of the directory to the garbage list and fix the
   next pointer of the previous page.
TODO analyze partial execution and recovery options
   
File data writing
-----------------
The current size of a file is determined at file opening by counting full pages
and adding the number of valid bytes indicated in the header of the last page.
Obviously, if bytes are written one by one, the page header (and to a lesser
extent the file page bytes themselves) will wear much faster than other bytes.
A RAM buffering mechanism can be used to reduce the number of writes in the
header, at the expense of data loss if power is cut, but this is not mandatory
per filesystem design.
Incoming data bytes are just written to the proper offset in the existing file
pages.

If file pointer is at the end of the file, this is an append, not a write.

File appending (growing)
------------------------
When the write pointer is at EOF, two situations happen:

Write beyond the file size within the same page:
1) Write the byte at correct offset
2) Update the page use size in the page header
If the data does not fit in the last page of the file, a new file page must be
allocated:
1) Allocate a page with type 01h and no "next" pointer
2) Write the data and length in header if the page will not be used in full
3) Update the header of the previous page to point to this new next page
4) Repeat this until all data has been appended.
TODO analyze partial execution and recovery options

File truncation (shrinking)
---------------------------
This is equivalent to a partial file deletion.
1) Find the page that contains the truncation point
2) Update the valid number of bytes in the header
3) Add the page that follows the truncation point to the garbage list
4) Fix the next pointer of the current page
TODO analyze partial execution and recovery options

EOF

