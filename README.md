# OutGuess for Windows

Ferramenta de esteganografia para ocultar arquivos em imagens JPEG, PNM e
PPM. Esta versão inclui adaptação nativa para Windows 64-bit e uma interface
gráfica Win32 para os fluxos mais comuns.

> Use somente em arquivos próprios ou com autorização. Esteganografia não
> substitui criptografia: proteja o conteúdo com uma chave forte.

## Recursos

- Ocultação e extração de mensagens em imagens.
- Chave secreta para codificação do payload.
- Correção de erros Golay com a opção `-e`.
- Preservação de estatísticas de frequência em JPEG.
- Executáveis Windows compilados estaticamente.
- Interface gráfica nativa em `bin-win64/outguess-gui.c`.

## Uso rápido no Windows

Os executáveis prontos ficam em `bin-win64/`:

```powershell
cd "C:\caminho\OutGuess-Windows\bin-win64"
```

Para ocultar `mensagem.txt` em `imagem.jpg`:

```powershell
.\outguess.exe -k "minha-chave" -d ".\mensagem.txt" ".\imagem.jpg" ".\saida.jpg"
```

Para extrair a mensagem:

```powershell
.\outguess.exe -k "minha-chave" -r ".\saida.jpg" ".\mensagem-recuperada.txt"
```

Os arquivos de entrada precisam existir. A chave usada na extração deve ser
exatamente a mesma usada para ocultar a mensagem.

## Interface gráfica

Após compilar, mantenha `outguess-gui.exe` e `outguess.exe` na mesma pasta e
execute:

```powershell
Start-Process ".\outguess-gui.exe"
```

A interface permite selecionar a imagem, o arquivo da mensagem, o destino,
a chave e a opção de correção de erros.

## Compilar no Windows

### MSYS2 MinGW64

Instale o [MSYS2](https://www.msys2.org/) e abra **MSYS2 MinGW 64-bit**. No
terminal MSYS2, instale as ferramentas:

```bash
pacman -Syu
pacman -Su
pacman -S --needed mingw-w64-x86_64-gcc make
```

Se o primeiro comando pedir para fechar o terminal, feche-o, abra novamente
**MSYS2 MinGW 64-bit** e continue com os comandos seguintes.

Compile o projeto:

```bash
cd /c/Users/VAIO/Downloads/OutGuess-Windows/OutGuess-Windows/src
make -f Makefile.mingw
```

O build gera `outguess.exe`, `histogram.exe` e `outguess-gui.exe` em `src/`.

Para executar a GUI pelo PowerShell:

```powershell
Start-Process "C:\Users\VAIO\Downloads\OutGuess-Windows\OutGuess-Windows\src\outguess-gui.exe"
```

### Linux ou macOS

Para cross-compilar os binários Windows em Debian ou Ubuntu:

```bash
sudo apt install mingw-w64 make
cd src
make -f Makefile.mingw \
  CC=x86_64-w64-mingw32-gcc \
  AR=x86_64-w64-mingw32-ar \
  RANLIB=x86_64-w64-mingw32-ranlib
```

## Estrutura

| Caminho | Descrição |
| --- | --- |
| `src/` | Código do OutGuess e build MinGW |
| `src/jpeg-6b-steg/` | Biblioteca JPEG modificada usada pelo projeto |
| `src/win32/` | Compatibilidade Windows e `getopt()` |
| `bin-win64/` | Binários prontos e código-fonte da GUI |
| `tests/` | Scripts e arquivos de teste |
| `README-WINDOWS.md` | Notas detalhadas da adaptação Windows |
| `man/outguess.txt` | Referência completa da linha de comando |

## Testes

Os testes de integração são scripts shell e exigem um ambiente POSIX:

```bash
make check
```

Para consultar as opções disponíveis:

```text
outguess.exe -h
```

## Licença e créditos

O projeto usa licença BSD. Consulte [LICENSE](LICENSE) e os cabeçalhos dos
arquivos-fonte. OutGuess foi originalmente desenvolvido por Niels Provos e
é mantido pela comunidade [Resurrecting Open Source Projects](https://github.com/resurrecting-open-source-projects/outguess).

O projeto também incorpora a biblioteca JPEG 6b modificada, código Arc4,
MD5 e o código de correção de erros Golay. Consulte
[CONTRIBUTING.md](CONTRIBUTING.md) para contribuir.