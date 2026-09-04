/*
 * outguess-gui.c - interface grafica nativa Win32 para o OutGuess.
 *
 * Nao reimplementa nenhuma logica de esteganografia: apenas monta a
 * linha de comando e chama outguess.exe (que deve estar na mesma pasta
 * deste executavel), capturando a saida para exibir num log na tela.
 *
 * Compilado com MinGW-w64, estatico, sem dependencias externas alem
 * das DLLs que ja fazem parte de qualquer Windows (comctl32, comdlg32).
 */
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

/* ---- IDs dos controles ---- */
#define ID_RADIO_EMBED   100
#define ID_RADIO_EXTRACT 101
#define ID_EDIT_IMGIN    110
#define ID_BTN_IMGIN     111
#define ID_EDIT_MSGFILE  120
#define ID_BTN_MSGFILE   121
#define ID_LBL_MSGFILE   122
#define ID_EDIT_IMGOUT   130
#define ID_BTN_IMGOUT    131
#define ID_LBL_IMGOUT    132
#define ID_EDIT_KEY      140
#define ID_CHK_ECC       150
#define ID_CHK_SHOWKEY   151
#define ID_BTN_RUN       160
#define ID_EDIT_LOG      170
#define ID_BTN_CLEAR     171
#define ID_BTN_OPEN      172
#define ID_BTN_CAPACITY  173
#define ID_BTN_COPY_LOG  174
#define ID_CHK_NO_OVERWRITE 152
#define ID_EDIT_QUALITY  180
#define ID_EDIT_ITER     181
#define ID_LBL_QUALITY   182
#define ID_LBL_ITER      183
#define ID_STATUS        184
#define ID_PROGRESS      185
#define WM_RUN_COMPLETE  (WM_APP + 1)
#define WM_LOG_APPEND    (WM_APP + 2)

static HWND hMain, hImgIn, hMsgFile, hLblMsgFile, hImgOut, hLblImgOut,
			hBtnImgIn, hBtnMsgFile, hBtnImgOut, hKey, hEcc, hShowKey,
			hLog, hRadioEmbed, hRadioExtract, hLblQuality, hLblIter,
			hQuality, hIter, hStatus, hProgress, hBtnOpen, hBtnCapacity,
			hBtnCopyLog, hNoOverwrite;
static HFONT hFont;
static unsigned long long lastPayloadBytes;
static int operationRunning;

static void AddHistory(HWND control, const char *value)
{
	if (!value[0])
		return;
	if (SendMessageA(control, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)value) == CB_ERR)
		SendMessageA(control, CB_ADDSTRING, 0, (LPARAM)value);
}

static void CopyLogToClipboard(void)
{
	int length = GetWindowTextLengthA(hLog);
	HGLOBAL memory;
	char *text;
	if (length <= 0 || !OpenClipboard(hMain))
		return;
	memory = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)length + 1);
	if (!memory) {
		CloseClipboard();
		return;
	}
	text = (char *)GlobalLock(memory);
	GetWindowTextA(hLog, text, length + 1);
	GlobalUnlock(memory);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, memory);
	CloseClipboard();
}

static void RememberCurrentPaths(void)
{
	char value[MAX_PATH];
	GetWindowTextA(hImgIn, value, sizeof(value));
	AddHistory(hImgIn, value);
	GetWindowTextA(hMsgFile, value, sizeof(value));
	AddHistory(hMsgFile, value);
	GetWindowTextA(hImgOut, value, sizeof(value));
	AddHistory(hImgOut, value);
}

static void QueueLog(const char *text)
{
	size_t length = strlen(text);
	char *copy = (char *)malloc(length + 1);
	if (!copy)
		return;
	memcpy(copy, text, length + 1);
	if (!PostMessageA(hMain, WM_LOG_APPEND, 0, (LPARAM)copy))
		free(copy);
}

static void QueueLogLine(const char *text)
{
	char line[4096];
	snprintf(line, sizeof(line), "%s\r\n", text);
	QueueLog(line);
}

typedef struct {
	char args[4096];
	char tempMessageFile[MAX_PATH];
	int embedding;
} RunContext;

/* Modo atual: 1 = embutir, 0 = extrair */
static int ModoEmbutir(void)
{
	return SendMessage(hRadioEmbed, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static void AppendLog(const char *txt)
{
	int len = GetWindowTextLength(hLog);
	SendMessage(hLog, EM_SETSEL, len, len);
	SendMessage(hLog, EM_REPLACESEL, FALSE, (LPARAM)txt);
	SendMessage(hLog, EM_SCROLLCARET, 0, 0);
}

static void AppendLogLine(const char *txt)
{
	AppendLog(txt);
	AppendLog("\r\n");
}

/* Monta o caminho de outguess.exe: mesma pasta deste .exe */
static void GetOutguessPath(char *buf, DWORD bufsize)
{
	char self[MAX_PATH];
	GetModuleFileNameA(NULL, self, MAX_PATH);
	char *slash = strrchr(self, '\\');
	if (slash)
		*(slash + 1) = '\0';
	else
		self[0] = '\0';
	snprintf(buf, bufsize, "%soutguess.exe", self);
}

static int BrowseOpen(HWND owner, char *buf, DWORD bufsize, const char *filter, const char *title)
{
	OPENFILENAMEA ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = owner;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = bufsize;
	ofn.lpstrFilter = filter;
	ofn.lpstrTitle = title;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	return GetOpenFileNameA(&ofn);
}

static int BrowseSave(HWND owner, char *buf, DWORD bufsize, const char *filter, const char *title, const char *defExt)
{
	OPENFILENAMEA ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = owner;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = bufsize;
	ofn.lpstrFilter = filter;
	ofn.lpstrTitle = title;
	ofn.lpstrDefExt = defExt;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	return GetSaveFileNameA(&ofn);
}

/* Roda outguess.exe com os argumentos dados, capturando stdout+stderr
 * e jogando no log. Retorna o exit code (ou -1 se falhou ao iniciar). */
static int RunOutguess(const char *args)
{
	char exePath[MAX_PATH];
	GetOutguessPath(exePath, MAX_PATH);

	if (GetFileAttributesA(exePath) == INVALID_FILE_ATTRIBUTES) {
		char message[MAX_PATH + 128];
		snprintf(message, sizeof(message),
			"outguess.exe nao foi encontrado em:\r\n%s\r\n\r\n"
			"Coloque outguess-gui.exe na mesma pasta que outguess.exe.",
			exePath);
		QueueLogLine(message);
		return -1;
	}

	char cmdline[4096];
	snprintf(cmdline, sizeof(cmdline), "\"%s\" %s", exePath, args);

	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&sa, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE hReadPipe, hWritePipe;
	if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
		QueueLogLine("[erro] falha ao criar pipe de saida");
		return -1;
	}
	SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOA si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = hWritePipe;
	si.hStdError = hWritePipe;
	si.hStdInput = NULL;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	QueueLogLine("---");
	QueueLogLine(cmdline);

	BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
	                          CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
	CloseHandle(hWritePipe);

	if (!ok) {
		CloseHandle(hReadPipe);
		QueueLogLine("[erro] nao foi possivel iniciar outguess.exe");
		return -1;
	}

	char buf[4096];
	DWORD n;
	while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &n, NULL) && n > 0) {
		buf[n] = '\0';
		QueueLog(buf);
	}
	CloseHandle(hReadPipe);

	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (int)exitCode;
}

static void QuoteInto(char *dst, size_t dstsize, const char *src)
{
	snprintf(dst, dstsize, "\"%s\"", src);
}

static void GetProcessPath(HWND control, char *path, size_t pathsize)
{
	wchar_t widePath[MAX_PATH];
	wchar_t shortPath[MAX_PATH];
	GetWindowTextW(control, widePath, sizeof(widePath) / sizeof(widePath[0]));
	DWORD shortLength = GetShortPathNameW(widePath, shortPath,
		sizeof(shortPath) / sizeof(shortPath[0]));
	const wchar_t *source = shortLength > 0 && shortLength < sizeof(shortPath) / sizeof(shortPath[0])
		? shortPath : widePath;
	int converted = WideCharToMultiByte(CP_ACP, 0, source, -1, path,
		(int)pathsize, NULL, NULL);
	if (converted == 0)
		path[0] = '\0';
}

static int GetUtf8ControlText(HWND control, char *text, size_t textsize)
{
	wchar_t wideText[1024];
	GetWindowTextW(control, wideText, sizeof(wideText) / sizeof(wideText[0]));
	return WideCharToMultiByte(CP_UTF8, 0, wideText, -1, text,
		(int)textsize, NULL, NULL) != 0;
}

static int ValidateOutputPath(const char *input, const char *output)
{
	char parent[MAX_PATH];
	DWORD attributes;
	if (_stricmp(input, output) == 0) {
		MessageBoxA(hMain, "O arquivo de saida deve ser diferente do arquivo de entrada.",
			"Destino invalido", MB_ICONWARNING);
		return 0;
	}
	attributes = GetFileAttributesA(output);
	if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		MessageBoxA(hMain, "O destino indicado e uma pasta, nao um arquivo.",
			"Destino invalido", MB_ICONWARNING);
		return 0;
	}
	strncpy(parent, output, sizeof(parent) - 1);
	parent[sizeof(parent) - 1] = '\0';
	char *slash = strrchr(parent, '\\');
	if (!slash)
		slash = strrchr(parent, '/');
	if (slash)
		*slash = '\0';
	else
		strcpy(parent, ".");
	attributes = GetFileAttributesA(parent);
	if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		MessageBoxA(hMain, "A pasta de destino nao existe.", "Destino invalido", MB_ICONWARNING);
		return 0;
	}
	attributes = GetFileAttributesA(output);
	if (attributes != INVALID_FILE_ATTRIBUTES) {
		if (MessageBoxA(hMain, "O arquivo de destino ja existe. Deseja substitui-lo?",
			"Confirmar substituicao", MB_YESNO | MB_ICONQUESTION) != IDYES)
			return 0;
	}
	return 1;
}

static int CreateTempMessageFile(const wchar_t *message, char *path, DWORD pathsize)
{
	char tempDir[MAX_PATH];
	char *utf8Message;
	DWORD tempDirLength = GetTempPathA(sizeof(tempDir), tempDir);
	if (tempDirLength == 0 || tempDirLength >= sizeof(tempDir))
		return 0;
	if (GetTempFileNameA(tempDir, "ogs", 0, path) == 0)
		return 0;

	int utf8Length = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
	if (utf8Length <= 0) {
		DeleteFileA(path);
		return 0;
	}
	utf8Message = (char *)malloc((size_t)utf8Length);
	if (!utf8Message || WideCharToMultiByte(CP_UTF8, 0, message, -1,
	    utf8Message, utf8Length, NULL, NULL) == 0) {
		free(utf8Message);
		DeleteFileA(path);
		return 0;
	}

	FILE *file = fopen(path, "wb");
	if (!file) {
		free(utf8Message);
		DeleteFileA(path);
		return 0;
	}

	size_t messageLength = (size_t)utf8Length - 1;
	int written = fwrite(utf8Message, 1, messageLength, file) == messageLength;
	free(utf8Message);
	if (fclose(file) != 0)
		written = 0;
	if (!written) {
		DeleteFileA(path);
		return 0;
	}
	return 1;
}

static DWORD WINAPI RunThread(LPVOID parameter)
{
	RunContext *context = (RunContext *)parameter;
	int result = RunOutguess(context->args);
	if (context->tempMessageFile[0] != '\0')
		DeleteFileA(context->tempMessageFile);
	PostMessageA(hMain, WM_RUN_COMPLETE, (WPARAM)result, (LPARAM)context->embedding);
	free(context);
	return 0;
}

static void OpenContainingFolder(const char *path)
{
	char folder[MAX_PATH];
	strncpy(folder, path, sizeof(folder) - 1);
	folder[sizeof(folder) - 1] = '\0';
	char *slash = strrchr(folder, '\\');
	if (!slash)
		slash = strrchr(folder, '/');
	if (slash)
		*slash = '\0';
	else
		strcpy(folder, ".");
	ShellExecuteA(hMain, "open", folder, NULL, NULL, SW_SHOWNORMAL);
}

static void RunCapacity(void)
{
	char image[MAX_PATH], quality[16], tool[MAX_PATH], command[2048];
	GetProcessPath(hImgIn, image, sizeof(image));
	GetWindowTextA(hQuality, quality, sizeof(quality));
	if (!image[0] || GetFileAttributesA(image) == INVALID_FILE_ATTRIBUTES) {
		MessageBoxA(hMain, "Escolha uma imagem JPEG valida primeiro.", "Capacidade", MB_ICONWARNING);
		return;
	}
	GetOutguessPath(tool, sizeof(tool));
	char *slash = strrchr(tool, '\\');
	if (slash)
		strcpy(slash + 1, "capacity.exe");
	char quotedImage[MAX_PATH + 2];
	QuoteInto(quotedImage, sizeof(quotedImage), image);
	snprintf(command, sizeof(command), "-e -p %s %s", quality, quotedImage);
	char fullCommand[4096];
	snprintf(fullCommand, sizeof(fullCommand), "\"%s\" %s", tool, command);
	AppendLogLine("--- capacidade ---");
	AppendLogLine(fullCommand);
	STARTUPINFOA startup;
	PROCESS_INFORMATION process;
	SECURITY_ATTRIBUTES security;
	HANDLE readPipe, writePipe;
	ZeroMemory(&startup, sizeof(startup));
	startup.cb = sizeof(startup);
	ZeroMemory(&process, sizeof(process));
	ZeroMemory(&security, sizeof(security));
	security.nLength = sizeof(security);
	security.bInheritHandle = TRUE;
	if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
		AppendLogLine("[erro] nao foi possivel capturar a capacidade.");
		return;
	}
	SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
	startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_HIDE;
	startup.hStdOutput = writePipe;
	startup.hStdError = writePipe;
	if (!CreateProcessA(NULL, fullCommand, NULL, NULL, TRUE, CREATE_NO_WINDOW,
		NULL, NULL, &startup, &process)) {
		CloseHandle(readPipe);
		CloseHandle(writePipe);
		AppendLogLine("[erro] capacity.exe nao foi encontrado.");
		return;
	}
	CloseHandle(writePipe);
	char output[4096];
	DWORD bytesRead;
	while (ReadFile(readPipe, output, sizeof(output) - 1, &bytesRead, NULL) && bytesRead > 0) {
		output[bytesRead] = '\0';
		AppendLog(output);
	}
	CloseHandle(readPipe);
	WaitForSingleObject(process.hProcess, INFINITE);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
}

static void OnRun(void)
{
	char imgin[MAX_PATH], msgfile[MAX_PATH], imgout[MAX_PATH], key[2048];
	char quality[16], iterations[16];
	wchar_t messageText[2048];
	GetProcessPath(hImgIn, imgin, sizeof(imgin));
	GetProcessPath(hMsgFile, msgfile, sizeof(msgfile));
	GetWindowTextW(hMsgFile, messageText, sizeof(messageText) / sizeof(messageText[0]));
	GetProcessPath(hImgOut, imgout, sizeof(imgout));
	if (!GetUtf8ControlText(hKey, key, sizeof(key))) {
		MessageBoxA(hMain, "Nao foi possivel ler a chave informada.", "Erro", MB_ICONERROR);
		return;
	}
	GetWindowTextA(hQuality, quality, sizeof(quality));
	GetWindowTextA(hIter, iterations, sizeof(iterations));

	if (imgin[0] == '\0') {
		MessageBoxA(hMain, "Escolha a imagem JPEG de entrada.", "Campo faltando", MB_ICONWARNING);
		return;
	}
	if (GetFileAttributesA(imgin) == INVALID_FILE_ATTRIBUTES) {
		MessageBoxA(hMain, "A imagem de entrada nao foi encontrada.", "Arquivo invalido", MB_ICONWARNING);
		return;
	}
	int jpegQuality = atoi(quality);
	int iterationLimit = atoi(iterations);
	if (ModoEmbutir() && (jpegQuality < 75 || jpegQuality > 100)) {
		MessageBoxA(hMain, "A qualidade JPEG deve estar entre 75 e 100.", "Valor invalido", MB_ICONWARNING);
		return;
	}
	if (ModoEmbutir() && (iterationLimit < 0 || iterationLimit > 65535)) {
		MessageBoxA(hMain, "O limite de iteracoes deve estar entre 0 e 65535.", "Valor invalido", MB_ICONWARNING);
		return;
	}
	int ecc = SendMessage(hEcc, BM_GETCHECK, 0, 0) == BST_CHECKED;
	char q_imgin[MAX_PATH + 2], q_msgfile[MAX_PATH + 2], q_imgout[MAX_PATH + 2], q_key[2050], keyArg[2070];
	QuoteInto(q_imgin, sizeof(q_imgin), imgin);
	if (key[0] != '\0') {
		QuoteInto(q_key, sizeof(q_key), key);
		snprintf(keyArg, sizeof(keyArg), "-k %s ", q_key);
	} else {
		strcpy(keyArg, "");
	}

	RunContext *context = (RunContext *)calloc(1, sizeof(RunContext));
	if (!context) {
		MessageBoxA(hMain, "Nao foi possivel iniciar a operacao.", "Erro", MB_ICONERROR);
		return;
	}
	context->embedding = ModoEmbutir();

	if (ModoEmbutir()) {
		if (msgfile[0] == '\0') {
			MessageBoxA(hMain, "Digite uma mensagem ou escolha um arquivo.", "Campo faltando", MB_ICONWARNING);
			free(context);
			return;
		}
		if (imgout[0] == '\0') {
			MessageBoxA(hMain, "Escolha onde salvar a imagem de saida.", "Campo faltando", MB_ICONWARNING);
			free(context);
			return;
		}
		if (SendMessageA(hNoOverwrite, BM_GETCHECK, 0, 0) == BST_CHECKED &&
			GetFileAttributesA(imgout) != INVALID_FILE_ATTRIBUTES) {
			MessageBoxA(hMain, "O arquivo de saida ja existe e a opcao nao sobrescrever esta ativa.",
				"Destino protegido", MB_ICONWARNING);
			free(context);
			return;
		}
		if (!ValidateOutputPath(imgin, imgout)) {
			free(context);
			return;
		}
		if (MessageBoxA(hMain, "Iniciar a insercao da mensagem na imagem?",
			"Confirmar operacao", MB_YESNO | MB_ICONQUESTION) != IDYES) {
			free(context);
			return;
		}
		RememberCurrentPaths();
		DWORD messageAttributes = GetFileAttributesA(msgfile);
		if (messageAttributes == INVALID_FILE_ATTRIBUTES ||
		    (messageAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			if (!CreateTempMessageFile(messageText, context->tempMessageFile,
			    sizeof(context->tempMessageFile))) {
				MessageBoxA(hMain, "Nao foi possivel preparar a mensagem.", "Erro", MB_ICONERROR);
				free(context);
				return;
			}
			QuoteInto(q_msgfile, sizeof(q_msgfile), context->tempMessageFile);
		} else {
			QuoteInto(q_msgfile, sizeof(q_msgfile), msgfile);
		}
		QuoteInto(q_imgout, sizeof(q_imgout), imgout);
		snprintf(context->args, sizeof(context->args), "%s%s-p %s -d %s %s %s",
			keyArg, ecc ? "-e " : "", quality, q_msgfile, q_imgin, q_imgout);
		if (iterations[0]) {
			char suffix[64];
			snprintf(suffix, sizeof(suffix), " -i %s", iterations);
			strncat(context->args, suffix, sizeof(context->args) - strlen(context->args) - 1);
		}
	} else {
		if (msgfile[0] == '\0') {
			MessageBoxA(hMain, "Escolha onde salvar a mensagem extraida.", "Campo faltando", MB_ICONWARNING);
			free(context);
			return;
		}
		if (SendMessageA(hNoOverwrite, BM_GETCHECK, 0, 0) == BST_CHECKED &&
			GetFileAttributesA(msgfile) != INVALID_FILE_ATTRIBUTES) {
			MessageBoxA(hMain, "O arquivo extraido ja existe e a opcao nao sobrescrever esta ativa.",
				"Destino protegido", MB_ICONWARNING);
			free(context);
			return;
		}
		if (!ValidateOutputPath(imgin, msgfile)) {
			free(context);
			return;
		}
		if (MessageBoxA(hMain, "Iniciar a extracao da mensagem?",
			"Confirmar operacao", MB_YESNO | MB_ICONQUESTION) != IDYES) {
			free(context);
			return;
		}
		RememberCurrentPaths();
		QuoteInto(q_msgfile, sizeof(q_msgfile), msgfile);
		snprintf(context->args, sizeof(context->args), "%s%s-r %s %s",
			keyArg, ecc ? "-e " : "", q_imgin, q_msgfile);
	}

	lastPayloadBytes = 0;
	if (context->embedding) {
		DWORD attributes = GetFileAttributesA(msgfile);
		if (attributes != INVALID_FILE_ATTRIBUTES &&
		    !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
			HANDLE file = CreateFileA(msgfile, GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			LARGE_INTEGER size;
			if (file != INVALID_HANDLE_VALUE && GetFileSizeEx(file, &size))
				lastPayloadBytes = (unsigned long long)size.QuadPart;
			if (file != INVALID_HANDLE_VALUE)
				CloseHandle(file);
		} else {
			int utf8Length = WideCharToMultiByte(CP_UTF8, 0, messageText, -1,
				NULL, 0, NULL, NULL);
			if (utf8Length > 0)
				lastPayloadBytes = (unsigned long long)utf8Length - 1;
		}
	}
	char status[128];
	if (context->embedding)
		snprintf(status, sizeof(status), "Payload: %llu bytes | capacidade no log", lastPayloadBytes);
	else
		strcpy(status, "Preparando extracao...");
	SetWindowTextA(hStatus, status);

	EnableWindow(GetDlgItem(hMain, ID_BTN_RUN), FALSE);
	EnableWindow(hBtnOpen, FALSE);
	operationRunning = 1;
	SendMessage(hProgress, PBM_SETMARQUEE, TRUE, 30);
	SetWindowTextA(hStatus, context->embedding ? "Embutindo mensagem..." : "Extraindo mensagem...");
	HANDLE thread = CreateThread(NULL, 0, RunThread, context, 0, NULL);
	if (!thread) {
		if (context->tempMessageFile[0] != '\0')
			DeleteFileA(context->tempMessageFile);
		free(context);
		SendMessage(hProgress, PBM_SETMARQUEE, FALSE, 0);
		EnableWindow(GetDlgItem(hMain, ID_BTN_RUN), TRUE);
		EnableWindow(hBtnOpen, TRUE);
		operationRunning = 0;
		SetWindowTextA(hStatus, "Pronto");
		MessageBoxA(hMain, "Nao foi possivel iniciar a operacao.", "Erro", MB_ICONERROR);
		return;
	}
	CloseHandle(thread);
}

static void UpdateModeVisibility(void)
{
	int embutir = ModoEmbutir();
	SetWindowTextA(hLblMsgFile, embutir ? "Mensagem ou arquivo a esconder:" : "Salvar mensagem extraida em:");
	ShowWindow(hLblImgOut, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hImgOut, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hBtnImgOut, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hLblQuality, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hQuality, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hLblIter, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hIter, embutir ? SW_SHOW : SW_HIDE);
	EnableWindow(hBtnOpen, FALSE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_CREATE: {
		hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

		#define MKSTATIC(txt, x, y, w, h) do { \
			HWND h_ = CreateWindowA("STATIC", txt, WS_CHILD | WS_VISIBLE, \
				x, y, w, h, hwnd, NULL, NULL, NULL); \
			SendMessage(h_, WM_SETFONT, (WPARAM)hFont, TRUE); \
		} while (0)

		MKSTATIC("Modo:", 12, 14, 60, 20);
		hRadioEmbed = CreateWindowA("BUTTON", "Embutir mensagem",
			WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
			80, 12, 160, 20, hwnd, (HMENU)ID_RADIO_EMBED, NULL, NULL);
		hRadioExtract = CreateWindowA("BUTTON", "Extrair mensagem",
			WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
			250, 12, 160, 20, hwnd, (HMENU)ID_RADIO_EXTRACT, NULL, NULL);
		SendMessage(hRadioEmbed, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hRadioExtract, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hRadioEmbed, BM_SETCHECK, BST_CHECKED, 0);

		MKSTATIC("Imagem JPEG de entrada:", 12, 50, 200, 20);
		hImgIn = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
			12, 72, 470, 24, hwnd, (HMENU)ID_EDIT_IMGIN, NULL, NULL);
		hBtnImgIn = CreateWindowA("BUTTON", "Procurar...", WS_CHILD | WS_VISIBLE,
			492, 72, 100, 24, hwnd, (HMENU)ID_BTN_IMGIN, NULL, NULL);
		SendMessage(hImgIn, WM_SETFONT, (WPARAM)hFont, TRUE);

		hLblMsgFile = CreateWindowA("STATIC", "Mensagem ou arquivo a esconder:", WS_CHILD | WS_VISIBLE,
			12, 108, 400, 20, hwnd, (HMENU)ID_LBL_MSGFILE, NULL, NULL);
		hMsgFile = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
			12, 130, 470, 24, hwnd, (HMENU)ID_EDIT_MSGFILE, NULL, NULL);
		hBtnMsgFile = CreateWindowA("BUTTON", "Procurar...", WS_CHILD | WS_VISIBLE,
			492, 130, 100, 24, hwnd, (HMENU)ID_BTN_MSGFILE, NULL, NULL);
		SendMessage(hLblMsgFile, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hMsgFile, WM_SETFONT, (WPARAM)hFont, TRUE);

		hLblImgOut = CreateWindowA("STATIC", "Imagem de saida (com a mensagem):", WS_CHILD | WS_VISIBLE,
			12, 166, 300, 20, hwnd, (HMENU)ID_LBL_IMGOUT, NULL, NULL);
		hImgOut = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
			12, 188, 470, 24, hwnd, (HMENU)ID_EDIT_IMGOUT, NULL, NULL);
		hBtnImgOut = CreateWindowA("BUTTON", "Procurar...", WS_CHILD | WS_VISIBLE,
			492, 188, 100, 24, hwnd, (HMENU)ID_BTN_IMGOUT, NULL, NULL);
		SendMessage(hLblImgOut, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hImgOut, WM_SETFONT, (WPARAM)hFont, TRUE);

		hLblQuality = CreateWindowA("STATIC", "Qualidade JPEG (75-100):", WS_CHILD | WS_VISIBLE,
			12, 224, 150, 20, hwnd, (HMENU)ID_LBL_QUALITY, NULL, NULL);
		hQuality = CreateWindowA("EDIT", "75", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
			165, 222, 60, 24, hwnd, (HMENU)ID_EDIT_QUALITY, NULL, NULL);
		hLblIter = CreateWindowA("STATIC", "Limite de iteracoes:", WS_CHILD | WS_VISIBLE,
			245, 224, 130, 20, hwnd, (HMENU)ID_LBL_ITER, NULL, NULL);
		hIter = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
			378, 222, 75, 24, hwnd, (HMENU)ID_EDIT_ITER, NULL, NULL);

		MKSTATIC("Chave secreta (opcional):", 12, 260, 180, 20);
		hKey = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
			12, 282, 300, 24, hwnd, (HMENU)ID_EDIT_KEY, NULL, NULL);
		hShowKey = CreateWindowA("BUTTON", "Mostrar", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			322, 284, 90, 20, hwnd, (HMENU)ID_CHK_SHOWKEY, NULL, NULL);
		SendMessage(hKey, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hShowKey, WM_SETFONT, (WPARAM)hFont, TRUE);

		hEcc = CreateWindowA("BUTTON", "Usar correcao de erro (mais robusto)",
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			12, 322, 320, 20, hwnd, (HMENU)ID_CHK_ECC, NULL, NULL);
		SendMessage(hEcc, WM_SETFONT, (WPARAM)hFont, TRUE);
		hNoOverwrite = CreateWindowA("BUTTON", "Nao sobrescrever arquivos existentes",
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			12, 346, 300, 20, hwnd, (HMENU)ID_CHK_NO_OVERWRITE, NULL, NULL);
		SendMessage(hNoOverwrite, BM_SETCHECK, BST_CHECKED, 0);
		SendMessage(hNoOverwrite, WM_SETFONT, (WPARAM)hFont, TRUE);

		CreateWindowA("BUTTON", "Limpar log", WS_CHILD | WS_VISIBLE,
			12, 376, 100, 32, hwnd, (HMENU)ID_BTN_CLEAR, NULL, NULL);
		hBtnOpen = CreateWindowA("BUTTON", "Abrir pasta", WS_CHILD | WS_VISIBLE,
			125, 376, 110, 32, hwnd, (HMENU)ID_BTN_OPEN, NULL, NULL);
		EnableWindow(hBtnOpen, FALSE);
		hBtnCapacity = CreateWindowA("BUTTON", "Ver capacidade", WS_CHILD | WS_VISIBLE,
			245, 376, 160, 32, hwnd, (HMENU)ID_BTN_CAPACITY, NULL, NULL);
		hBtnCopyLog = CreateWindowA("BUTTON", "Copiar log", WS_CHILD | WS_VISIBLE,
			12, 414, 100, 28, hwnd, (HMENU)ID_BTN_COPY_LOG, NULL, NULL);
		SendMessage(hBtnCapacity, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hBtnCopyLog, WM_SETFONT, (WPARAM)hFont, TRUE);
		CreateWindowA("BUTTON", "Executar", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
			420, 376, 172, 32, hwnd, (HMENU)ID_BTN_RUN, NULL, NULL);
		SendMessage(GetDlgItem(hwnd, ID_BTN_RUN), WM_SETFONT, (WPARAM)hFont, TRUE);

		hStatus = CreateWindowA("STATIC", "Pronto", WS_CHILD | WS_VISIBLE,
			125, 418, 280, 20, hwnd, (HMENU)ID_STATUS, NULL, NULL);
		hProgress = CreateWindowA(PROGRESS_CLASSA, "", WS_CHILD | WS_VISIBLE | PBS_MARQUEE,
			420, 414, 172, 16, hwnd, (HMENU)ID_PROGRESS, NULL, NULL);
		SendMessage(hProgress, PBM_SETMARQUEE, FALSE, 0);

		MKSTATIC("Log de execucao", 12, 454, 150, 20);
		hLog = CreateWindowA("EDIT", "",
			WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
			ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			12, 478, 580, 180, hwnd, (HMENU)ID_EDIT_LOG, NULL, NULL);
		SendMessage(hLog, WM_SETFONT, (WPARAM)hFont, TRUE);

		AppendLogLine("OutGuess GUI pronta. Escolha o modo, preencha os campos e clique Executar.");
		UpdateModeVisibility();
		DragAcceptFiles(hwnd, TRUE);
		return 0;
	}

	case WM_COMMAND: {
		int id = LOWORD(wp);
		int code = HIWORD(wp);
		switch (id) {
		case ID_RADIO_EMBED:
		case ID_RADIO_EXTRACT:
			if (code == BN_CLICKED)
				UpdateModeVisibility();
			break;
		case ID_BTN_IMGIN: {
			char buf[MAX_PATH] = "";
			if (BrowseOpen(hwnd, buf, MAX_PATH,
			    "Imagens JPEG\0*.jpg;*.jpeg\0Todos os arquivos\0*.*\0",
			    "Escolha a imagem JPEG"))
				SetWindowTextA(hImgIn, buf);
			break;
		}
		case ID_BTN_MSGFILE: {
			char buf[MAX_PATH] = "";
			if (ModoEmbutir()) {
				if (BrowseOpen(hwnd, buf, MAX_PATH,
				    "Todos os arquivos\0*.*\0", "Escolha o arquivo de mensagem"))
					SetWindowTextA(hMsgFile, buf);
			} else {
				if (BrowseSave(hwnd, buf, MAX_PATH,
				    "Arquivo de texto\0*.txt\0Todos os arquivos\0*.*\0",
				    "Salvar mensagem extraida como", "txt"))
					SetWindowTextA(hMsgFile, buf);
			}
			break;
		}
		case ID_BTN_IMGOUT: {
			char buf[MAX_PATH] = "";
			if (BrowseSave(hwnd, buf, MAX_PATH,
			    "Imagem JPEG\0*.jpg\0Todos os arquivos\0*.*\0",
			    "Salvar imagem de saida como", "jpg"))
				SetWindowTextA(hImgOut, buf);
			break;
		}
		case ID_CHK_SHOWKEY: {
			int show = SendMessage(hShowKey, BM_GETCHECK, 0, 0) == BST_CHECKED;
			SendMessage(hKey, EM_SETPASSWORDCHAR, show ? 0 : '*', 0);
			InvalidateRect(hKey, NULL, TRUE);
			break;
		}
		case ID_BTN_RUN:
			OnRun();
			break;
		case ID_BTN_CLEAR:
			SetWindowTextA(hLog, "");
			break;
		case ID_BTN_OPEN: {
			char path[MAX_PATH];
			GetWindowTextA(ModoEmbutir() ? hImgOut : hMsgFile, path, sizeof(path));
			OpenContainingFolder(path);
			break;
		}
		case ID_BTN_CAPACITY:
			RunCapacity();
			break;
		case ID_BTN_COPY_LOG:
			CopyLogToClipboard();
			SetWindowTextA(hStatus, "Log copiado");
			break;
		}
		return 0;
	}

	case WM_LOG_APPEND:
		AppendLog((const char *)lp);
		free((void *)lp);
		return 0;

	case WM_RUN_COMPLETE: {
		int result = (int)wp;
		int embedding = (int)lp;
		EnableWindow(GetDlgItem(hMain, ID_BTN_RUN), TRUE);
		operationRunning = 0;
		SendMessage(hProgress, PBM_SETMARQUEE, FALSE, 0);
		if (result == 0) {
			EnableWindow(hBtnOpen, TRUE);
			char status[128];
			snprintf(status, sizeof(status), "Concluido | payload: %llu bytes", lastPayloadBytes);
			SetWindowTextA(hStatus, status);
			AppendLogLine(embedding
				? "[ok] mensagem embutida com sucesso."
				: "[ok] mensagem extraida com sucesso.");
			MessageBoxA(hMain, embedding ? "Mensagem embutida com sucesso!" :
				"Mensagem extraida com sucesso!", "OutGuess", MB_ICONINFORMATION);
		} else {
			SetWindowTextA(hStatus, "Falha na operacao");
			AppendLogLine("[falhou] outguess.exe retornou erro. Veja o log acima.");
			MessageBoxA(hMain, "A operacao falhou. Veja o log para detalhes.",
				"OutGuess", MB_ICONERROR);
		}
		return 0;
	}

	case WM_DROPFILES: {
		HDROP drop = (HDROP)wp;
		char path[MAX_PATH];
		if (DragQueryFileA(drop, 0, path, sizeof(path))) {
			const char *extension = strrchr(path, '.');
			int isJpeg = extension && (_stricmp(extension, ".jpg") == 0 ||
				_stricmp(extension, ".jpeg") == 0);
			if (isJpeg || !ModoEmbutir())
				SetWindowTextA(hImgIn, path);
			else
				SetWindowTextA(hMsgFile, path);
		}
		DragFinish(drop);
		return 0;
	}

	case WM_CLOSE:
		if (operationRunning) {
			MessageBoxA(hwnd, "Aguarde a operacao terminar antes de fechar a janela.",
				"Operacao em andamento", MB_ICONINFORMATION);
			return 0;
		}
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int nShow)
{
	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	WNDCLASSA wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = "OutGuessGuiWndClass";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassA(&wc);

	hMain = CreateWindowA("OutGuessGuiWndClass", "OutGuess - Esteganografia em JPEG",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 620, 704,
		NULL, NULL, hInst, NULL);

	ShowWindow(hMain, nShow);
	UpdateWindow(hMain);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
