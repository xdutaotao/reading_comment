/*
 * CD Info 1.0 - prints various information about a CD,
 *               detects the type of the CD.
 *
 *  (c) 1996,1997  Gerd Knorr <kraxel@cs.tu-berlin.de>
 *       and       Heiko Eissfeldt <heiko@colossus.escape.de>
 *
 * compile: 'make cdinfo'
 * usage: 'cdinfo  [ dev ]' --  dev defaults to /dev/cdrom
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>

/*
Subject:   -65- How can I read an IRIX (EFS) CD-ROM on a machine which
                doesn't use EFS?
Date: 18 Jun 1995 00:00:01 EST

  You want 'efslook', at
  ftp://viz.tamu.edu/pub/sgi/software/efslook.tar.gz.

and
! Robert E. Seastrom <rs@access4.digex.net>'s software (with source
! code) for using an SGI CD-ROM on a Macintosh is at
! ftp://bifrost.seastrom.com/pub/mac/CDROM-Jumpstart.sit151.hqx.

*/

#define FS_NO_DATA              0   /* audio only */
#define FS_HIGH_SIERRA		1
#define FS_ISO_9660		2
#define FS_INTERACTIVE		3
#define FS_HFS			4
#define FS_UFS			5
#define FS_EXT2			6
#define FS_ISO_HFS              7  /* both hfs & isofs filesystem */
#define FS_ISO_9660_INTERACTIVE 8  /* both CD-RTOS and isofs filesystem */
#define FS_UNKNOWN	       15
#define FS_MASK		       15

#define XA		       16
#define MULTISESSION	       32
#define PHOTO_CD	       64
#define HIDDEN_TRACK          128
#define CDTV		      256
#define BOOTABLE       	      512
#define VIDEOCDI       	     1024

#if 0
#define STRONG "\033[1m"
#define NORMAL "\033[0m"
#else
#define STRONG "__________________________________\n"
#define NORMAL ""
#endif

int filehandle;                                   /* Handle of /dev/>cdrom< */
int rc;                                                      /* return code */
int i,j;                                                           /* index */
int isofs_size = 0;                                      /* size of session */
int start_track;                                   /* first sector of track */
int ms_offset;                /* multisession offset found by track-walking */
int data_start;                                       /* start of data area */

char buffer[2048];                                           /* for CD-Data */
char buffer2[2048];                                          /* for CD-Data */
char buffer3[2048];                                          /* for CD-Data */
char buffer4[2048];                                          /* for CD-Data */
char buffer5[2048];                                          /* for CD-Data */

char                       toc_header[2];               /* first/last Track */
struct cdrom_tocentry      *toc[CDROM_LEADOUT+1];            /* TOC-entries */
struct cdrom_mcn           mcn;
struct cdrom_multisession  ms;
struct cdrom_subchnl       sub;
int                        first_data = -1;        /* # of first data track */
int                        num_data = 0;                /* # of data tracks */
int                        first_audio = -1;      /* # of first audio track */
int                        num_audio = 0;              /* # of audio tracks */

/* ------------------------------------------------------------------------ */
/* some iso9660 fiddeling                                                   */

void read_super(int offset)
{
    /* sector 16, super block */
    memset(buffer,0,2048);
    lseek(filehandle,2048*(offset+16),SEEK_SET);
    read(filehandle,buffer,2048);
}

void read_super2(int offset)
{
    /* sector 0, for photocd check */
    memset(buffer2,0,2048);
    lseek(filehandle,2048*offset,SEEK_SET);
    read(filehandle,buffer2,2048);
}

void read_super3(int offset)
{
    /* sector 4, for ufs check */
    memset(buffer3,0,2048);
    lseek(filehandle,2048*(offset+4),SEEK_SET);
    read(filehandle,buffer3,2048);
}

void read_super4(int offset)
{
    /* sector 17, for bootable CD check */
    memset(buffer4,0,2048);
    lseek(filehandle,2048*(offset+17),SEEK_SET);
    read(filehandle,buffer4,2048);
}

void read_super5(int offset)
{
    /* sector 150, for Video CD check */
    memset(buffer5,0,2048);
    lseek(filehandle,2048*(offset+150),SEEK_SET);
    read(filehandle,buffer5,2048);
}

int is_isofs(void)
{
    return 0 == strncmp(&buffer[1],"CD001",5);
}

int is_hs(void)
{
    return 0 == strncmp(&buffer[9],"CDROM",5);
}

int is_cdi(void)
{
    return (0 == strncmp(&buffer[1],"CD-I",4));
}

int is_cd_rtos(void)
{
    return (0 == strncmp(&buffer[8],"CD-RTOS",7));
}

int is_bridge(void)
{
    return (0 == strncmp(&buffer[16],"CD-BRIDGE",9));
}

int is_xa(void)
{
    return 0 == strncmp(&buffer[1024],"CD-XA001",8);
}

int is_cdtv(void)
{
    return (0 == strncmp(&buffer[8],"CDTV",4));
}

int is_photocd(void)
{
    return 0 == strncmp(&buffer2[64], "PPPPHHHHOOOOTTTTOOOO____CCCCDDDD", 24);
}

int is_hfs(void)
{
    return (0 == strncmp(&buffer2[512],"PM",2)) ||
           (0 == strncmp(&buffer2[512],"TS",2)) ||
	   (0 == strncmp(&buffer2[1024], "BD",2));
}

int is_ext2(void)
{
    return 0 == strncmp(&buffer2[0x438],"\x53\xef",2);
}

int is_ufs(void)
{
    return 0 == strncmp(&buffer3[1372],"\x54\x19\x01\x0" ,4);
}

int is_bootable(void)
{
    return 0 == strncmp(&buffer4[7],"EL TORITO",9);
}

int is_video_cdi(void)
{
    return 0 == strncmp(&buffer5[0],"VIDEO_CD",8);
}

int get_size(void)		/* iso9660 volume space in 2048 byte units */
{
    return ((buffer[80] & 0xff) |
	    ((buffer[81] & 0xff) << 8) |
	    ((buffer[82] & 0xff) << 16) |
	    ((buffer[83] & 0xff) << 24));
}

int guess_filesystem(int start_session)
{
    int ret = 0;

    read_super(start_session);

#undef _DEBUG
#ifdef _DEBUG
    /* buffer is defined */
    if (is_cdi())     printf("CD-I, ");
    if (is_cd_rtos()) printf("CD-RTOS, ");
    if (is_isofs())   printf("ISOFS, ");
    if (is_hs())      printf("HS, ");
    if (is_bridge())  printf("BRIDGE, ");
    if (is_xa())      printf("XA, ");
    if (is_cdtv())    printf("CDTV, ");
    puts("");
#endif

    /* filesystem */
    if (is_cdi() && is_cd_rtos() && !is_bridge() && !is_xa()) {
        return FS_INTERACTIVE;
    } else {	/* read sector 0 ONLY, when NO greenbook CD-I !!!! */

        read_super2(start_session);

#ifdef _DEBUG
	/* buffer2 is defined */
	if (is_photocd()) printf("PHOTO CD, ");
	if (is_hfs()) printf("HFS, ");
	if (is_ext2()) printf("EXT2 FS, ");
	puts("");
#endif
        if (is_hs())
	    ret |= FS_HIGH_SIERRA;
	else if (is_isofs()) {
	    if (is_cd_rtos() && is_bridge())
	        ret = FS_ISO_9660_INTERACTIVE;
	    else if (is_hfs())
	        ret = FS_ISO_HFS;
	    else
	        ret = FS_ISO_9660;
	    isofs_size = get_size();

	    read_super4(start_session);

#ifdef _DEBUG
	    /* buffer4 is defined */
	    if (is_bootable()) printf("BOOTABLE, ");
	    puts("");
#endif
	    if (is_bootable())
		ret |= BOOTABLE;

	    if (is_bridge() && is_xa() && is_isofs() && is_cd_rtos()) {
	        read_super5(start_session);

#ifdef _DEBUG
		/* buffer5 is defined */
		if (is_video_cdi()) printf("VIDEO-CDI, ");
		puts("");
#endif
		if (is_video_cdi())
		    ret |= VIDEOCDI;
	    }
	} else if (is_hfs())
	    ret |= FS_HFS;
	else if (is_ext2())
	    ret |= FS_EXT2;
	else {

	    read_super3(start_session);

#ifdef _DEBUG
	    /* buffer3 is defined */
	    if (is_ufs()) printf("UFS, ");
	    puts("");
#endif
	    if (is_ufs())
	        ret |= FS_UFS;
	    else
	        ret |= FS_UNKNOWN;
	}
    }
    /* other checks */
    if (is_xa())
        ret |= XA;
    if (is_photocd())
	ret |= PHOTO_CD;
    if (is_cdtv())
	ret |= CDTV;
    return ret;
}

/* ------------------------------------------------------------------------ */

char *devname = "/dev/cdrom";
char *progname;

int
main(int argc, char *argv[])
{
    int fs=0;
    int need_lf;
    progname = strrchr(argv[0],'/');
    progname = progname ? progname+1 : argv[0];
    if (argc > 1) devname = argv[1];

    printf("CD Info 1.0 | (c) 1996 Gerd Knorr & Heiko Eiﬂfeldt\n");

    /* open device */
    filehandle = open(devname,O_RDONLY);
    if (filehandle == -1) {
	fprintf(stderr,"%s: %s: %s\n",progname, devname, strerror(errno));
	exit(1);
    }

    /* read the number of tracks from CD*/
    if (ioctl(filehandle,CDROMREADTOCHDR,&toc_header)) {
	fprintf(stderr,"%s: read TOC ioctl failed, give up\n",progname);
	return(0);
    }
    
    printf(STRONG "track list (%i - %i)\n" NORMAL,
	   toc_header[0],toc_header[1]);
    
    /* read toc */
    for (i = toc_header[0]; i <= CDROM_LEADOUT; i++) {
	toc[i] = malloc(sizeof(struct cdrom_tocentry));
	if (toc[i] == NULL) {
	    fprintf(stderr, "out of memory\n");
	    exit(1);
	}
	memset(toc[i],0,sizeof(struct cdrom_tocentry));
	toc[i]->cdte_track  = i;
	toc[i]->cdte_format = CDROM_MSF;
	if (ioctl(filehandle,CDROMREADTOCENTRY,toc[i])) {
	    fprintf(stderr,
		    "%s: read TOC entry ioctl failed for track %i, give up\n",
		    progname,toc[i]->cdte_track);
	    exit(1);
	}
	printf("%3d: %02d:%02d:%02d (%06d) %s%s\n",
	       (int)toc[i]->cdte_track,
	       (int)toc[i]->cdte_addr.msf.minute,
	       (int)toc[i]->cdte_addr.msf.second,
	       (int)toc[i]->cdte_addr.msf.frame,
	       (int)toc[i]->cdte_addr.msf.frame +
	       (int)toc[i]->cdte_addr.msf.second*75 + 
	       (int)toc[i]->cdte_addr.msf.minute*75*60 - 150,
	       (toc[i]->cdte_ctrl & CDROM_DATA_TRACK) ? "data ":"audio",
	       CDROM_LEADOUT == i ? " (leadout)" : "");
	if (i == CDROM_LEADOUT)
	    break;
	if (toc[i]->cdte_ctrl & CDROM_DATA_TRACK) {
	    num_data++;
	    if (-1 == first_data)
		first_data = toc[i]->cdte_track;
	} else {
	    num_audio++;
	    if (-1 == first_audio)
		first_audio = toc[i]->cdte_track;
	}
	/* skip to leadout */
	if (i == (int)(toc_header[1]))
	    i = CDROM_LEADOUT-1;
    }
    
    printf(STRONG "what ioctl's report\n" NORMAL);

    /* get mcn */
    printf("get mcn     : "); fflush(stdout);
    if (ioctl(filehandle,CDROM_GET_MCN,&mcn))
	printf("FAILED\n");
    else
	printf("%s\n",mcn.medium_catalog_number);

    /* get disk status */
    printf("disc status : "); fflush(stdout);
    switch (ioctl(filehandle,CDROM_DISC_STATUS,0)) {
    case CDS_NO_INFO: printf("no info\n"); break;
    case CDS_NO_DISC: printf("no disc\n"); break;
    case CDS_AUDIO:   printf("audio\n"); break;
    case CDS_DATA_1:  printf("data mode 1\n"); break;
    case CDS_DATA_2:  printf("data mode 2\n"); break;
    case CDS_XA_2_1:  printf("XA mode 1\n"); break;
    case CDS_XA_2_2:  printf("XA mode 2\n"); break;
    default:          printf("unknown (failed?)\n");
    }
    
    /* get multisession */
    printf("multisession: "); fflush(stdout);
    ms.addr_format = CDROM_LBA;
    if (ioctl(filehandle,CDROMMULTISESSION,&ms))
	printf("FAILED\n");
    else
	printf("%d%s\n",ms.addr.lba,ms.xa_flag?" XA":"");

    /* get audio status from subchnl */
    printf("audio status: "); fflush(stdout);
    sub.cdsc_format = CDROM_MSF;
    if (ioctl(filehandle,CDROMSUBCHNL,&sub))
	printf("FAILED\n");
    else {
	switch (sub.cdsc_audiostatus) {
	case CDROM_AUDIO_INVALID:    printf("invalid\n");   break;
	case CDROM_AUDIO_PLAY:       printf("playing");     break;
	case CDROM_AUDIO_PAUSED:     printf("paused");      break;
	case CDROM_AUDIO_COMPLETED:  printf("completed\n"); break;
	case CDROM_AUDIO_ERROR:      printf("error\n");     break;
	case CDROM_AUDIO_NO_STATUS:  printf("no status\n"); break;
	default:                     printf("Oops: unknown\n");
	}
	if (sub.cdsc_audiostatus == CDROM_AUDIO_PLAY ||
	    sub.cdsc_audiostatus == CDROM_AUDIO_PAUSED) {
	    printf(" at: %02d:%02d abs / %02d:%02d track %d\n",
		   sub.cdsc_absaddr.msf.minute,
		   sub.cdsc_absaddr.msf.second,
		   sub.cdsc_reladdr.msf.minute,
		   sub.cdsc_reladdr.msf.second,
		   sub.cdsc_trk);
	}
    }
    printf(STRONG "try to find out what sort of CD this is\n" NORMAL);

    /* try to find out what sort of CD we have */
    if (0 == num_data) {
	/* no data track, may be a "real" audio CD or hidden track CD */
	start_track = (int)toc[1]->cdte_addr.msf.frame +
	    (int)toc[1]->cdte_addr.msf.second*75 + 
	    (int)toc[1]->cdte_addr.msf.minute*75*60 - 150;
	/* CD-I/Ready says start_track <= 30*75 then CDDA */
	if (start_track > 100 /* 100 is just a guess */) {
	    fs = guess_filesystem(0);
	    if ((fs & FS_MASK) != FS_UNKNOWN)
		fs |= HIDDEN_TRACK;
	    else {
		fs &= ~FS_MASK; /* del filesystem info */
		printf("Oops: %i unused sectors at start, but hidden track check failed.\n",start_track);
	    }
	}
    } else {
        /* we have data track(s) */
	for (j = 2, i = first_data; i <= toc_header[1]; i++) {
	    if (!(toc[i]->cdte_ctrl & CDROM_DATA_TRACK))
	        break;
	    start_track = (i == 1) ? 0 :
		    (int)toc[i]->cdte_addr.msf.frame +
		    (int)toc[i]->cdte_addr.msf.second*75 + 
		    (int)toc[i]->cdte_addr.msf.minute*75*60
		    -150;
	    /* save the start of the data area */
	    if (i == first_data)
	        data_start = start_track;
	    
	    /* skip tracks which belong to the current walked session */
	    if (start_track < data_start + isofs_size)
	        continue;

	    fs = guess_filesystem(start_track);
	    if (!(((fs & FS_MASK) == FS_ISO_9660 ||
		   (fs & FS_MASK) == FS_ISO_HFS ||
		   (fs & FS_MASK) == FS_ISO_9660_INTERACTIVE) && (fs & XA)))
	        break;	/* no method for non-iso9660 multisessions */

	    if (i > 1) {
		/* track is beyond last session -> new session found */
		ms_offset = start_track;
		printf("session #%d starts at track %2i, offset %6i, isofs size %6i\n",
		       j++,toc[i]->cdte_track,start_track,isofs_size);
		fs |= MULTISESSION;
	    }
	}
    }

    switch(fs & FS_MASK) {
    case FS_NO_DATA:
	if (num_audio > 0)
	    printf("Audio CD\n");
	break;
    case FS_ISO_9660:
	printf("CD-ROM with iso9660 fs\n");
	break;
    case FS_ISO_9660_INTERACTIVE:
	printf("CD-ROM with CD-RTOS and iso9660 fs\n");
	break;
    case FS_HIGH_SIERRA:
	printf("CD-ROM with high sierra fs\n");
	break;
    case FS_INTERACTIVE:
	printf("CD-Interactive%s\n", num_audio > 0 ? "/Ready" : "");
	break;
    case FS_HFS:
	printf("CD-ROM with Macintosh HFS\n");
	break;
    case FS_ISO_HFS:
	printf("CD-ROM with both Macintosh HFS and iso9660 fs\n");
	break;
    case FS_UFS:
	printf("CD-ROM with Unix UFS\n");
	break;
    case FS_EXT2:
	printf("CD-ROM with Linux second extended filesystem\n");
	break;
    case FS_UNKNOWN:
	printf("CD-ROM with unknown filesystem\n");
	break;
    }
    switch(fs & FS_MASK) {
    case FS_ISO_9660:
    case FS_ISO_9660_INTERACTIVE:
    case FS_ISO_HFS:
	printf("iso9660: %i MB size, label `%.32s'\n",
	       isofs_size/512,buffer+40);
	break;
    }
    need_lf = 0;
    if (first_data == 1 && num_audio > 0)
	need_lf += printf("mixed mode CD   ");
    if (fs & XA)
	need_lf += printf("XA sectors   ");
    if (fs & MULTISESSION)
	need_lf += printf("Multisession, offset = %i   ",ms_offset);
    if (fs & HIDDEN_TRACK)
	need_lf += printf("Hidden Track   ");
    if (fs & PHOTO_CD)
	need_lf += printf("%sPhoto CD   ", num_audio > 0 ? " Portfolio " : "");
    if (fs & CDTV)
	need_lf += printf("Commodore CDTV   ");
    if (first_data > 1)
	need_lf += printf("CD-Plus/Extra   ");
    if (fs & BOOTABLE)
	need_lf += printf("bootable CD   ");
    if (fs & VIDEOCDI && num_audio == 0)
	need_lf += printf("Video CD   ");
    if (need_lf) puts("");

    exit(0);
}
