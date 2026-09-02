# OutGuess — build nativo para Windows 11

Este é o código-fonte original do OutGuess (Niels Provos, 1999-2001,
mantido atualmente por voluntários em
[resurrecting-open-source-projects/outguess](https://github.com/resurrecting-open-source-projects/outguess)),
**adaptado para compilar e rodar nativamente no Windows 11** (x86-64),
sem depender de WSL, Cygwin ou do runtime do MSYS2 no destino final.

O algoritmo em si (embedding enviesado que preserva o histograma DCT,
formato de payload, correção de erros Golay(23,12,7)) **não foi alterado
em nada** — só a camada de portabilidade do sistema operacional.

## O que foi adaptado

O código original assume um ambiente Unix/BSD. As mudanças, todas
isoladas atrás de `#ifdef _WIN32` (o build para Linux/macOS continua
funcionando exatamente como antes), foram:

| Área | Original (Unix) | Adaptação Windows |
|---|---|---|
| Leitura de arquivo | `mmap()` de `<sys/mman.h>` | O próprio código já tinha um fallback via `read()` quando `HAVE_MMAP` não está definido — só precisamos deixá-lo indefinido |
| Parsing de argumentos | `getopt()` de `<unistd.h>` | Implementação própria em `src/win32/getopt.c` (mesma assinatura) |
| Byte order | `htonl`/`ntohl` de `<netinet/in.h>` | Reimplementados em `src/win32/compat.h`, sem depender de Winsock |
| Tipos BSD | `u_char`, `u_int`, `u_int32_t` etc. de `<sys/types.h>` | Typedefs próprios baseados em `<stdint.h>` |
| Abertura de arquivo | `open(name, O_RDONLY)` | Adicionado `O_BINARY` — sem isso, o CRT do Windows faz tradução de CRLF e corrompe JPEGs binários |
| Config da libjpeg | `jconfig.cfg` genérico | Usa o `jconfig.vc` que já vinha no pacote (feito para Windows/MSVC, funciona igualmente bem com MinGW) |
| `strcasecmp` | glibc/BSD | Mapeado para `_stricmp` do MSVCRT |

Toda a lógica de adaptação está isolada em dois arquivos novos:
- `src/win32/compat.h`
- `src/win32/getopt.c`

Mais um `config.h` mínimo (`src/config.h`) que substitui o gerado
automaticamente pelo `autoconf` no build original (o `autoconf` original
faz `AC_FUNC_MMAP`, que não faz sentido para o alvo Windows).

## Binários prontos

Em `bin-win64/` estão `outguess.exe` e `histogram.exe`, compilados com
MinGW-w64, **linkados estaticamente** — dependem apenas de
`KERNEL32.dll` e `msvcrt.dll`, que fazem parte de qualquer instalação
do Windows (incluindo Windows 11). Não é necessário instalar nada além
de copiar o `.exe`.

O código-fonte da interface gráfica está em `bin-win64/outguess-gui.c`.
Depois de recompilar, `outguess-gui.exe` será gerado em `src/`; mantenha-o
na mesma pasta que `outguess.exe` para executá-lo.

Testado (via Wine, embed + extract round-trip com os arquivos de
exemplo em `tests/`): a mensagem extraída de um JPEG com esteganografia
embutida é bit a bit idêntica à mensagem original.

## Como recompilar

### No Windows, com MSYS2 (recomendado se quiser mexer no código)

1. Instale o [MSYS2](https://www.msys2.org/).
2. Abra o terminal **"MSYS2 MinGW64"** (não o MSYS normal).
3. `pacman -S mingw-w64-x86_64-gcc make`
4. `cd src && make -f Makefile.mingw`

Esse comando gera `outguess.exe`, `histogram.exe` e `outguess-gui.exe`.

### Cross-compilando a partir de Linux/macOS

```
sudo apt install mingw-w64      # Debian/Ubuntu
cd src
make -f Makefile.mingw CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar RANLIB=x86_64-w64-mingw32-ranlib
```

Isso gera `outguess.exe` e `histogram.exe` dentro de `src/`.

## Uso

Idêntico ao OutGuess original (veja `man/outguess.txt`):

```
outguess.exe -k "minha-chave" -d mensagem.txt imagem.jpg saida.jpg
outguess.exe -k "minha-chave" -r saida.jpg mensagem-recuperada.txt
```

## Licença

BSD, igual ao projeto original — ver `LICENSE` e o cabeçalho de cada
arquivo-fonte.
