#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned short read16(FILE *file)
{
	int high = fgetc(file);
	int low = fgetc(file);
	return high < 0 || low < 0 ? 0 : (unsigned short)((high << 8) | low);
}

static int is_sof(int marker)
{
	return (marker >= 0xc0 && marker <= 0xc3) || (marker >= 0xc5 && marker <= 0xc7) ||
		(marker >= 0xc9 && marker <= 0xcb) || (marker >= 0xcd && marker <= 0xcf);
}

static int contains_iptc(const unsigned char *data, size_t length)
{
	size_t position;
	for (position = 0; position + 6 <= length; position++) {
		if (memcmp(data + position, "8BIM", 4) == 0 &&
			data[position + 4] == 0x04 && data[position + 5] == 0x04)
			return 1;
	}
	return 0;
}

static void usage(const char *program)
{
	fprintf(stderr, "Uso: %s imagem.jpg\n", program);
}

int main(int argc, char **argv)
{
	FILE *file;
	int first, second, marker;
	int width = 0, height = 0, components = 0, precision = 0;
	int jfif = 0, exif = 0, iptc = 0, xmp = 0;
	unsigned long long theoreticalBits = 0;

	if (argc != 2) {
		usage(argv[0]);
		return 2;
	}
	file = fopen(argv[1], "rb");
	if (!file) {
		perror(argv[1]);
		return 1;
	}
	if (fgetc(file) != 0xff || fgetc(file) != 0xd8) {
		fprintf(stderr, "Arquivo invalido: assinatura JPEG ausente.\n");
		fclose(file);
		return 1;
	}
	while ((first = fgetc(file)) != EOF) {
		if (first != 0xff)
			continue;
		do second = fgetc(file); while (second == 0xff);
		if (second == EOF)
			break;
		marker = second;
		if (marker == 0xd9 || marker == 0xda)
			break;
		if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7))
			continue;
		unsigned short length = read16(file);
		if (length < 2)
			break;
		long payload = (long)length - 2;
		if (marker == 0xe0 && payload >= 5) {
			char id[6] = {0};
			fread(id, 1, 5, file);
			payload -= 5;
			if (memcmp(id, "JFIF\0", 5) == 0)
				jfif = 1;
		} else if (marker == 0xe1 && payload >= 6) {
			char *app1 = (char *)malloc((size_t)payload);
			if (!app1 || fread(app1, 1, (size_t)payload, file) != (size_t)payload) {
				free(app1);
				fclose(file);
				return 1;
			}
			if (payload >= 6 && memcmp(app1, "Exif\0\0", 6) == 0)
				exif = 1;
			if (payload >= 29 && memcmp(app1, "http://ns.adobe.com/xap/1.0/", 29) == 0)
				xmp = 1;
			free(app1);
			payload = 0;
		} else if (marker == 0xed && payload > 0) {
			unsigned char *app13 = (unsigned char *)malloc((size_t)payload);
			if (!app13 || fread(app13, 1, (size_t)payload, file) != (size_t)payload) {
				free(app13);
				fclose(file);
				return 1;
			}
			if (payload >= 13 && memcmp(app13, "Photoshop 3.0", 13) == 0 &&
				contains_iptc(app13, (size_t)payload))
				iptc = 1;
			free(app13);
			payload = 0;
		} else if (is_sof(marker) && payload >= 6) {
			precision = fgetc(file);
			height = read16(file);
			width = read16(file);
			components = fgetc(file);
			payload -= 6;
		}
		while (payload-- > 0)
			fgetc(file);
	}
	fclose(file);
	if (!width || !height) {
		fprintf(stderr, "Nao foi encontrado um marcador SOF JPEG.\n");
		return 1;
	}
	theoreticalBits = (unsigned long long)width * height * (components ? components : 1);
	printf("Arquivo: %s\nDimensoes: %dx%d\nPrecisao: %d bits\nComponentes: %d\n",
		argv[1], width, height, precision, components);
	printf("Espaco de cor: %s\n", components == 1 ? "tons de cinza" :
		components == 3 ? "YCbCr/RGB" : components == 4 ? "CMYK/YCCK" : "desconhecido");
	printf("JFIF: %s\nEXIF: %s\nIPTC: %s\nXMP: %s\n",
		jfif ? "sim" : "nao", exif ? "sim" : "nao", iptc ? "sim" : "nao", xmp ? "sim" : "nao");
	printf("Capacidade teorica: %llu bits (%llu bytes)\n", theoreticalBits, theoreticalBits / 8);
	printf("Nota: a capacidade real depende dos coeficientes DCT e pode ser menor.\n");
	return 0;
}
