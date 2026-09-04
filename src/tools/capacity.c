#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../config.h"
#include "../outguess.h"
#include "../pnm.h"
#include "../jpg.h"

int steg_foil;
int steg_foilfail;
int steg_stat;

void *checkedmalloc(size_t size)
{
	void *memory = malloc(size);
	if (!memory) {
		fprintf(stderr, "Memoria insuficiente.\n");
		exit(1);
	}
	return memory;
}

static void usage(const char *program)
{
	fprintf(stderr, "Uso: %s [-e] [-p qualidade] imagem.jpg\n", program);
	fprintf(stderr, "  -e              calcula capacidade com correcao de erros\n");
	fprintf(stderr, "  -p qualidade    qualidade JPEG entre 75 e 100 (padrao: 75)\n");
}

int main(int argc, char **argv)
{
	FILE *input;
	image *jpegImage;
	bitmap bitmap;
	int errorCorrection = 0;
	char *quality = NULL;
	int option;
	char *end;
	long qualityValue;

	while ((option = getopt(argc, argv, "ep:")) != -1) {
		if (option == 'e')
			errorCorrection = 1;
		else if (option == 'p')
			quality = optarg;
		else {
			usage(argv[0]);
			return 2;
		}
	}
	if (argc - optind != 1) {
		usage(argv[0]);
		return 2;
	}

	if (quality)
	{
		errno = 0;
		qualityValue = strtol(quality, &end, 10);
		if (errno || *quality == '\0' || *end != '\0' || qualityValue < 75 || qualityValue > 100) {
			fprintf(stderr, "A qualidade JPEG deve estar entre 75 e 100.\n");
			return 2;
		}
		init_JPEG_handler(quality);
	}
	else
		init_JPEG_handler(NULL);
	input = fopen(argv[optind], "rb");
	if (!input) {
		perror(argv[optind]);
		return 1;
	}
	jpegImage = read_JPEG_file(input);
	fclose(input);
	if (!jpegImage) {
		fprintf(stderr, "Nao foi possivel ler o JPEG.\n");
		return 1;
	}

	memset(&bitmap, 0, sizeof(bitmap));
	bitmap_from_jpg(&bitmap, jpegImage, 0);
	printf("Imagem: %s\n", argv[optind]);
	printf("Bits utilizaveis: %d\n", bitmap.bits);
	printf("Capacidade sem error correction: %d bytes\n", bitmap.bits / 8);
	int estimatedBits = preserve_jpg(&bitmap, -1);
	if (estimatedBits >= 0)
		printf("Capacidade com error correction: %d bytes\n", estimatedBits / 8);
	else
		printf("Capacidade com error correction: indisponivel\n");
	printf("Recomendacao: %s\n", errorCorrection
		? (estimatedBits >= 0 ? "Use -e; a mensagem deve caber na capacidade ECC." : "Evite -e nesta imagem.")
		: "Use -e para maior tolerancia a erros quando a capacidade permitir.");
	return 0;
}
