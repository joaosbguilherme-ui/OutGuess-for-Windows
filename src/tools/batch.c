#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void usage(const char *program)
{
	fprintf(stderr, "Uso:\n");
	fprintf(stderr, "  %s embed [-e] [-p qualidade] [-f] [-k chave] -m mensagem -i padrao -o diretorio\n", program);
	fprintf(stderr, "  %s extract [-e] [-f] [-k chave] -i padrao -o diretorio\n", program);
	fprintf(stderr, "  -f sobrescreve arquivos de saida existentes (padrao: ignora)\n");
}

static int get_option(int argc, char **argv, const char *name, char **value)
{
	int i;
	for (i = 2; i + 1 < argc; i++) {
		if (strcmp(argv[i], name) == 0) {
			*value = argv[i + 1];
			return 1;
		}
	}
	return 0;
}

static int has_option(int argc, char **argv, const char *name)
{
	int i;
	for (i = 2; i < argc; i++)
		if (strcmp(argv[i], name) == 0)
			return 1;
	return 0;
}

static int ensure_directory(const char *path)
{
	char parent[MAX_PATH];
	DWORD attributes = GetFileAttributesA(path);
	if (attributes != INVALID_FILE_ATTRIBUTES)
		return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	strncpy(parent, path, sizeof(parent) - 1);
	parent[sizeof(parent) - 1] = '\0';
	char *slash = strrchr(parent, '\\');
	if (!slash)
		slash = strrchr(parent, '/');
	if (slash && slash != parent && !(slash == parent + 2 && parent[1] == ':')) {
		*slash = '\0';
		if (!ensure_directory(parent))
			return 0;
	}
	return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void quote(char *target, size_t size, const char *value)
{
	snprintf(target, size, "\"%s\"", value);
}

static void basename_from_path(const char *path, char *name, size_t size)
{
	const char *slash = strrchr(path, '\\');
	const char *filename = slash ? slash + 1 : path;
	strncpy(name, filename, size - 1);
	name[size - 1] = '\0';
}

static int process_file(const char *mode, const char *key, const char *message,
	const char *input, const char *outputDirectory, int errorCorrection,
	const char *quality, int force)
{
	char program[MAX_PATH], inputQuoted[MAX_PATH + 2], output[MAX_PATH], outputQuoted[MAX_PATH + 2];
	char keyQuoted[1024], keyArgument[1030], messageQuoted[MAX_PATH + 2];
	char command[4096], filename[MAX_PATH];
	char *slash;
	int pathLength;
	STARTUPINFOA startup;
	PROCESS_INFORMATION process;
	GetModuleFileNameA(NULL, program, sizeof(program));
	slash = strrchr(program, '\\');
	if (slash)
		strcpy(slash + 1, "outguess.exe");
	basename_from_path(input, filename, sizeof(filename));
	pathLength = snprintf(output, sizeof(output), "%s\\%s", outputDirectory, filename);
	if (pathLength < 0 || pathLength >= (int)sizeof(output)) {
		fprintf(stderr, "[falha] caminho de saida muito longo: %s\n", input);
		return 1;
	}
	if (strcmp(mode, "extract") == 0) {
		char *extension = strrchr(output, '.');
		if (extension)
			strcpy(extension, ".txt");
		else
			strcat(output, ".txt");
	}
	if (message && strcmp(input, message) == 0) {
		fprintf(stderr, "[falha] mensagem nao pode ser a imagem: %s\n", input);
		return 1;
	}
	if (_stricmp(input, output) == 0) {
		fprintf(stderr, "[falha] saida nao pode ser a mesma imagem: %s\n", input);
		return 1;
	}
	if (!force && GetFileAttributesA(output) != INVALID_FILE_ATTRIBUTES) {
		printf("[ignorado] saida ja existe: %s\n", output);
		return 0;
	}
	quote(inputQuoted, sizeof(inputQuoted), input);
	quote(outputQuoted, sizeof(outputQuoted), output);
	if (key)
		quote(keyQuoted, sizeof(keyQuoted), key);
	if (key)
		snprintf(keyArgument, sizeof(keyArgument), "-k %s ", keyQuoted);
	else
		keyArgument[0] = '\0';
	if (strcmp(mode, "embed") == 0) {
		quote(messageQuoted, sizeof(messageQuoted), message);
		if (quality) {
			snprintf(command, sizeof(command), "\"%s\" %s%s-p %s -d %s %s %s",
				program, keyArgument, errorCorrection ? "-e " : "", quality,
				messageQuoted, inputQuoted, outputQuoted);
		} else {
			snprintf(command, sizeof(command), "\"%s\" %s%s-d %s %s %s",
				program, keyArgument, errorCorrection ? "-e " : "",
				messageQuoted, inputQuoted, outputQuoted);
		}
	} else {
		snprintf(command, sizeof(command), "\"%s\" %s%s-r %s %s",
			program, keyArgument, errorCorrection ? "-e " : "", inputQuoted,
			outputQuoted);
	}
	printf("[%s] %s\nComando: %s\n", mode, input, command);
	ZeroMemory(&startup, sizeof(startup));
	startup.cb = sizeof(startup);
	ZeroMemory(&process, sizeof(process));
	if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
		NULL, NULL, &startup, &process)) {
		fprintf(stderr, "Nao foi possivel iniciar outguess.exe (erro %lu).\n",
			(unsigned long)GetLastError());
		return 1;
	}
	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD exitCode = 1;
	GetExitCodeProcess(process.hProcess, &exitCode);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return (int)exitCode;
}

int main(int argc, char **argv)
{
	WIN32_FIND_DATAA data;
	HANDLE search;
	char *key = NULL, *message = NULL, *pattern = NULL, *output = NULL, *quality = NULL;
	char directory[MAX_PATH], searchPattern[MAX_PATH], input[MAX_PATH];
	char *slash;
	int processed = 0, failed = 0;
	int errorCorrection = has_option(argc, argv, "-e");
	int force = has_option(argc, argv, "-f");
	if (argc < 2 || (strcmp(argv[1], "embed") != 0 && strcmp(argv[1], "extract") != 0) ||
		!get_option(argc, argv, "-i", &pattern) ||
		!get_option(argc, argv, "-o", &output) ||
		(has_option(argc, argv, "-p") && !get_option(argc, argv, "-p", &quality)) ||
		(strcmp(argv[1], "embed") == 0 && !get_option(argc, argv, "-m", &message))) {
		usage(argv[0]);
		return 2;
	}
	if (quality) {
		char *end;
		long value;
		errno = 0;
		value = strtol(quality, &end, 10);
		if (errno || *quality == '\0' || *end != '\0' || value < 75 || value > 100) {
			fprintf(stderr, "A qualidade JPEG deve estar entre 75 e 100.\n");
			return 2;
		}
	}
	if (message && GetFileAttributesA(message) == INVALID_FILE_ATTRIBUTES) {
		fprintf(stderr, "Mensagem nao encontrada: %s\n", message);
		return 1;
	}
	if (!ensure_directory(output)) {
		fprintf(stderr, "Nao foi possivel criar a pasta de saida: %s\n", output);
		return 1;
	}
	strncpy(searchPattern, pattern, sizeof(searchPattern) - 1);
	searchPattern[sizeof(searchPattern) - 1] = '\0';
	slash = strrchr(searchPattern, '\\');
	if (!slash)
		slash = strrchr(searchPattern, '/');
	if (slash) {
		*slash = '\0';
		strncpy(directory, searchPattern, sizeof(directory) - 1);
		directory[sizeof(directory) - 1] = '\0';
		strcpy(searchPattern, slash + 1);
	} else {
		strcpy(directory, ".");
	}
	if (snprintf(input, sizeof(input), "%s\\%s", directory, searchPattern) >= (int)sizeof(input)) {
		fprintf(stderr, "Padrao de entrada muito longo: %s\n", pattern);
		return 2;
	}
	search = FindFirstFileA(input, &data);
	if (search == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Nenhum arquivo encontrado para: %s\n", pattern);
		return 1;
	}
	do {
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			if (snprintf(input, sizeof(input), "%s\\%s", directory, data.cFileName) >= (int)sizeof(input)) {
				fprintf(stderr, "[ignorado] caminho muito longo: %s\n", data.cFileName);
				continue;
			}
			processed++;
			const char *extension = strrchr(input, '.');
			if (!extension || (_stricmp(extension, ".jpg") != 0 && _stricmp(extension, ".jpeg") != 0)) {
				printf("[ignorado] nao e JPEG: %s\n", input);
				continue;
			}
			if (process_file(argv[1], key, message, input, output,
				errorCorrection, quality, force) != 0)
				failed++;
		}
	} while (FindNextFileA(search, &data));
	FindClose(search);
	printf("Processados: %d | Sucessos: %d | Falhas: %d\n",
		processed, processed - failed, failed);
	return failed ? 1 : 0;
}
