.\"
.\" COPYRIGHT (C) 2003 By Arthur Naseef
.\"
.\" This file is covered under the GNU General Public License Version 2.  For
.\"  more information, see the file COPYING.
.\"
.\"
.\" FILE: ovlfsmount.m
.\"
.\" DESCRIPTION:
.\"	Manual page for the ovlfsmount program which is designed to manage
.\"	ovlfs mounts more easily than fstab.
.\"
.\"
.\" REVISION HISTORY
.\"
.\" DATE	AUTHOR		DESCRIPTION
.\" ===========	===============	==============================================
.\" 2003-06-01	ARTHUR N.	Initial Release
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
.TH ovlfsmount 8 "CURRENT_DATE" "Arthur Naseef" "Linux System Admin"
.\"
.\" SECTION BREAK
.\"
.SH NAME
ovlfsmount -
mount program for the Overlay Filesystem.
.\"
.\" SECTION BREAK
.\"
.SH VERSION
This manual page covers the version of the
ovlfsmount program which corresponds to
version OVL_VERSION of the overlay filesystem.
.\"
.\" SECTION BREAK
.\"
.SH SYNOPSIS
ovlfsmount
.HI
[\-MmnPpsuvxz]
[\-i\ tab\-file]
[\-K\ selector\-name]
[\-o\ mount\-prog]
[\-S\ selector\-name]
[\-U\ mtab\-file]
[fsname ...]
.\"
.\" SECTION BREAK
.\"
.SH DESCRIPTION
The Overlay Filesystem, ovlfs,
takes many mount options in order to
mount even the simplest filesystem configuration,
and the filesystem has
many options available.
When using the standard mount program,
the command line,
or the similar fstab entry,
becomes rather long and difficult to read.
.\"
.P
With the \fBovlfsmount\fR program,
the mount options for the ovlfs
are more easily maintained.
Furthermore, the ovlfsmount program
has the following special features:
.P
.TP
\fBPrefiltering\fR
The contents
of the input file
are filtered
with a command,
which is specified
in the input file itself,
before being interpreted
by ovlfsmount.
So, for example,
it is possible
to use a preprocessor, such as m4,
to make the file more readable and
easier to maintain.
.TP
\fBOption Selection\fR
The filesystem to mount,
as well as
mount options,
are selected
based on the
results of a command.
.\"
.\" SUB-SECTION BREAK
.\"
.SS Options
The following options
are accepted by ovlfsmount
on the command line.
.\"
.TP
.BI \-i	tab\-file
Path to the ovlfs mount-tab file
describing the mounts.
.TP
.BI \-K	selector\-name
Determine the current
selector key
for the named selector.
See the section,
\fBSELECTORS\fR, below
for more information.
Note that no filesystem
is mounted.
.TP
.B \-M
Directly mount the filesystem
using the mount system call
instead of calling
the \fBmount(8)\fR program.
.TP
.B \-m
Use the \fBmount(8)\fR program
instead of mounting
the filesystem directly
with the mount system call.
.TP
.B \-n
Don't actually mount the filesystem,
but show the command
that would be executed instead.
Also displays the media's selector key
found when a selector is used.
.TP
.BI \-o	mount\-prog
Use the specified mount program
to mount the filesystem
instead of
the default mount program
or the mount system call.
.TP
.B \-P
Read the mount table file
and display the list
of mount entries
and selector entries.
Filesystems are not mounted.
.TP
.B \-p
Preprocess the mount table file
and display the result
on standard output.
Note that filesystems
are not mounted.
.TP
.BI \-S	selector\-name
Mount the filesystem
matched by the selector
with the name,
\fBselector\-name\fR.
See the section,
\fBSELECTORS\fR,
below for more information.
.TP
.B \-s
Read the mount table
from standard input
instead of a file.
.TP
.BI \-U	mtab\-file
Update the specified mtab file
with the filesystem information
for successful mounts.
Note that this option
has no effect
when a mount program
is used instead of
the mount system call.
.TP
.B \-u
Update the mtab file
with the filesystem information
for successful mounts.
The mtab file
is not updated
by default.
Note that this option
has no effect
when a mount program
is used instead of
the mount system call.
.TP
.B \-v
Verbose: display the
mount and selector
commands used,
in addition
to the media's selector key,
when a selector is used.
.TP
.B \-x
Display the
mount and selector
commands used.
.TP
.B \-z
Do not update the mtab file.
This is the default.
.\"
.TE
.\"
.\" SECTION BREAK
.\"
.SH USAGE
Once an \fBovlfstab(5)\fR
file is created,
ovlfs filesystem can
be mounted
by name or selector.
To mount a
filesystem by name,
execute ovlfsmount
as follows:
.P
.ti +1i
ovlfsmount \fBfs\-name\fR
.P
Replace \fBfs\-name\fR with
the name of the filesystem,
as it is defined
in the ovlfstab file.
To mount a filesystem
by a selector,
execute ovlfsmount
as follows:
.P
.ti +1i
ovlfsmount -S \fBsel\-name\fR
.P
Replace \fBsel\-name\fR with
the name of the selector.
.\"
.\" SECTION BREAK
.\"
.SH SELECTORS
Selectors allow
the name
of the filesystem
that will be mounted
to be chosen
based on the
output of a command.
Different mount points,
storage files,
and other options
may be set
in this manner.
An example use
of this feature
is the use
of different storage files
and storage roots
for each of many CDs,
based on the identification
for each CD.
.\"
.P
The key to this function
is the selector's
key generation command.
For example,
"cdinfo | md5sum",
could be a good choice
for identifying CDs.
Output from
the selector command
is compared directly
with the key values
stored in the ovlfstab file.
The matching entry
is used to
perform the mount.
When no entry matches,
the program exits
with an error message.
.\"
.P
Two types of selectors
are available: CD and special.
Both operate
in the same manner,
but CD selectors use
a pre-defined key generation command
while special selectors
require the command
to be specified.
Commands specified
in ovlfstab
are not processed
by the shell.
Each argument specified
is passed as-is
to execv(3).
If a shell command
is desired,
use "sh -c"
followed by the
shell command
as a single argument.
For example:
.\"
.P
.ti 1i
Command "/bin/sh" "-c" "echo entire-shell-command-here"
.\"
.SS Configuration
Selectors are defined
in the ovfstab file.
The following information
is required
for each selector:
.BL
.sp 1
.LI
Selector name
used to identify
the selector.
.sp 1
.LI
List of keys
with a filesystem
name for each key.
.sp 1
.LI
Selector command name and arguments
(special selectors only).
.LE
.\"
.P
In addition,
mount options
may be specified
for each key
which will override
the defaults
for the named filesystem.
.\" .\"
.\" .\" SECTION BREAK
.\" .\"
.\" .SH WARNINGS
.\" WARNING INFO HERE
.\"
.\" SECTION BREAK
.\"
.SH PATHS AND COMMANDS
.TP
/etc/ovlfstab
Default path
to the ovlfs
mount-tab file.
.TP
/etc/mtab
Default path
to the mtab file.
.TP
/sbin/mount
Default path
for the mount
program when using
an external mount program.
.TP
"/bin/sh -c 'set -- $(cdinfo | ( md5sum || cksum )); echo $1'"
Default CD selector
key generation command.
Note the lack
of paths for the
cdinfo, m5dsum, and cksum
commands.
.\"
.\" SECTION BREAK
.\"
.SH NOTES
.BL
.LI
There is no unmount
version of this program.
Just use the system
\fBumount(8)\fR program
instead.
Of course,
contributions are welcome.
.LE
.\"
.\" SECTION BREAK
.\"
.SH SEE ALSO
execv(3), mount(8), ovl(8), ovlfstab(5)
.\"
.\" SECTION BREAK
.\"
.SH COPYRIGHT
This manual page and associated source code is licensed under the
GNU General Public License Version 2.
Of course, all source derived from the linux kernel source is copyrighted
by the original author of that source.
.sp 1
COPYRIGHT (C) 2003 By Arthur Naseef
.\"
.\" SECTION BREAK
.\"
.SH FILE VERSION
 @(#)ovlmount.m	1.2	[03/07/27 22:20:33]
