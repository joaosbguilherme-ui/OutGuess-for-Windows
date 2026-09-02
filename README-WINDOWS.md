# OutGuess — build nativo para Windows 11

Este projeto adapta o código-fonte do **OutGuess** para compilação e execução
nativas em Windows 11 (x86-64), sem exigir WSL, Cygwin ou o runtime do MSYS2
na máquina que executará os binários.

> **Importante:** o ambiente MSYS2 é usado para compilar, não é requisito para
> executar os binários distribuídos quando eles estiverem devidamente
> linkados conforme o build do projeto.

## Objetivo da adaptação

A camada de portabilidade foi mantida separada da lógica principal. A intenção
é preservar o comportamento do OutGuess e alterar somente o necessário para
o ambiente Windows.

Entre os pontos de compatibilidade estão:

| Área | Ambiente Unix/BSD | Adaptação Windows |
| --- | --- | --- |
| Leitura de arquivo | `mmap()` quando disponível | Fallback baseado em `read()` quando `HAVE_MMAP` não está definido |
| Argumentos | `getopt()` de `<unistd.h>` | Implementação compatível em `src/win32/getopt.c` |
| Byte order | `htonl()` / `ntohl()` | Implementação em `src/win32/compat.h` |
| Tipos BSD | Tipos de `<sys/types.h>` | Definições compatíveis baseadas em `<stdint.h>` |
| Arquivos binários | APIs Unix | Uso de `O_BINARY` para evitar tradução de dados pelo CRT |
| `strcasecmp()` | glibc/BSD | Mapeamento para `_stricmp()` no MSVCRT |
| JPEG | Configuração Unix | Configuração Windows da biblioteca JPEG |

A adaptação específica está concentrada principalmente em:

- `src/win32/compat.h`
- `src/win32/getopt.c`
- `src/config.h`

## Binários prontos

Os binários distribuídos ficam em `bin-win64/`.

Quando a release fornecer executáveis prontos, o usuário final poderá
simplesmente baixar o pacote da página de Releases e executar os binários
compatíveis, sem instalar o ambiente de desenvolvimento.

O executável da GUI precisa permanecer na mesma pasta que `outguess.exe`:

```text
bin-win64/
├── outguess.exe
└── outguess-gui.exe
```

> A disponibilidade e o conteúdo dos binários dependem da release publicada.
> Consulte o arquivo da release correspondente antes de distribuir ou
> automatizar o download.

## Requisitos para compilação

### Windows + MSYS2

1. Instale o [MSYS2](https://www.msys2.org/).
2. Abra **MSYS2 MinGW 64-bit**.
3. Atualize o ambiente:

```bash
pacman -Syu
```

Se o MSYS2 solicitar o fechamento do terminal, feche-o, abra novamente
**MSYS2 MinGW 64-bit** e continue:

```bash
pacman -Su
pacman -S --needed mingw-w64-x86_64-gcc make
```

### Compilar

A partir da raiz do projeto:

```bash
cd /c/caminho/OutGuess-for-Windows/src
make -f Makefile.mingw
```

Os artefatos gerados dependem das regras presentes no
`Makefile.mingw`. Na configuração documentada do projeto, são gerados:

- `outguess.exe`
- `histogram.exe`
- `outguess-gui.exe`

## Executar a GUI

No PowerShell, entre no diretório onde os executáveis foram gerados:

```powershell
cd "C:\caminho\OutGuess-for-Windows\src"
Start-Process ".\outguess-gui.exe"
```

Mantenha `outguess.exe` no mesmo diretório da GUI.

## Uso pela linha de comando

O fluxo básico permanece compatível com a sintaxe tradicional do OutGuess.

### Inserção

```powershell
.\outguess.exe -k "minha-chave" -d ".\mensagem.txt" ".\imagem.jpg" ".\saida.jpg"
```

### Extração

```powershell
.\outguess.exe -k "minha-chave" -r ".\saida.jpg" ".\mensagem-recuperada.txt"
```

### Ajuda

```powershell
.\outguess.exe -h
```

Para a referência completa das opções, consulte
[`man/outguess.txt`](man/outguess.txt).

## Cross-compilação

Em Debian/Ubuntu, instale o toolchain:

```bash
sudo apt install mingw-w64 make
```

Depois:

```bash
cd src

make -f Makefile.mingw   CC=x86_64-w64-mingw32-gcc   AR=x86_64-w64-mingw32-ar   RANLIB=x86_64-w64-mingw32-ranlib
```

O resultado depende das regras do `Makefile.mingw`.

## Testes

Os testes de integração podem ser executados no ambiente POSIX:

```bash
make check
```

Um teste importante para validar o fluxo de esteganografia é o
**round-trip**:

```text
arquivo original
      ↓
    embed
      ↓
   imagem
      ↓
   extract
      ↓
arquivo recuperado
```

O arquivo recuperado deve ser comparado byte a byte com o original.

## Solução de problemas

### `make: command not found`

Você provavelmente está no terminal MSYS2 errado. Abra
**MSYS2 MinGW 64-bit** e instale:

```bash
pacman -S --needed mingw-w64-x86_64-gcc make
```

### `gcc: command not found`

Verifique se o pacote MinGW-w64 foi instalado e se o terminal utilizado é
**MSYS2 MinGW 64-bit**, não o terminal MSYS comum.

### A GUI não inicia

Confirme que os dois executáveis estão juntos:

```text
outguess.exe
outguess-gui.exe
```

Também tente executar diretamente no PowerShell:

```powershell
.\outguess-gui.exe
```

### A imagem JPEG apresenta comportamento inesperado

Não abra/salve novamente a imagem em um editor que recomprima o JPEG entre
a inserção e a extração. Preserve o arquivo de saída produzido pelo
OutGuess.

## Estrutura relevante

```text
OutGuess-for-Windows/
├── bin-win64/
├── man/
├── src/
│   ├── jpeg-6b-steg/
│   └── win32/
├── tests/
├── README.md
├── README-WINDOWS.md
├── LICENSE
└── CONTRIBUTING.md
```

## Licença

BSD, conforme o projeto original. Consulte `LICENSE` e os cabeçalhos dos
arquivos-fonte.
O OutGuess original foi desenvolvido por Niels Provos e posteriormente
mantido pela comunidade de código aberto.
