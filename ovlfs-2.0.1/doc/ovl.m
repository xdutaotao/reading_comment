.\" COPYRIGHT (C) 1998-2003 By Arthur Naseef
.\"
.\" This file is covered under the GNU General Public License Version 2.  For
.\"  more information, see the file COPYING.
.\"
.\"
.\" FILE: ovl.m
.\"
.\" FILE VERSION:
.\"	@(#)ovl.m	1.4	[03/07/27 22:20:33]
.\"
.\" DESCRIPTION: Manual page for the overlay filesystem.
.\"
.\"
.\" REVISION HISTORY
.\"
.\" DATE	AUTHOR		DESCRIPTION
.\" ===========	===============	==============================================
.\" 03/08/1998	ARTHUR N.	Initial Release
.\" 03/10/1998	ARTHUR N.	Changed tmac.an to tmac.asn to remove the
.\"				 conflict with the man page tmac.an file.
.\" 03/10/1998	ARTHUR N.	Added the shortcuts for options.
.\" 06/01/2003	ARTHUR N.	Updated for the new release, and made other,
.\"				 minor enhancements.
.\"
.\"
.\" M4 definitions:
include(tmac.asn)dnl
define(m4_concat, $1$2$3$4$5$6$7$8$9)dnl
define(CURRENT_DATE, translit(esyscmd(date), `
', `'))dnl
define(OVL_VERSION, translit(esyscmd(awk '{print $1; exit}' Version), `
', `'))dnl
.\"
.\"
.\"
.ad l
.TH ovl 8 "CURRENT_DATE" "Arthur Naseef" "Linux System Admin"
.\"
.\" SECTION BREAK
.\"
.SH NAME
ovl.o -
overlay pseudo filesystem.
.\"
.\" SECTION BREAK
.\"
.SH VERSION
This manual page covers version OVL_VERSION of the overlay filesystem.
.\"
.\" SECTION BREAK
.\"
.SH SYNOPSIS
modprobe ovl.o
.br
mount -t ovl \fIovl0\fR /mnt -o root=\fIrootdir1\fR,storage=\fIrootdir2\fR
.\"
.\" SECTION BREAK
.\"
.SH DESCRIPTION
The overlay filesystem is a pseudo filesystem that allows any filesystem,
including one that is read-only, to be modified by saving the changes in
another filesystem.
.P
There is much documentation on the issues of using the overlay filesystem
that is included with the source distribution.
This manual page is intended only to clarify how to use the filesystem.
.\"
.\" SECTION BREAK
.\"
.SH USAGE
In order to use the overlay filesystem,
determine the root directories of the base and storage filesystems,
and any special mount options to use.
Then, the mount command may be used to mount the filesystem.
As shown in the SYNOPSIS above,
use the mount command to mount the filesystem.
The device name is not important, and can be chosen at your leisure,
because the filesystem does not use a device.
.P
For example, if the root of the base filesystem is \fB/cdrom\fR
the root of the storage filesystem is \fB/cdrom_ovl\fR,
the storage file is \fB/.cdrom_stg\fR,
and the mount point is \fB/mnt\fR,
then the following mount command can be used:
.P
.in +4n
.hy 0
.HI /sbin/mount
\-t ovl cdrom_ovl \fB/mnt\fR \-o root=\fB/cdrom\fR,storage=\fB/cdrom_ovl\fR,\
stg_file=\fB/.cdrom_stg\fR
.HE
.hy 1
.in
.\"
.\" SUB-SECTION BREAK
.\"
.SS MOUNT OPTIONS
Many mount options are available
with the ovlfs.
Here are the general-purpose
mount options:
.P
.in +1i
base_root,
storage_root,
stg_method,
nostorage,
maxmem,
xmnt,
noxmnt,
updmntonly,
and noupdmntonly.
.in
.P
Options which affect
the storage of inode mappings,
if compiled
into the driver,
are listed below:
.P
.in +1i
storemaps,
nostoremaps,
basemap,
nobasemap,
stgmap,
and nostgmap
.in
.P
Last, the following options are available for the magic directories,
if compiled into the driver:
.P
.in +1i
magic,
nomagic,
hidemagic,
showmagic,
basemagic,
nobasemagic,
ovlmagic,
noovlmagic,
smagic,
and bmagic
.in
.P
Note that there are shortcuts for these options;
see the subsection, \fBShortcuts\fR, for more information.
Each option is described
in more detail here:
.TP
root=\fIdir\fR
Set the root of the base filesystem to the named directory;
this is the only way to specify this information.
The string \fBbase_root=\fR may be used in place of root=.
.TP
storage=\fIdir\fR
Set the root of the storage filesystem to the named directory;
this is the only way to specify this information.
The string \fBstorage_root=\fR may be used in place of storage=.
.TP
stg_file=\fIpath\fR
Set the path to the storage file to the named path.
All inode information and inode mapping information is
saved between mounts in the storage file.
.TP
nostorage
Turn off the use of storage.
This option does not work yet.
.TP
maxmem
Turn on checking of the amount of available memory before
allocating more memory for inode storage for the overlay filesystem.
From testing, it seems that this option is better left off.
.TP
xmnt
Turn on the crossing of mount points in the base
and storage filesystems.
See the file, Options, distributed with
the source tree for more information.
.TP
noxmnt
Turn off the crossing of mount points in the base
and storage filesystems.
See the file, Options, distributed with
the source tree for more information.
.TP
storemaps
Store all inode mapping information in the storage file.
.TP
nostoremaps
Do not store any inode mapping information in the storage file.
.TP
basemap
Store the inode mapping information for the base filesystem
in the storage file.
.TP
nobasemap
do not store the inode mapping information for the base filesystem
in the storage file.
.TP
stgmap
Store the inode mapping information for the storage filesystem
in the storage file.
.TP
nostgmap
Do not store the inode mapping information for the storage filesystem
in the storage file.
.TP
hidemagic
Do not display the magic directories in directory listings.
In other words, with this option,
the readdir() function will not include the magic directories.
.TP
showmagic
Display the magic directories in directory listings.
This is the opposite of the hidemagic option.
.TP
magic
Turn on the default magic directory options.
At the time of this writing, the default options include
basemagic, ovlmagic, and hidemagic.
.TP
basemagic
Turn on the magic directory for the base filesystem.
.TP
nobasemagic
Turn off the magic directory for the base filesystem.
.TP
ovlmagic
Turn on the magic directory for the storage filesystem.
.TP
noovlmagic
Turn off the magic directory for the storage filesystem.
.TP
smagic=\fIdir\fR
Set the name of the magic directory for the storage filesystem
to the specified name.
Do not include any slashes in this name.
The option, \fBmagic=\fIdir\fR, is a synonym for this option.
.TP
bmagic=\fIdir\fR
Set the name of the magic directory for the base filesystem
to the specified name.
Do not include any slashes in this name.
The option, \fBmagicr=\fIdir\fR, is a synonym for this option.
.\"
.\" SUB-SECTION BREAK
.\"
.SS Shortcuts
All of the supported mount options have shortcuts to
help reduce the length of the mount options on the command line.
.P
.de li
.LI
\\$1
.Tf 24n
\\$2
..
.BL
.li nostorage nost
.li maxmem mm
.li xmnt xm
.li noxmnt noxm
.li updmntonly um
.li noupdmntonly noum
.li base_root=<dir> br=<dir>
.li storage_root=<dir> sr=<dir>
.li stg_file=<path> sf=<path>
.li stg_method=<path> method=<path>
.li storemaps ma
.li nostoremaps noma
.li basemap bm
.li nobasemap nobm
.li stgmap sm
.li nostgmap nosm
.li hidemagic hm
.li magic mg
.li nomagic nomg
.li basemagic mb
.li nobasemagic nomb
.li ovlmagic mo
.li noovlmagic nomo
.li showmagic ms
.li smagic=<name> sn=<name>
.li bmagic=<name> bn=<name>
.LE
.\"
.\" SECTION BREAK
.\"
.SH WARNINGS
The overlay filesystem in many ways emulates the VFS.
There are many possible ways that the use of the overlay
filesystem may cause problems.
A few of the most important issues are mentioned here.
Please see the rest of the documentation included with the source
for more information.
.P
Note that in spite of all of these warnings, and all others
documented for the overlay filesystem,
the use of the overlay filesystem should not cause any corruption
to any of the filesystems involved.
However, for your own safety, it is a good idea to backup ALL
filesystems on the host before using the overlay filesystem.
.P
.BL
.LI
Do not attempt to use a storage root that is a subdirectory of the
base root directory, or vice versa.
.P
.LI
All inodes that are not regular files or directories,
such as device files, pipes, and sockets,
may be directly accessed for reading and writing
through the overlay filesystem.
.P
.LI
The access and change times of the inodes in the base filesystem
may be affected by the use of the overlay filesystem,
although the modification times should not be affected -
except for files that are not directories or regular files.
.P
.LI
Any change to a file in the base filesystem causes the entire
file to be copied into the storage filesystem, even if only
one byte of the file is modified, and regardless of the size
of the file.
.P
.LI
Since names are only pointers to inodes, files and directories that
need to be created in the storage filesystem to store changes from
the base filesystem, may take a name other than the one being
accessed in the base filesystem.
This should only happen with files that have more than one hard link.
.P
.LI
Removing files from the overlay filesystem does not remove their inodes.
The only way to cleanup old inodes in the overlay filesystem is to remove
the storage file, or use a new storage file, for the filesystem.
.LE
.\"
.\" SECTION BREAK
.\"
.SH SEE ALSO
mount(8), ovlfsmount(8), ovlfstab(5)
.\"
.\" SECTION BREAK
.\"
.SH COPYRIGHT
This manual page and associated source code is licensed under the
GNU General Public License Version 2.
Of course, all source derived from the linux kernel source is copyrighted
by the original author of that source.
.sp 1
COPYRIGHT (C) 1998-2003 By Arthur Naseef
