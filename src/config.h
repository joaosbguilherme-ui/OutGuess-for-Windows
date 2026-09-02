/*
 * config.h - versao minima para build nativo Windows (MinGW-w64)
 *
 * No autoconf original, este arquivo eh gerado pelo script `configure`.
 * Para o build nativo Windows nao usamos autoconf (o mmap()/AC_FUNC_MMAP
 * do autoconf nao existem no MSVCRT), entao fornecemos manualmente os
 * poucos defines que outguess.c e md5.c realmente consultam.
 */
#ifndef OUTGUESS_CONFIG_H
#define OUTGUESS_CONFIG_H

/* MinGW-w64 (msvcrt/ucrt) tem snprintf() em conformidade com C99. */
#define HAVE_SNPRINTF 1

/* Windows nao possui mmap()/munmap() POSIX: outguess.c cai automaticamente
 * no fallback via read() quando HAVE_MMAP nao esta definido. */
/* #undef HAVE_MMAP */

/* Todo arquivo .c do projeto inclui "config.h" bem cedo, entao eh aqui que
 * puxamos a camada de compatibilidade Win32 (tipos u_char/u_int/u_intNN_t,
 * getopt, htonl sem winsock, O_BINARY etc.) para todos eles de uma vez. */
#ifdef _WIN32
#include "win32/compat.h"
#endif

#endif /* OUTGUESS_CONFIG_H */
