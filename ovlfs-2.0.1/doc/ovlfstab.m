.\"
.\" COPYRIGHT (C) 1998-2003 By Arthur Naseef
.\"
.\" This file is covered under the GNU General Public License Version 2.  For
.\"  more information, see the file COPYING.
.\"
.\"
.\" FILE: ovlfstab.m
.\"
.\" DESCRIPTION:
.\"	Manual page for the ovlfstab mount tab file's format.
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
.TH ovlfstab 5 "CURRENT_DATE" "Arthur Naseef" "Linux System Admin"
.\"
.\" SECTION BREAK
.\"
.SH NAME
ovlfstab -
format of the
mount table
for ovlfsmount.
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
.SH DESCRIPTION
Filesystem and selector entries
are defined
in the ovlfstab file
for use
by the \fBovlfsmount(8)\fR
program.
The format
of the file
is described below.
Please see
the manual page
for the \fBovlfsmount(8)\fR
program for more information.
.\"
.\" SUB-SECTION BREAK
.\"
.SS File Format
Below is a description
of the overall
file format.
Detailed descriptions
of each element
of the file format
are given later.
.\"
.P
The file consists
of one or more
of following elements:
.BL
.P
.LI
Prefilter Command
.P
.LI
Filesystem Declaration
.P
.LI
Filesystem Selector Declaration
.LE
.\"
.P
Each of these
may be used
multiple times,
in any order.
In addition
to these elements,
comments may be
placed on any line
in the file.
.\"
.\" SUB-SECTION BREAK
.\"
.SS Prefilter Command
Using the format below,
the prefilter command
changes the interpretation of
(i.e. filters)
the file
until the end of file
is reached.
Note that successive
filters work inline
with previous ones,
with the output
of earlier filters
providing input
to later filters.
The format
of a prefilter command
is as follows:
.sp 1
.ti +1i
`#!!' <command\-string>
.sp 1
where <command\-string>
is a string,
terminated by a newline,
interpreted by /bin/sh.
For example,
the following input:
.sp 1
.ti +1i
`#!!' /bin/m4 -DHOME=$HOME
.sp 1
would be interpreted
as follows:
.sp 1
.ti +1i
`/bin/sh -c "/bin/m4 -DHOME=$HOME"'
.\"
.\" SUB-SECTION BREAK
.\"
.SS Filesystem Declaration
Using the syntax below,
a single filesystem mount
can be specified
and given a name.
In addition to 
the mount point,
storage root,
and base root,
all filesystem options
can be specified.
.sp 1
.ti +1i
FILESYSTEM <name> <mount\-parms> END
.sp 1
where <name> is a string
that names the filesystem,
and <mount\-parms> defines
the mount point
and other mount parameters.
Please see the section,
\fBMOUNT PARAMETERS\fR,
below for a list
of parameters
and their syntax.
.\"
.P
The following
is an example declaration
of a filesystem
named "safe-root":
.sp 1
.nf
    FILESYSTEM "safe-root"
        mount-pt    /mnt/safe
        base_root   /
        storage     /ovl/safe-root
        options
          "magic,hidemagic,smagic=Magic_stg,bmagic=Magic_base"
    END
.fi
.\"
.\" SUB-SECTION BREAK
.\"
.SS Filesystem Selector Declaration
Two types of selectors exist,
CD and special.
The syntax of both
types of selector statements
is as follows:
.sp 1
.in +1i
.HI "CD SELECTOR"
<name> [<selector\-table>]
.HE
.br
END
.sp 1
.HI "SPECIAL SELECTOR"
<name> <selector\-command> [<selector\-table>]
.HE
.br
END
.\"
.sp 1
where <name>
is a string
that identifies the selector,
<selector\-command> is a string
used to determine the selector key
at mount time,
and <selector\-table>
maps keys to filesystem names,
defining which filesystem
is mounted for each key.
.\"
.P
The syntax for <selector\-command>
is as follows:
.sp 1
.ti +1i
COMMAND <program> [<arg> ...]
.sp 1
where <program> is a string
that specifies the
program to execute,
and <arg> is a string
passed to the program
as an argument.
For example:
.sp 1
.ti +1i
COMMAND "/usr/local/cd_vol_id" "-d" "/dev/cdrom"
.sp 1
Mapping of the selector keys
to filesystem names,
and, optionally, additional options
for each mount,
may be specified
in the <selector\-table>.
The table may contain
any number of entries;
the format of each entry
is as follows:
.sp 1
.ti +1i
KEY <key> <fs\-name> [<mount\-parm> ...]
.sp 1
where <key> is a string
containing the selector key,
<fs\-name> is a string
containing the name
of the filesystem
to be mounted,
and <mount\-parm> contains
a mount parameter.
Mount parameters specified
in the table
take precedence over
the same options
in the filesystem declaration.
.\"
.\" SUB-SECTION BREAK
.\"
.SS Comments
Limited commenting
is available
in the ovlfstab file.
Every line that
begins with the character,
\fB#\fR,
is considered a comment line.
All content through
the next newline is ignored.
Note that the \fB#\fR may
be preceeded by whitespace.
.\"
.\" SECTION BREAK
.\"
.SH MOUNT PARAMETERS
Below is a description
of each of the mount parameters
supported by \fBovlfsmount(8)\fR.
The syntax of each
is the parameter name
immediately followed
by a string
containing the value
for the parameter.
.TP
\fBBASE_ROOT\fR
Path to the root directory
for the mount's base filesystem.
.TP
\fBOPTIONS\fR
Mount options
passed to the ovlfs
at mount time.
.TP
\fBMOUNT-POINT\fR or \fBMOUNT-PT\fR
Mount point for the filesystem.
Access to the ovlfs
will be rooted
at this point.
.TP
\fBSTG_FILE\fR
Path to the storage file
into which the ovlfs
inodes and other
filesystem information
is stored persistently.
.TP
\fBSTG_METHOD\fR
Name of the storage method
used by the mount.
As of this writing,
only the storage method
named "Storage File"
is available.
.TP
\fBSTORAGE\fR
Path to the root directory
for the mount's storage filesystem.
.\"
.\" SECTION BREAK
.\"
.SH STRING FORMATS
Strings in the file
can be given
in one of
the following formats:
.BL
.sp 1
.LI
.HI "Quoted -"
Started with a
double or single quote,
all text
up to the next
unescaped, matching quote,
including whitespace
is included in the string.
The backslash character
is used to escape
the immediately following character.
No other special handling
is performed.
.sp 1
.LI
.HI "Special -"
Preceeded by the
total length of
the string,
enclosed in braces,
the string is
otherwise read from
the file as-is.
For example,
`{4}/mnt' is equivalent
to "/mnt".
.sp 1
.LI
.HI "Normal -"
Any string of
non-whitespace
characters in the file,
that is not a keyword,
is interpreted
as a string.
.\"
.P
Also note that adjacent strings,
without any whitespace
separating them,
are concatenated into
a single string.
.\"
.\" SECTION BREAK
.\"
.SH KEYWORDS
The following keywords
are recognized
by the \fBovlfsmount(8)\fR program
as of this writing.
Note that keywords
are case-insensitive.
.sp 1
.BL
.LI
BASE_ROOT
.LI
CD
.LI
COMMAND
.LI
END
.LI
FILESYSTEM
.LI
KEY
.LI
OPTIONS
.LI
MOUNT-POINT
.LI
MOUNT-PT
.LI
SELECTOR
.LI
SPECIAL
.LI
STG_FILE
.LI
STG_METHOD
.LI
STORAGE
.LE
.\"
.\" SECTION BREAK
.\"
.SH EXAMPLE
.nf
# safe-root creates an environment that can be used with chroot to give full
#  access to the root filesystem while protecting it from modification.

FILESYSTEM "safe-root"
	mount-pt     "/ovlfs/safe-root"
	base_root    "/"
	storage      "/ovlfs/safe-root.stg"
	stg_file     "/ovlfs_stg/safe-root.dat"
	options
          "magic,hidemagic,smagic=Magic_stg,bmagic=Magic_base"
END


# cd-video01 mounts a CD named "Video 01" (note the CD must already be mounted
#  on /mnt/cdrom).

FILESYSTEM "cd-video01"
	mount-pt    "/ovlfs/cd"
	base_root   "/mnt/cdrom"
	storage     "/ovlfs/cd-video01"
	stg_file    "/ovlfs_stg/cd-video01.dat"
END


# cd-video02 mounts a CD named "Video 02" (note the CD must already be mounted
#  on /mnt/cdrom).

FILESYSTEM "cd-video02"
	mount-pt    "/ovlfs/cd"
	base_root   "/mnt/cdrom"
	storage     "/ovlfs/cd-video02"
	stg_file    "/ovlfs_stg/cd-video02.dat"
END


# Select cd-video01 or cd-video02.

CD SELECTOR "cd-video"
	KEY "10A9FE3000AA5903"	"cd-video01"
	KEY "3A602AAB7FB4776E"	"cd-video02" options "nomagic"
END
.fi
.\"
.\" SECTION BREAK
.\"
.SH EXAMPLE WITH PREFILTERING
.nf
#!!/usr/bin/m4 -DOVL_MNTS_DIR=/ovlfs -DOVL_STGFILE_DIR=/ovlfs_stg
`define(m4_concat, $1$2$3$4$5$6$7$8$9)dnl'

# safe-root creates an environment that can be used with chroot to give full
#  access to the root filesystem while protecting it from modification.

FILESYSTEM "safe-root"
	mount-pt     `m4_concat'(OVL_MNTS_DIR, "/safe-root")
	base_root    "/"
	storage      `m4_concat'(OVL_MNTS_DIR, "/safe-root.stg")
	stg_file     `m4_concat'(OVL_STGFILE_DIR, "/safe-root.dat")
	options
          "magic,hidemagic,smagic=Magic_stg,bmagic=Magic_base"
END

# cd-video01 mounts a CD named "Video 01" (note the CD must already be mounted
#  on /mnt/cdrom).

FILESYSTEM "cd-video01"
	mount-pt    `m4_concat'(OVL_MNTS_DIR, "/cd")
	base_root   "/mnt/cdrom"
	storage     `m4_concat'(OVL_MNTS_DIR, "/cd-video01")
	stg_file    `m4_concat'(OVL_STGFILE_DIR, "/cd-video01.dat")
END


# cd-video02 mounts a CD named "Video 02" (note the CD must already be mounted
#  on /mnt/cdrom).

FILESYSTEM "cd-video02"
	mount-pt    `m4_concat'(OVL_MNTS_DIR, "/cd")
	base_root   "/mnt/cdrom"
	storage     `m4_concat'(OVL_MNTS_DIR, "/cd-video02")
	stg_file    `m4_concat'(OVL_STGFILE_DIR, "/cd-video02.dat")
END


# Select cd-video01 or cd-video02.

CD SELECTOR "cd-video"
	KEY "10A9FE3000AA5903"	"cd-video01"
	KEY "3A602AAB7FB4776E"	"cd-video02" options "nomagic"
END
.fi
.\"
.\" SECTION BREAK
.\"
.SH SEE ALSO
ovl(8),
ovlfsmount(8)
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
 @(#)ovlfstab.m	1.1	[03/07/27 22:20:33]
