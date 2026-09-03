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
#include <stdio.h>
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

static HWND hMain, hImgIn, hMsgFile, hLblMsgFile, hImgOut, hLblImgOut,
            hBtnImgOut, hKey, hEcc, hShowKey, hLog, hRadioEmbed, hRadioExtract;
static HFONT hFont;

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
		char msg[MAX_PATH + 128];
		snprintf(msg, sizeof(msg),
			"outguess.exe nao foi encontrado em:\r\n%s\r\n\r\n"
			"Coloque outguess-gui.exe na mesma pasta que outguess.exe.",
			exePath);
		MessageBoxA(hMain, msg, "Erro", MB_ICONERROR);
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
		AppendLogLine("[erro] falha ao criar pipe de saida");
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

	AppendLogLine("---");
	AppendLogLine(cmdline);

	BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
	                          CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
	CloseHandle(hWritePipe);

	if (!ok) {
		CloseHandle(hReadPipe);
		AppendLogLine("[erro] nao foi possivel iniciar outguess.exe");
		return -1;
	}

	char buf[4096];
	DWORD n;
	while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &n, NULL) && n > 0) {
		buf[n] = '\0';
		AppendLog(buf);
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

static int CreateTempMessageFile(const char *message, char *path, DWORD pathsize)
{
	char tempDir[MAX_PATH];
	DWORD tempDirLength = GetTempPathA(sizeof(tempDir), tempDir);
	if (tempDirLength == 0 || tempDirLength >= sizeof(tempDir))
		return 0;
	if (GetTempFileNameA(tempDir, "ogs", 0, path) == 0)
		return 0;

	FILE *file = fopen(path, "wb");
	if (!file) {
		DeleteFileA(path);
		return 0;
	}

	size_t messageLength = strlen(message);
	int written = fwrite(message, 1, messageLength, file) == messageLength;
	if (fclose(file) != 0)
		written = 0;
	if (!written) {
		DeleteFileA(path);
		return 0;
	}
	return 1;
}

static void OnRun(void)
{
	char imgin[MAX_PATH], msgfile[MAX_PATH], imgout[MAX_PATH], key[512];
	char tempMessageFile[MAX_PATH] = "";
	GetWindowTextA(hImgIn, imgin, MAX_PATH);
	GetWindowTextA(hMsgFile, msgfile, MAX_PATH);
	GetWindowTextA(hImgOut, imgout, MAX_PATH);
	GetWindowTextA(hKey, key, sizeof(key));

	if (imgin[0] == '\0') {
		MessageBoxA(hMain, "Escolha a imagem JPEG de entrada.", "Campo faltando", MB_ICONWARNING);
		return;
	}
	if (key[0] == '\0') {
		MessageBoxA(hMain, "Digite uma chave.", "Campo faltando", MB_ICONWARNING);
		return;
	}

	int ecc = SendMessage(hEcc, BM_GETCHECK, 0, 0) == BST_CHECKED;
	char q_imgin[MAX_PATH + 2], q_msgfile[MAX_PATH + 2], q_imgout[MAX_PATH + 2], q_key[520];
	QuoteInto(q_imgin, sizeof(q_imgin), imgin);
	QuoteInto(q_key, sizeof(q_key), key);

	char args[4096];

	if (ModoEmbutir()) {
		if (msgfile[0] == '\0') {
			MessageBoxA(hMain, "Digite uma mensagem ou escolha um arquivo.", "Campo faltando", MB_ICONWARNING);
			return;
		}
		if (imgout[0] == '\0') {
			MessageBoxA(hMain, "Escolha onde salvar a imagem de saida.", "Campo faltando", MB_ICONWARNING);
			return;
		}
		DWORD messageAttributes = GetFileAttributesA(msgfile);
		if (messageAttributes == INVALID_FILE_ATTRIBUTES ||
		    (messageAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			if (!CreateTempMessageFile(msgfile, tempMessageFile, sizeof(tempMessageFile))) {
				MessageBoxA(hMain, "Nao foi possivel preparar a mensagem.", "Erro", MB_ICONERROR);
				return;
			}
			QuoteInto(q_msgfile, sizeof(q_msgfile), tempMessageFile);
		} else {
			QuoteInto(q_msgfile, sizeof(q_msgfile), msgfile);
		}
		QuoteInto(q_imgout, sizeof(q_imgout), imgout);
		snprintf(args, sizeof(args), "-k %s %s-d %s %s %s",
			q_key, ecc ? "-e " : "", q_msgfile, q_imgin, q_imgout);
	} else {
		if (msgfile[0] == '\0') {
			MessageBoxA(hMain, "Escolha onde salvar a mensagem extraida.", "Campo faltando", MB_ICONWARNING);
			return;
		}
		QuoteInto(q_msgfile, sizeof(q_msgfile), msgfile);
		snprintf(args, sizeof(args), "-k %s %s-r %s %s",
			q_key, ecc ? "-e " : "", q_imgin, q_msgfile);
	}

	EnableWindow(GetDlgItem(hMain, ID_BTN_RUN), FALSE);
	int rc = RunOutguess(args);
	EnableWindow(GetDlgItem(hMain, ID_BTN_RUN), TRUE);
	if (tempMessageFile[0] != '\0')
		DeleteFileA(tempMessageFile);

	if (rc == 0) {
		AppendLogLine(ModoEmbutir()
			? "[ok] mensagem embutida com sucesso."
			: "[ok] mensagem extraida com sucesso.");
		MessageBoxA(hMain,
			ModoEmbutir() ? "Mensagem embutida com sucesso!" : "Mensagem extraida com sucesso!",
			"OutGuess", MB_ICONINFORMATION);
	} else if (rc > 0) {
		AppendLogLine("[falhou] outguess.exe retornou erro. Veja o log acima.");
		MessageBoxA(hMain, "Algo deu errado. Veja o log para detalhes.", "OutGuess", MB_ICONERROR);
	}
}

static void UpdateModeVisibility(void)
{
	int embutir = ModoEmbutir();
	SetWindowTextA(hLblMsgFile, embutir ? "Mensagem a esconder:" : "Salvar mensagem extraida em:");
	ShowWindow(hLblImgOut, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hImgOut, embutir ? SW_SHOW : SW_HIDE);
	ShowWindow(hBtnImgOut, embutir ? SW_SHOW : SW_HIDE);
	SetWindowTextA(hImgIn, "");
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
		hImgIn = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
			12, 72, 470, 24, hwnd, (HMENU)ID_EDIT_IMGIN, NULL, NULL);
		CreateWindowA("BUTTON", "Procurar...", WS_CHILD | WS_VISIBLE,
			492, 72, 100, 24, hwnd, (HMENU)ID_BTN_IMGIN, NULL, NULL);
		SendMessage(hImgIn, WM_SETFONT, (WPARAM)hFont, TRUE);

		hLblMsgFile = CreateWindowA("STATIC", "Mensagem ou arquivo a esconder:", WS_CHILD | WS_VISIBLE,
			12, 108, 260, 20, hwnd, (HMENU)ID_LBL_MSGFILE, NULL, NULL);
		hMsgFile = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
			12, 130, 470, 24, hwnd, (HMENU)ID_EDIT_MSGFILE, NULL, NULL);
		CreateWindowA("BUTTON", "Procurar...", WS_CHILD | WS_VISIBLE,
			492, 130, 100, 24, hwnd, (HMENU)ID_BTN_MSGFILE, NULL, NULL);
		SendMessage(hLblMsgFile, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hMsgFile, WM_SETFONT, (WPARAM)hFont, TRUE);

		hLblImgOut = CreateWindowA("STATIC", "Imagem de saida (com a mensagem):", WS_CHILD | WS_VISIBLE,
			12, 166, 300, 20, hwnd, (HMENU)ID_LBL_IMGOUT, NULL, NULL);
		hImgOut = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
			12, 188, 470, 24, hwnd, (HMENU)ID_EDIT_IMGOUT, NULL, NULL);
		hBtnImgOut = CreateWindowA("BUTTON", "Procurar...", WS_CHILD | WS_VISIBLE,
			492, 188, 100, 24, hwnd, (HMENU)ID_BTN_IMGOUT, NULL, NULL);
		SendMessage(hLblImgOut, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hImgOut, WM_SETFONT, (WPARAM)hFont, TRUE);

		MKSTATIC("Chave secreta:", 12, 224, 120, 20);
		hKey = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
			12, 246, 300, 24, hwnd, (HMENU)ID_EDIT_KEY, NULL, NULL);
		hShowKey = CreateWindowA("BUTTON", "Mostrar", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			322, 248, 90, 20, hwnd, (HMENU)ID_CHK_SHOWKEY, NULL, NULL);
		SendMessage(hKey, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendMessage(hShowKey, WM_SETFONT, (WPARAM)hFont, TRUE);

		hEcc = CreateWindowA("BUTTON", "Usar correcao de erro (mais robusto)",
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			12, 280, 320, 20, hwnd, (HMENU)ID_CHK_ECC, NULL, NULL);
		SendMessage(hEcc, WM_SETFONT, (WPARAM)hFont, TRUE);

		CreateWindowA("BUTTON", "Executar", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
			420, 274, 172, 32, hwnd, (HMENU)ID_BTN_RUN, NULL, NULL);
		SendMessage(GetDlgItem(hwnd, ID_BTN_RUN), WM_SETFONT, (WPARAM)hFont, TRUE);

		MKSTATIC("Log:", 12, 318, 60, 20);
		hLog = CreateWindowA("EDIT", "",
			WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
			ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			12, 340, 580, 180, hwnd, (HMENU)ID_EDIT_LOG, NULL, NULL);
		SendMessage(hLog, WM_SETFONT, (WPARAM)hFont, TRUE);

		AppendLogLine("OutGuess GUI pronta. Escolha o modo, preencha os campos e clique Executar.");
		UpdateModeVisibility();
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
		}
		return 0;
	}

	case WM_CLOSE:
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
		CW_USEDEFAULT, CW_USEDEFAULT, 620, 570,
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
