/* ls - list directory entries */

/*  for Human68k  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <jfctype.h>
#include <doslib.h>
#include <files.h>

const char *montab[] = {
	"???", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
	"Aug", "Sep", "Oct", "Nov", "Dec", "???", "???", "???"
};

char *def_argv[1] = { "." };


void *xrealloc(ptr, size)
void *ptr;
unsigned int size;
{
	return (ptr ? realloc(ptr, size) : malloc(size));
}

char *strdup(s)
char *s;
{
	char *retval;
	if (retval = malloc(strlen(s) + 1)) strcpy(retval, s);
	return retval;
}

char *strtail(s)
char *s;
{
	return (s + strlen(s));
}

char *stpcpy(s1, s2)
char *s1, *s2;
{
	return strtail(strcpy(s1, s2));
}



#define FA_ALL 0xFF
#define FA_SUBDIR 0x100
#define FA_ROOTDIR 0x200

#define WIDTH 80  /* width of console */
#define NTAB 8  /* tab length */

typedef int bool;

#define TRUE 1
#define FALSE 0

typedef struct entry {
	unsigned char *name;
	unsigned short mode;
	unsigned short int time;
	unsigned short int date;
	unsigned int size;
	unsigned int csize;
} ENTRY;


enum { SINGLE_COLUMN, MULTI_COLUMN, IN_A_LINE } format;
enum { VIS_ROW, VIS_QUESTION, VIS_OCTAL } visualize = VIS_ROW;

bool
	long_format = FALSE,
	in_a_line = FALSE,
	recurse = FALSE,
	directory = FALSE,
	fast = FALSE,
	reverse = FALSE,
	print_size = FALSE,
	along_row = FALSE,
	mark_dir = FALSE,
	mark_exe = FALSE,
	print_hidden = FALSE,
	print_reldir = FALSE,
	case_independent = FALSE;


int width;

bool first = TRUE;

int namecmp(p1, p2)
ENTRY *p1, *p2;
{
	return case_independent ? stricmp(p1->name, p2->name) : strcmp(p1->name, p2->name);
}

int timecmp(p1, p2)
ENTRY *p1, *p2;
{
	int retval;

	retval = ((p2->date << 16) | p2->time) - ((p1->date << 16) | p1->time);
	return (retval == 0) ? namecmp(p1, p2) : retval;
}

int (*cmp_func)() = namecmp;


ENTRY **root = NULL;
int number_of_entry = 0;
int number_of_subdir = 0;


void nospace()
{
	fprintf(stderr, "ls: Insufficient memory.\n");
	exit(1);
}

int is_reldir(name)
char *name;
{
	if (name[0] != '.') return 0;
	if (name[1] == '\0') return 1;
	if (name[1] != '.') return 0;
	return ((name[2] == '\0') ? 2 : 0);
}

int cmp(p1, p2)
ENTRY *p1, *p2;
{
	int result = (*cmp_func)(p1, p2);
	return (reverse ? -result : result);
}

void sort(base, num)
ENTRY *base[];
int num;
{
	if (num > 1) {
		ENTRY **tail;
		ENTRY *ref;
		ENTRY **p1, **p2;

		p2 = tail = (p1 = base) + num - 1;  /* &base[num - 1] */
		ref = base[num / 2];
		do {
			while (cmp(*p1, ref) < 0) p1++;
			while (cmp(*p2, ref) > 0) p2--;
			if (p1 <= p2) {
				ENTRY *tmp = *p1;
				*p1++ = *p2;
				*p2-- = tmp;
			}
		} while (p1 <= p2);

		sort(base, p2 - base + 1);
		sort(p1, tail - p1 + 1);
	}
}

char *linebuf, *bufp;

void putline()
{
	bufp = stpcpy(bufp, "\r\n");
	WRITE(1, linebuf, bufp - linebuf);
	bufp = linebuf;
}

int print_name(s)
unsigned char *s;
{
	int retval = 0;

	while (*s)
		if (iskanji(s[0]) && iskanji2(s[1])) {
			sprintf(bufp, "%c%c", s[0], s[1]);
			s += 2;
			bufp = strtail(bufp);
			retval += 2;
			continue;
		}
		else {
			unsigned char ch = *s++;

			if (isgraph(ch)) {
		put1ch:
				*bufp++ = ch;
				++retval;
			}
			else {
				switch (visualize)
				{
				case VIS_QUESTION:
					ch = '?';
				default:
					goto put1ch;
				case VIS_OCTAL:
					sprintf(bufp, "\\%03o", ch);
					bufp = strtail(bufp);
					retval += 5;
					break;
				}
			}
		}

	return retval;
}

int tailcmp(s1, s2)
char *s1, *s2;
{
	int l1, l2;

	if ((l1 = strlen(s1)) < (l2 = strlen(s2))) return -1;
	return stricmp((s1 + l1 - l2), s2);
}

int print_entry(list)
ENTRY *list;
{
	int retval = print_name(list->name);

	if (mark_dir && (list->mode & FA_DIREC)) {
		*bufp++ = '/';
		++retval;
	}
	else if (mark_exe) {
		if (list->mode & 0x40) {
			*bufp++ = '@';
			++retval;
		}
		else if (( (list->mode & 0x80) ||
			(tailcmp(list->name, ".x") == 0) ||
			(tailcmp(list->name, ".r") == 0) ||
			(tailcmp(list->name, ".bat") == 0) ) ) {
			*bufp++ = '*';
			++retval;
		}
	}

	return retval;
}

void output(list, num)
ENTRY *list[];
int num;
{
	int i, j;


	if (long_format || (format == SINGLE_COLUMN)) {
		for (i = 0; i < num; i++) {
			if (print_size) {
				sprintf(bufp, "%4u ", list[i]->csize);
				bufp = strtail(bufp);
			}

			if (long_format) {
				unsigned short int mode, date, time;

				mode = list[i]->mode;
				date = list[i]->date;
				time = list[i]->time;

				if (mode & 0x40) *bufp++ = 'l';
				else if (mode & FA_LABEL) *bufp++ = 'v';
				else if (mode & FA_DIREC) *bufp++ = 'd';
				else *bufp++ = '-';

				sprintf(bufp, "%c%c%cr%c%c %10u ",
					((mode & FA_ARCH) ? 'a' : '-'),
					((mode & FA_SYSTEM) ? 's' : '-'),
					((mode & FA_HIDDEN) ? 'h' : '-'),
					((mode & FA_RDONLY) ? '-' : 'w'),
					((mode & 0x80) ? 'x' : '-'),
					list[i]->size
					);
				bufp = strtail(bufp);
				if (mode & FA_ROOTDIR)
					bufp = stpcpy(bufp, "                     ");
				else {
					sprintf(bufp, "%3s %2u %4u %02u:%02u:%02u ",
						montab[(date >> 5) & 0x0F],
						(date & 0x1F),
						((date >> 9) + 1980),
						((time >> 11) & 0x1F),
						((time >> 5) & 0x3F),
						((time & 0x1F) * 2)
						);
				}
				bufp = strtail(bufp);
			}

			print_entry(list[i]);
			putline();
		}
	}
	else if (format == IN_A_LINE) {
		int column = 0;

		for (i = 0; i < num; i++) {
			char sizes[8];
			int len;

			len = strlen(list[i]->name);
			if (print_size) {
				sprintf(sizes, "%u ", list[i]->csize);
				len += strlen(sizes);
			}

			if (column > 0) {
				bufp = stpcpy(bufp, ", ");
				column += 2;
			}
			if ((column + len + 2) >= width) {
				putline();
				column = 0;
			}
			if (print_size) bufp = stpcpy(bufp, sizes);
			print_entry(list[i]);
			column += len;
		}
		if (column > 0) putline();
	}
	else {	/* MULTI_COLUMN */
		int fieldsize, hnum, vnum, d, sc;

		for (fieldsize = 0, i = 0; i < num; i++) {
			int len;
			if ((len = strlen(list[i]->name)) > fieldsize) fieldsize = len;
		}
		if (print_size) fieldsize += 5;
		fieldsize += 2;

		hnum = (width - 1) / fieldsize;
		vnum = num / hnum;
		if (vnum * hnum < num) {
			++vnum;
			if (!along_row) {
				hnum = num / vnum;
				if (hnum * vnum < num) ++hnum;
			}
		}

		d = along_row ? 1 : vnum;

		for (sc = i = 0; i < vnum; i++) {
			if (!along_row) sc = i;
			for (j = 0; j < hnum; j++) {
				int col = 0;
				if (sc >= num) break;
				if (print_size) {
					sprintf(bufp, "%4u ", list[sc]->csize);
					bufp = strtail(bufp);
					col += 5;
				}
				col += print_entry(list[sc]);
				while (col < fieldsize) {
					*bufp++= ' ';
					++col;
				}
				sc += d;
			}
			putline();
		}
	}

	first = FALSE;
}

unsigned int size_of_cluster(pathname)
char *pathname;
{
	struct DPBPTR buf;

	GETDPB( ((pathname[1] == ':') ? (toupper(pathname[0]) - 'A' + 1) : 0), &buf);
	return ((buf.sec + 1) * buf.byte);
}

unsigned int cluster_size(bytesize, bytes_per_cluster)
unsigned int bytesize;
unsigned int bytes_per_cluster;
{
	unsigned int retval = bytesize / bytes_per_cluster;
	if ((retval * bytes_per_cluster) < bytesize) ++retval;
	return retval;
}

ENTRY *add_entry(name)
char *name;
{
	ENTRY *node;

	if ((root = xrealloc(root, (number_of_entry + 1) * sizeof(ENTRY *))) == NULL) nospace();
	if ((node = malloc(sizeof(ENTRY))) == NULL) nospace();
	if ((node->name = strdup(name)) == NULL) nospace();
	root[number_of_entry++] = node;
	return node;
}

void open_directory(dir)
char *dir;
{
	char pathname[MAXPATH], *p1, *p2;
	struct FILBUF dta;
	int code;
	unsigned int bytes_per_cluster;

	strcpy(pathname, dir);
	bytes_per_cluster = size_of_cluster(pathname);
	p1 = strtail(pathname);
	if (strchr(":/", *(p1 - 1)) == NULL) *p1++ = '/';
	strcpy(p1, "*.*");

	root = NULL;
	number_of_entry = 0;
	number_of_subdir = 0;

	for (code = FILES(&dta, pathname, FA_ALL); code == 0; code = NFILES(&dta)) {
		ENTRY *node;
		int isrel;

		if ((isrel = is_reldir(dta.name)) && !print_reldir) continue;
		if (!print_hidden) {
			if (dta.name[1] == '.') continue;
			if ((dta.atr & (FA_LABEL|FA_HIDDEN))) continue;
		}

		node = add_entry(dta.name);
		node->mode = dta.atr;
		if (!isrel && (dta.atr & FA_DIREC)) {
			node->mode |= FA_SUBDIR;
			++number_of_subdir;
		}
		node->time = dta.time;
		node->date = dta.date;
		node->csize = cluster_size((node->size = dta.filelen), bytes_per_cluster);
	}
}

void ls_subdir(char *, bool);

void ls_onedir(pathname)
char *pathname;
{
	ENTRY **save_root = root;
	int save_nent = number_of_entry;
	int save_ndir = number_of_subdir;
	int i;


	open_directory(pathname);

	if (print_size || long_format) {
		unsigned int total;

		for (total = 0, i = 0; i < number_of_entry; i++) total += root[i]->csize;
		sprintf(bufp, "total %u", total);
		bufp = strtail(bufp);
		putline();
	}

	if (number_of_entry) {
		if (!fast) sort(root, number_of_entry);
		output(root, number_of_entry);
		system("process");
		if (recurse) {
			char *p1;
			p1 = strtail(pathname);
			if (strchr(":/", *(p1 - 1)) == NULL) strcpy(p1, "/");
			ls_subdir(pathname, 1);
		}
		free(root);
	}
	root = save_root;
	number_of_entry = save_nent;
	number_of_subdir = save_ndir;

	first = FALSE;
}

void ls_subdir(rootdir, header)
char *rootdir;
bool header;
{
	ENTRY **sp;
	int i;

	for (sp = root, i = number_of_subdir; i; sp++)
		if ((*sp)->mode & FA_SUBDIR) {
			char pathname[MAXPATH];
			strcat(strcpy(pathname, rootdir), (*sp)->name);
			if (header) {
				if (!first) putline();
				print_name(pathname);
				*bufp++ = ':';
				putline();
			}
			ls_onedir(pathname);
			--i;
		}
}

void fix_file(pathname, dta)
char *pathname;
struct FILBUF *dta;
{
	ENTRY *node;

	node = add_entry(pathname);
	node->mode = dta->atr;
	if (fast || (dta->atr & FA_DIREC)) {
		node->mode |= FA_SUBDIR;
		++number_of_subdir;
	}
	node->date = dta->date;
	node->time = dta->time;
	node->csize = cluster_size((node->size = dta->filelen), size_of_cluster(pathname));
}

int
doname(name)
char *name;
{
	char pathname[MAXPATH], *pathp, *nextname, *fnamptr, *p;
	struct FILBUF dta;
	struct NAMECKBUF nameckbuf;
	int pseudo;
	int code;

	if (strpbrk(name, "*?")) return 0;
	pathp = pathname;
	if (name[1] == ':') {
		*pathp++ = toupper(*name);
		++name;
		*pathp++ = *name++;
	}
	if (*name == '/') *pathp++ = *name++;
	*pathp = '\0';
	if (*pathname && *name == '\0') {
		ENTRY *node;
add_root:
		node = add_entry(pathname);
		node->mode = FA_DIREC | FA_SUBDIR | FA_ROOTDIR;
		node->date = 0;
		node->time = 0;
		node->size = 0;
		node->csize = 0;
		++number_of_subdir;
		return 1;
	}
go:
	if (nextname = strchr(name, '/')) *nextname = '\0';
	fnamptr = pathp;
	pathp = stpcpy(pathp, name);
	pseudo = is_reldir(name);
	if (nextname) *nextname++ = '/';
	if (pseudo) {
		if (nextname) goto gogo;
		if (NAMECK(pathname, &nameckbuf) < 0 || nameckbuf.name[0]) pseudo = 0;
		if (nameckbuf.path[1] == '\0') goto add_root;
	}
	if (pseudo) {
		strip_excessive_slashes(nameckbuf.drive);
		if (FILES(&dta, nameckbuf.drive, FA_ALL) < 0) return 0;
	}
	else {
		if (FILES(&dta, pathname, FA_ALL) < 0) return 0;
		pathp = stpcpy(fnamptr, dta.name);
	}
	if (nextname) {
	gogo:
		*pathp++ = '/';
		*pathp = '\0';
		name = nextname;
		goto go;
	}

	fix_file(pathname, &dta);
	return 1;
}

void main(argc, argv[])
int argc;
char *argv[];
{
	int i;
	char *p;


	width = (p = getenv("COLUMNS")) ? atoi(p) : WIDTH;
	if ((linebuf = malloc(width + 3)) == NULL) nospace();
	bufp = linebuf;

	format = isatty(1) ? MULTI_COLUMN :  SINGLE_COLUMN;

	for (i = 1; (i < argc) && (*argv[i] == '-'); i++) {
		if (argv[i][1] == '\0') {
			++i;
			break;
		}
		for (p = argv[i]; *++p; ) {
			switch (*p)
			{
			case 'l':
				long_format = TRUE;
				break;
			case '1':
				format = SINGLE_COLUMN;
				break;
			case 'x':
				along_row = TRUE;
			case 'C':
				format = MULTI_COLUMN;
				break;
			case 'm':
				format = IN_A_LINE;
				break;
			case 'I':
				case_independent = TRUE;
				break;
			case 'R':
				recurse = TRUE;
				break;
			case 'f':
				directory = FALSE;
				fast = TRUE;
			case 'a':
				print_reldir = TRUE;
			case 'A':
				print_hidden = TRUE;
				break;
			case 'd':
				if (!fast) directory = TRUE;
				break;
			case 'F':
				mark_exe = TRUE;
			case 'p':
				mark_dir = TRUE;
				break;
			case 'q':
				visualize = VIS_QUESTION;
				break;
			case 'b':
				visualize = VIS_OCTAL;
				break;
			case 'r':
				reverse = TRUE;
				break;
			case 's':
				print_size = TRUE;
				break;
			case 't':
				cmp_func = timecmp;
				break;
			default:
				fprintf(stderr, "使用法: ls [-1ACFILRUabdflmpqrstx] [<ファイル>] ...\n");
				exit(1);
			}
		}
	}

	if (i == argc) {
		argv = def_argv;
		argc = 1;
		i = 0;
	}

	for (; i < argc; i++) {
		char *dp;

		for (dp = argv[i]; *dp; dp++) if (*dp == '\\') *dp = '/';
		strip_excessive_slashes(argv[i]);
		if (!doname(argv[i])) {
			sprintf(bufp, "%s not found", argv[i]);
			bufp = strtail(bufp);
			putline();
		}
	}

	sort(root, number_of_entry);

	if (directory) output(root, number_of_entry);	/*  Output all arguments  */
	else {
		int number_of_files = number_of_entry - number_of_subdir;

		if (number_of_files) {
			/*  Output file arguments  */

			ENTRY **filelist;
			ENTRY **sp;
			int i;

			/*  Exstract file arguments  */
			if ((filelist = malloc(number_of_files * sizeof(ENTRY *))) == NULL) nospace();
			for (sp = root, i = 0; i < number_of_files; sp++)
				if (!((*sp)->mode & FA_SUBDIR)) filelist[i++] = *sp;
			/*  Output file arguments  */
			output(filelist, number_of_files);
			free(filelist);
		}

		/*  Output entries in directory arguments  */
		ls_subdir("", (number_of_subdir > 1) || number_of_files);
	}

	exit(0);
}

/* end */
