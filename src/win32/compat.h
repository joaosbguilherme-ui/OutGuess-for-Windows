/*
 * compat.h - camada de compatibilidade para portar o OutGuess (originalmente
 * escrito para Unix/BSD) para Windows nativo, compilado com MinGW-w64.
 *
 * Este arquivo soh eh incluido quando _WIN32 esta definido; em Linux/macOS
 * o build continua usando exatamente os cabecalhos POSIX originais.
 *
 * Substitui, para o alvo Windows:
 *   - <unistd.h> / getopt()      -> implementacao propria de getopt()
 *   - <netinet/in.h> htonl/ntohl -> macros de byte-swap sem depender de
 *                                    winsock (evita precisar de -lws2_32)
 *   - <sys/mman.h>               -> nao usado (HAVE_MMAP fica indefinido,
 *                                    o proprio outguess.c cai no fallback
 *                                    via read())
 *   - u_char/u_int/u_short       -> tipos BSD que o MSVCRT nao define
 *   - O_BINARY                   -> necessario para abrir JPEG/PNM sem
 *                                    tradução de CRLF
 */
#ifndef OUTGUESS_WIN32_COMPAT_H
#define OUTGUESS_WIN32_COMPAT_H

#ifdef _WIN32

#include <io.h>
#include <string.h>

/* ---- Tipos estilo BSD usados por todo o codigo original ---- */
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;

/* ---- u_intNN_t (sys/types.h do BSD/glibc, ausentes no MSVCRT) ---- */
#include <stdint.h>
typedef uint8_t  u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

/* ---- strcasecmp / strncasecmp (o MSVCRT so tem os prefixados com _) ---- */
#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

/* ---- htonl/ntohl sem precisar linkar Winsock ---- */
#ifndef htonl
static __inline unsigned long outguess_htonl(unsigned long x)
{
	return ((x & 0x000000ffUL) << 24) |
	       ((x & 0x0000ff00UL) << 8)  |
	       ((x & 0x00ff0000UL) >> 8)  |
	       ((x & 0xff000000UL) >> 24);
}
#define htonl(x) outguess_htonl(x)
#define ntohl(x) outguess_htonl(x)
#endif

/* ---- abertura de arquivo binaria: evita traducao de CRLF do CRT ---- */
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif

/* ---- getopt() minimalista (MinGW-w64 nao inclui getopt por padrao) ---- */
extern char *optarg;
extern int   optind, opterr, optopt;
int outguess_getopt(int argc, char * const argv[], const char *optstring);
#define getopt outguess_getopt

#endif /* _WIN32 */

#endif /* OUTGUESS_WIN32_COMPAT_H */
