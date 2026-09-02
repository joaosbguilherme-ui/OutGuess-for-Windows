# OutGuess for Windows

Port do **OutGuess** para Windows 64-bit, com suporte a linha de comando e
uma interface gráfica Win32 para os fluxos mais comuns de esteganografia.

> **Uso responsável:** utilize somente em arquivos próprios ou com autorização.
> Esteganografia não substitui criptografia; se o conteúdo for sensível,
> proteja-o adequadamente antes de ocultá-lo.

## Download

Baixe a versão mais recente na página de
[Releases](https://github.com/joaosbguilherme-ui/OutGuess-for-Windows/releases).

Veja as instruções completas para Windows em
[README-WINDOWS.md](README-WINDOWS.md).

## Recursos

- Ocultação e extração de arquivos em imagens JPEG.
- Suporte a imagens PNM e PPM conforme o OutGuess.
- Chave secreta para codificação do payload.
- Correção de erros Golay com a opção `-e`.
- Preservação das estatísticas de frequência em JPEG.
- Executáveis Windows 64-bit.
- Interface gráfica nativa Win32.
- Build com MinGW-w64.
- Compatibilidade da linha de comando com o uso tradicional do OutGuess.

## Uso rápido

Os binários prontos ficam em `bin-win64/`.

### Ocultar um arquivo

```powershell
cd "C:\caminho\OutGuess-for-Windows\bin-win64"

.\outguess.exe -k "minha-chave" -d ".\mensagem.txt" ".\imagem.jpg" ".\saida.jpg"
```

### Extrair um arquivo

```powershell
.\outguess.exe -k "minha-chave" -r ".\saida.jpg" ".\mensagem-recuperada.txt"
```

A chave utilizada na extração deve ser exatamente a mesma utilizada na
inserção. Os arquivos de entrada e os diretórios de destino precisam existir.

### Ver a ajuda

```powershell
.\outguess.exe -h
```

## Interface gráfica

Após compilar, mantenha `outguess-gui.exe` e `outguess.exe` na mesma pasta.

```powershell
Start-Process ".\outguess-gui.exe"
```

A GUI permite selecionar a imagem, o arquivo a ser ocultado, o destino, a
chave e a opção de correção de erros.

## Compilação

Para instruções completas de instalação, dependências, compilação e
solução de problemas no Windows, consulte
[README-WINDOWS.md](README-WINDOWS.md).

Resumo com MSYS2 MinGW64:

```bash
pacman -Syu
pacman -Su
pacman -S --needed mingw-w64-x86_64-gcc make

cd /c/caminho/OutGuess-for-Windows/src
make -f Makefile.mingw
```

O build gera os executáveis do projeto em `src/`, conforme a configuração
atual do `Makefile.mingw`.

## Estrutura

| Caminho | Descrição |
| --- | --- |
| `src/` | Código do OutGuess e arquivos de build |
| `src/jpeg-6b-steg/` | Biblioteca JPEG 6b modificada |
| `src/win32/` | Camada de compatibilidade Windows |
| `bin-win64/` | Binários e código-fonte da GUI |
| `tests/` | Scripts e arquivos de teste |
| `README-WINDOWS.md` | Guia detalhado para Windows |
| `man/outguess.txt` | Referência da linha de comando |

## Testes

Os testes de integração usam o ambiente de build do projeto:

```bash
make check
```

Para uma verificação básica de round-trip, o arquivo ocultado deve ser
recuperado sem alteração após `embed` e `extract`.

## Compatibilidade e portabilidade

A adaptação Windows concentra as mudanças específicas do sistema em
`src/win32/` e mantém o código principal do OutGuess separado da camada de
compatibilidade.

Consulte [README-WINDOWS.md](README-WINDOWS.md) para detalhes técnicos sobre
`getopt()`, byte order, tipos BSD, modo binário de arquivos e configuração
da biblioteca JPEG.

## Licença e créditos

O projeto utiliza licença BSD. Consulte [LICENSE](LICENSE) e os cabeçalhos
dos arquivos-fonte.

O OutGuess foi originalmente desenvolvido por **Niels Provos** e é mantido
pela comunidade [Resurrecting Open Source Projects](https://github.com/resurrecting-open-source-projects/outguess).

O projeto também incorpora componentes relacionados a JPEG 6b, Arc4, MD5 e
correção de erros Golay. Consulte [CONTRIBUTING.md](CONTRIBUTING.md) para
informações sobre contribuição.
