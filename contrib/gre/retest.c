#include	<stdio.h>
#include	<string.h>
#include	"re.h"

int suckline(char *, int);
	
main(int argc, char **argv)
{
	re_re *r;
	char *s;
	char text[4096], pat[4096];
	unsigned char map[256];
	FILE *exprs[1024];
	char *match[10][2];
	int n, i, bufc;
	extern re_debug;

	for(n = 0; n < 256; n++)
		map[n] = n;
	for(;;){
		if(suckline(text, sizeof text))
			break;
		if(suckline(pat, sizeof pat))
			break;
		printf("searching for '%s' in '%s'\n", pat, text);
		r = re_recomp(pat, pat+strlen(pat), map);
		if(r == 0)
			continue;
		if(re_reexec(r, text, text+strlen(text), match)){
			printf("matched:");
			for(i = 0; i < 10; i++)
				if(match[i][0]){
					printf(" G%d='", i);
					fwrite(match[i][0], 1, match[i][1]-match[i][0], stdout);
					putchar('\'');
				}
			putchar('\n');
		} else
			printf("no match\n");
		re_refree(r);
	}
	exit(0);
}

void
re_error(char *s)
{
	fprintf(stderr, "pattern error: %s\n", s);
	/* NOTREACHED */
}

char buf[4096];
char *bp = buf;
char *ep = buf;

int
get1(void)
{
	if(bp >= ep){
		int n;

		n = read(0, buf, sizeof buf);
		if(n <= 0)
			return(-1);
		bp = buf;
		ep = buf+n;
	}
	return(*(unsigned char *)bp++);
}

int
suckline(char *s, int n)
{
	int c;

	while(n-- > 0){
		if((c = get1()) < 0)
			return(1);
		if(c == '\n')
			break;
		*s++ = c;
	}
	*s = 0;
	return(0);
}
