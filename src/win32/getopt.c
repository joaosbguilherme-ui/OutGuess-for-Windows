/*
 * getopt.c - implementacao minimalista de getopt(3) para o build Windows
 * do OutGuess (MinGW-w64 nao traz getopt() na sua libc por padrao).
 *
 * Suporta apenas o que o outguess.c realmente usa: opcoes de letra unica,
 * com ou sem argumento obrigatorio (":" apos a letra na optstring).
 * Nao implementa "--" nem opcoes longas, que o outguess.c nao usa.
 */
#include <stdio.h>
#include <string.h>

char *optarg = NULL;
int   optind = 1;
int   opterr = 1;
int   optopt = 0;

static char *nextchar = NULL;

int
outguess_getopt(int argc, char * const argv[], const char *optstring)
{
	if (nextchar == NULL || *nextchar == '\0') {
		if (optind >= argc)
			return -1;

		if (argv[optind][0] != '-' || argv[optind][1] == '\0')
			return -1;

		if (strcmp(argv[optind], "--") == 0) {
			optind++;
			return -1;
		}

		nextchar = &argv[optind][1];
		optind++;
	}

	optopt = *nextchar;
	nextchar++;

	{
		const char *p = strchr(optstring, optopt);

		if (p == NULL || optopt == ':') {
			if (opterr)
				fprintf(stderr, "%s: opcao invalida -- '%c'\n",
					argv[0], optopt);
			return '?';
		}

		if (p[1] == ':') {
			if (*nextchar != '\0') {
				optarg = nextchar;
				nextchar = NULL;
			} else if (optind < argc) {
				optarg = argv[optind];
				optind++;
			} else {
				if (opterr)
					fprintf(stderr,
					    "%s: a opcao -- '%c' requer um argumento\n",
					    argv[0], optopt);
				return '?';
			}
		}
	}

	return optopt;
}
