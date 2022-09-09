/*
 * CP/M Archiver. Copyright 1983 Clifford Heath. See the LICENSE
 */
#include	"cpm.h"

FCB	cpm_dir[NDIR];
char	cpm_buf[1024];
int	cpm_dirty;
byte	freeclus[31];
int	cpm_fclus;

FCB	*findent(),
	*mkentry(),
	*cpm_fopen();

cpm_format()
{
	byte	k;
	byte	*p;

	for (p = (byte *)cpm_dir; p < (byte *)(cpm_dir+NDIR);)
		*p++ = 0xE5;
	cpm_dirty = 1;
}

cpm_start(name)
char *name;
{
	int	k;

	cpm_dev(name);
	for (k = 0; k < 31; k++)
		freeclus[k] = 0;
	cpm_fclus = 241;
	freeclus[0] = 03;		/* reserve directory space */
}

cpm_getdir()
{
	int	ok = 1;
	byte	b;
	int	j,n;
	FCB	*d;

	/* Read directory */
	for (b = 0; b < 2; b++)
		if (!clread(b,(char *)(cpm_dir+b*DPC))) {
			printf("Directory read error\n");
			return 0;
		}
	for (d = cpm_dir; d < cpm_dir+NDIR; d++) {
		if (d->inuse != 0xE5) {	/* entry is used */
			for (j = 0; j < 16; j++) {
				if ((n = d->cls[j]) == 0)
					continue;
				if (!clisused(n))
					cluse(n);
				else
					ok = 0;
			/*	printf("Clus %d used twice\n",n); */
			}
		}
	}
	cpm_dirty = 0;
	return ok;
}

cpm_end()
{
	byte	k;

	if (!cpm_dirty)
		return;
	for (k = 0; k < 2; k++)
		if (!clwrite(k,(char *)(cpm_dir+k*DPC)))
			printf("Directory write error\n");
	cpm_dirty = 0;
	close(floppydes);
}

FCB *
cpm_creat(name)
byte	*name;
{
	cpm_delete(name);
	return cpm_fopen(name,1);
}

FCB *
cpm_open(name)
byte	*name;
{
	return cpm_fopen(name,0);
}

FCB *
cpm_fopen(name,cr)
byte	*name;
{
	FCB	*d;
	FCB	ff;

	formatfcb(&ff,name);
	if ((d = findent(&ff,0)) == 0)	/* File didn't exist */
		if (!cr || (d = mkentry(&ff,0)) == 0)
			return 0;
	return d;
}

/*
 *	Write to file
 */
cpm_write(fcb,buf,where,count)
FCB	*fcb;
char	*buf;
long	where;
{
	byte	*b;
	FCB	*d;
	int	ext, clus, offs;
	int	i, c = count;
	int	new;		/* This cluster has just been allocated */

	if (fcb == 0)
		return -1;
	while (count > 0) {
		cpm_dirty = 1;
		ext = where/16384L;
		clus = (int)(where%16384L)/1024;
		offs = where%1024;
		/* Make sure we are on correct extent */
		if (ext != fcb->extent) {
			/* Get or make next extent */
			if ((d = mkentry(fcb,ext)) == 0)
				return c-count;
			if (ext > fcb->extent)
				fcb->records = 128;	/* extend if necess */
			fcb = d;
		}
		/* Make sure a cluster is allocated */
		b = &fcb->cls[clus];
		new = 0;
		if (*b == 0) {
			if (!(*b = getcluster()))
				return c-count;
			new = 1;
			if (offs || offs+count < 1024)
				for (i = 0; i < offs;)
					cpm_buf[i++] = 0;
		} else if (offs && !clread(*b,cpm_buf))
			return c-count;		/* Read error */
		for (i = offs; count > 0 && i < 1024; count--, where++)
			cpm_buf[i++] = *buf++;
		/* This should pad with zero if we are not past old eof */
		if (new)
			while (i < 1024)
				cpm_buf[i++] = '\032';
		fcb->records = ((where%16384)+127)/128;
		if (fcb->records == 0)		/* At end of extent */
			fcb->records = 128;
		if (clwrite(*b,cpm_buf))
			return c-count;		/* Write error */
			/* Note that size will be wrong if this happens */
	}
	return c;
}

cpm_read(fcb,buf,where,count)
FCB	*fcb;
char	*buf;
long	where;
{
	FCB	*d;
	int	ext, clus, offs;
	int	i, c = count, cx;
	byte	*b;

	if (fcb == 0)
		return 0;
	while (count > 0) {
		ext = where/16384L;
		clus = (int)(where%16384L)/1024;
		offs = where%1024;
		/* Make sure we are on correct extent */
		if (ext != fcb->extent) {
			/* Get next extent */
			if ((d = findent(fcb,ext)) == 0)
				return c-count;
			fcb = d;
		}
		/* Check for end of file */
		if (fcb->records < 128) {
			/* Do we need to truncate ? */
			cx = fcb->records*128L - (where+count)%16384;
			if (cx < 0) {
				c += cx;
				count += cx;
			}
		}
		if (count <= 0)	/* Already at or past end of file */
			return 0;
		/* Check that a cluster is allocated */
		b = &fcb->cls[clus];
		if (*b == 0) {	/* None allocated, so pretend */
			for (i = offs; count > 0 && i < 1024; count--, where++)
				*buf++ = 0;
			continue;
		}
		if (!clread(*b,cpm_buf))
			return c-count;		/* Read error */
		for (i = offs; count > 0 && i < 1024; count--, where++)
			*buf++ = cpm_buf[i++];
	}
	return c;
}

cpm_close(fcb)
FCB	*fcb;
{
	;
}

/*
 *	Delete the named file
 */
cpm_delete(name)
byte	*name;
{
	FCB	*d;
	byte	*b;
	static	FCB	ff;
	int	found = 0;

	formatfcb(&ff,name);
	for (d = cpm_dir; d < cpm_dir+NDIR; d++) {
		if (d->inuse == 0xE5)
			continue;
		if (!namatch(d,&ff))
			continue;
		found = 1;
		cpm_dirty = 1;
		d->inuse = 0xE5;
		for (b = d->cls; b < d->cls+16; b++) {
			if (*b)
				clunuse(*b);
			*b = 0;
		}
	}
	return found;
}

FCB *
mkentry(fcb,ext)
FCB	*fcb;
{
	FCB	*d;

	if ((d = findent(fcb,ext)) != 0)
		return d;		/* One already existed */
	for (d = cpm_dir; d < cpm_dir+NDIR; d++)
		if (d->inuse == 0xE5) {
			byte	*p;

			copyfcb(d,fcb);
			for (p = d->cls; p < d->cls+16;)
				*p++ = 0;
			cpm_dirty = 1;
			d->extent = ext;
			return d;
		}
	return 0;
}

FCB	*
findent(fcb,ext)
FCB	*fcb;
{
	FCB	*d;

	for (d = cpm_dir; d < cpm_dir+NDIR; d++) {
		if (d->inuse == 0xE5)
			continue;
		if (d->extent != ext)
			continue;
		if (!namatch(fcb,d))
			continue;
		return d;
	}
	return 0;
}

getcluster() /* find a free cluster */
{
	int	i;

	if (!cpm_fclus)
		return 0;
	for (i = 0; i < NCLUS; i++)
		if (freeclus[i/8] == 0xFF)	/* All allocated */
			i |= 07;		/* So race forwards */
		else if (!clisused(i)) {
			cluse(i);
			return i;
		}
	/* Shouldn't get here ! */
	return 0;
}

formatfcb(fcb,nam)
FCB	*fcb;
byte	*nam;
{
	register byte	*p;
	register char	*q;

	for (p = nam; *p; p++);
	while (p >= nam && *p != '/')
		p--;
	nam = p+1;
	for (q = fcb->name; q < fcb->name+8; q++)
		if (*nam != '.' && *nam)
			*q = ucase((char)*nam++);
		else
			*q = ' ';
	while (*nam && *nam++ != '.')
		;
	for (q = fcb->ext; q < fcb->ext+3; q++)
		if (*nam)
			*q = ucase((char)*nam++);
		else
			*q = ' ';
	for (p = fcb->cls; p < fcb->cls+16;)
		*p++ = 0;
	fcb->extent = 0;
	fcb->inuse = 0;
	fcb->records = 0;
}

copyfcb(fcb1,fcb2)
FCB	*fcb1,*fcb2;
{
	int	n;
	register char *p,*q;

	p = (char *)fcb1;
	q = (char *)fcb2;
	for (n = 0; n < 32; n++)
		*p++ = *q++;
}

ucase(c)
char	c;
{
	return (c>='a' && c<='z') ? (c-32) : c;
}
