/*
 *	Copyright 1983 Clifford Heath. See the LICENSE
 */
#include	<stdio.h>
#include	<errno.h>
#include	"cpm.h"

int	floppydes = -1;
long	lseek();

cpm_dev(name)
char *name;
{
	extern	int	errno;
	int	mode = 2;

	if (floppydes >= 0)
		close(floppydes);
loop:
	if ((floppydes = open(name,mode)) < 0) {
		if (floppydes = creat(name,0666) < 0) {
			perror(name);
			exit(1);
		} else {
			close(floppydes);
			goto loop;
		}
		if (mode) {	/* Try read only ? */
			mode = 0;
			goto loop;
		}
		perror(name);
		exit(1);
	}
	return mode;
}

long
trans(clus,rec)
register int clus,rec;
{
	register logical, track, sector;

#ifdef	DEBUG
	printf(" cluster %d , record %d",clus,rec);
#endif
	if (clus < 0)
		printf("System error - negative cluster\n");
	logical = clus*8+rec;
	track = logical/26 + 2;
	sector = logical%26;
	if (sector < 13)
		sector = (sector*6)%26;
	else
		sector = (sector*6)%26 + 1;
#ifdef	DEBUG
	printf(" translated to %d,%d\n",track,sector);
#endif
	return (long)(track*26 + sector) * 128L;
}

clread(clus,buff)
register byte clus;
register char * buff;
{
	register rec;
	register ok = 1;
#ifdef	DEBUG
	printf("Reading");
#endif
	for (rec = 0; rec < 8; rec++) {
		lseek(floppydes, trans((int)clus,rec), 0);
		if (read(floppydes,buff,128) != 128)
			ok = 0;
		buff += 128;
	}
	return ok;
}

clwrite(clus,buff)
register byte clus;
register char *buff;
{
	register rec;
	register ok = 1;
#ifdef	DEBUG
	printf("Writing");
#endif
	for (rec = 0; rec < 8; rec++) {
		lseek(floppydes, trans((int)clus,rec), 0);
		if (write(floppydes,buff,128) != 128)
			ok = 0;
		buff += 128;
	}
	return ok;
}
