/*
 * CP/M Archiver. Copyright 1983 Clifford Heath. See the LICENSE
 */
#include	"cpm.h"
#include	<stdio.h>
#include	<errno.h>

extern	int	errno;
char	buf[1024];
char	buf1[1024];
int	clobber = 0, verbose = 0, binary = 1;
int	nfiles;
char	cmd = 0;
char	*device;
char	**files;
int	replace(), delete(), extract();

main(argc,argv)
char **argv;
{
	register f;
	register char *p = argv[1];

	if (argc < 3)
		erexit("Usage: %s [tcrxd][v] device [file ...]\n",argv[0]);
	device = argv[2];
	files = argv+3;
	nfiles = argc-3;

	while (*p) switch(*p++) {
	case 't':	/* List */
	case 'x':	/* Extract */
	case 'r':	/* Replace */
	case 'd':	/* Delete */
		if (cmd)
			erexit("Only one of [tcrxd] may be specified\n");
		cmd = p[-1];
		break;
	case 'v':
		verbose++;
		break;
	case 'a':
		binary = 0;
		break;
	case 'c':
		clobber++;
		break;
	case '-': continue;

	default:
		printf("Unknown option: %c\n",p[-1]);
		exit(0);
	}

	if (cmd == 0)
		if (clobber)
			cmd = 'r';
		else
			erexit("One of [tcrxd] must be specified\n");
	if (!nfiles && cmd == 'd') {
		clobber++;
		cmd = 0;
	}
	if ((f = open(device,0)) < 0) {
		if (errno != ENOENT) {
			perror(device);
			exit(1);
		}
		printf("Create %s (y or n) ?",device);
		if (getchar() != 'y')
			erexit("abort\n");
		while(getchar() != '\n');
		close(f);
		cpm_start(device);
		cpm_format();
		clobber++;
	} else {
		cpm_start(device);
		if (clobber) {
			printf("Really clobber %s \7(y or n) ?",device);
			if (getchar() != 'y')
				erexit("abort\n");
			while(getchar() != '\n');
			cpm_format();
		} else if (!cpm_getdir()) {
			printf("\7Warning - directory consistency errors\n");
			if (cmd == 'r')
				erexit("Replace command disallowed\n");
		}
	}
	switch (cmd) {
	case 't': listdir(); break;
	case 'r': forall(replace); break;
	case 'd': forall(delete); break;
	case 'x': if (nfiles)
			forall(extract);
		  else
			extrall();
		  break;
	}
	cpm_end();
	exit(0);
}

erexit(s,a)
char	*s;
{
	fprintf(stderr,s,a);
	exit(1);
}

forall(f)
int	(*f)();
{
	while (nfiles--)
		(*f)(*files++);
}

listdir()
{
	long	size[NDIR];
	FCB	*d, *e;
	int	clusters = 0, files = 0;
	int	k, j, c;

	for (d = cpm_dir; d < cpm_dir+NDIR; d++) {
		if (d->inuse == 0xe5)
			continue;
		clusters += (d->records+7) >> 3;
		if (d->extent == 0)
			size[d-cpm_dir] = d->records;
		else for (e = cpm_dir; e < cpm_dir+NDIR; e++) {
			if (e == d || e->inuse == 0xe5)
				continue;
			if (namatch(d,e) && e->extent == 0) {
				size[e-cpm_dir] += d->records;
				break;
			}
		}
	}
	for (d = cpm_dir; d < cpm_dir+NDIR; d++) {
		if (d->inuse == 0xe5 || d->extent)
			continue;
		files++;
		for (k = 0; k < 8; k++) {
			c = d->name[k];
			if (c == ' ')
				break;
			else putchar(lcase(c));
		}
		j = k;
		putchar('.');
		for (; d->ext[k-j] != ' ' && k < (j+3); k++)
			putchar(lcase(d->ext[k-j]));
		if (verbose) {
			while (k++ < 13) /* Count up to 14 chars output */
				putchar(' ');
			printf("%9D \t",size[d-cpm_dir]*128L);
			if (~files&01)
				putchar('\n');
		} else
			putchar('\n');
	}
	if (!verbose)
		return;
	if (files & 01)
		putchar('\n');
	printf("\n%d files, %dk used, %dk free\n",files,clusters,cpm_fclus);
}

replace(f)
char	*f;
{
	int	fd;
	long	a = 0;
	int	r;
	int	ret = 0;		/* Last char was return flag */
	FCB	*fcb;
	register char *p, *q;

	if ((fd = open(f,0)) < 0) {
		perror(f);
		cpm_close(fcb);
		return;
	}
	show('r',f);
	if ((fcb = cpm_creat(f)) == 0) {
		printf("%s: Can't create on %s\n",f,device);
		return;
	}
	p = buf1;
	while ((r = read(fd,buf,1024)) > 0) {
		if (!binary) {
			/*
			 *	Pack buf1, inserting '\r' before '\n'
			 */
			for (q = buf; q < buf+r;) {
				/* Don't insert \r if there was one */
				if (*q == '\n' && !ret)
					*p++ = '\r';
				else
					*p++ = *q++;
				if (p[-1] == '\r')
					ret = 1;
				else
					ret = 0;
				/* Time to flush output buffer ? */
				if (p == buf1+1024) {
					if (cpm_write(fcb,buf1,a,1024)
					          != 1024) {
					   printf("Write error on %s\n",device);
					   goto out;
					}
					a += 1024;
					p = buf1;
				}
			}
		} else {
			if (cpm_write(fcb,buf,a,r) != r) {
				printf("Write error on %s\n",device);
				goto out;
			}
			a += r;
		}
	}

	if (!binary && p > buf1)
		cpm_write(fcb,buf1,a,p-buf1);
out:	close(fd);
	cpm_close(fcb);
}

extract(f)
char	*f;
{
	int	fd;
	long	a = 0;
	int	r;
	FCB	*fcb;
	register char *p, *q;

	if ((fcb = cpm_open(f)) == 0) {
		printf("%s: Not found on %s\n",f,device);
		return;
	}
	show('x',f);
	if ((fd = creat(f,0666)) < 0) {
		perror(f);
		cpm_close(fcb);
		return;
	}
	while ((r = cpm_read(fcb,buf,a,1024)) > 0) {
		if (!binary) {
			for (p = buf, q = buf1; p < buf+r; p++)
				switch(*p) {
				case '\032':	/* End of file */
					goto wr;
				case '\r':
					*p = '\n';
				default:
					*q++ = *p;
				case '\n':
					continue;
				}

		wr:	if (write(fd,buf1,q-buf1) != q-buf1) {
				printf("Write error on %s\n",f);
				goto out;
			}
		} else if (write(fd,buf,r) != r) {
				printf("Write error on %s\n",f);
				goto out;
		}
		a += r;
	}
out:	close(fd);
	cpm_close(fcb);
}

delete(f)
char	*f;
{
	if (cpm_delete(f))
		show('d',f);
	else
		printf("%f not found on %s\n",f,device);
}

extrall()
{
	FCB	*d;
	register char	*p, *q;
	char	file[12];

	for (d = cpm_dir; d < cpm_dir+NDIR; d++) {
		if (d->inuse == 0xE5 || d->extent != 0)
			continue;
		q = file;
		for (p = d->name; p < d->name+8; p++)
			if (*p != ' ')
				*q++ = lcase(*p);
		*q++ = '.';
		for (p = d->ext; p < d->ext+3; p++)
			if (*p != ' ')
				*q++ = lcase(*p);
		*q++ = 0;
		extract(file);
	}
}

lcase(c)
{
	if (c >= 'A' && c <= 'Z')
		return c+'a'-'A';
	return c;
}

show(c,f)
char	c, *f;
{
	if (verbose)
		printf("%c - %s\n",c,f);
}
