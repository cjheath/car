/*
 *	CP/M	HEADER
 *
 *	This is the CP/M * system written under UNIX 
 *	by Clifford Heath.
 *	* ( CP/M is a trademark of Digital Equipment Corp. )
 *
 *	Copyright 1983 Clifford Heath. See the LICENSE
 */
#define	DPC	32		/* Directory entries per cluster */
#define	NDIR	(2*DPC)		/* Directory is two clusters */
#define	NFILE	10		/* Max open files */
#define	NCLUS	244

typedef	unsigned char	byte;

typedef	struct dfcb {
	byte	inuse;		/* 0xe5 in this byte indicates unused entry */
	char	name[8];
	char	ext[3];
	byte	extent;		/* an extent is 16K of space */
	char	fill[2];
	byte	records;	/* total no. of records in extent */
	byte	cls[16];	/* 16 separate cluster numbers for the 16K */
}	FCB;

extern	byte	freeclus[31];	/* 31 * 8 bit map of free clusters */
extern	FCB	cpm_dir[NDIR];	/* work directory block */
extern	int	cpm_fclus;	/* Number of free clusters */
extern	int	floppydes;	/* File descriptor for floppy */
FCB	*cpm_open(), *cpm_creat();

#define	clisused(n)		(freeclus[n/8] & (01 << (n%8)))
#define	cluse(n)		((freeclus[n/8] |= 01 << (n%8)),cpm_fclus--)
#define	clunuse(n)		((freeclus[n/8] &= ~(01 << (n%8))),cpm_fclus++)
#define	namatch(fcb1,fcb2)	(strncmp((fcb1)->name,(fcb2)->name,8+3)==0)
