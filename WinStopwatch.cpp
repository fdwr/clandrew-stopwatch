#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include "resource.h"

#ifndef SetWindowExStyle
#define SetWindowExStyle(hwnd, value)  ((DWORD)SetWindowLong(hwnd, GWL_EXSTYLE, value))
#endif

unsigned int g_accumulatedTickCount = 0; // Any previous accumulated time before pausing.
unsigned int g_startTickCount = 0; // Last start time (set to GetTickCount).
unsigned int g_timerInterval = 0;
constexpr unsigned int g_defaultTimerInterval = 1000;

HWND g_hDlg = nullptr;
HWND g_hTimeEdit = nullptr;
HWND g_hState = nullptr;
HWND g_hLogo = nullptr;
HFONT g_hTimeFont = nullptr;
bool g_isTimerActive = false;
bool g_isUpdatingTimeEdit = false;

INT_PTR CALLBACK DialogMessageHandler(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG), nullptr, DialogMessageHandler);
    return TRUE;
}

void TwoDigitItoa(unsigned int n, wchar_t buffer[2])
{
    n %= 100; // In case of overflow, just show last two digits.
    buffer[0] = '0' + n / 10;
    buffer[1] = '0' + n % 10;
}

template <typename T>
T ValueOrZero(T value, T minimum, T maximum)
{
    return (value >= minimum && value <= maximum) ? value : T(0);
}

unsigned int TwoDigitAtoi(wchar_t buffer[2])
{
    unsigned int first  = ValueOrZero(buffer[0] - '0', 0, 9);
    unsigned int second = ValueOrZero(buffer[1] - '0', 0, 9);
    return first * 10 + second;
}

void UpdateDisplayedText()
{
    g_isUpdatingTimeEdit = true;

    wchar_t buffer[9] = {'0','0',':','0','0',':','0','0','\0'};

    unsigned int totalTickCount = g_accumulatedTickCount;
    unsigned int seconds = totalTickCount / 1000;
    unsigned int minutes = seconds / 60;
    unsigned int hours   = minutes / 60;
    seconds %= 60;
    minutes %= 60;

    TwoDigitItoa(hours,   &buffer[0]);
    TwoDigitItoa(minutes, &buffer[3]);
    TwoDigitItoa(seconds, &buffer[6]);

    // Update text (without messing up the caret position, in case user is selecting it).
    DWORD caretStart, caretEnd;
    HWND editHwnd = GetDlgItem(g_hDlg, IDC_TIME);
    SendMessage(editHwnd, EM_GETSEL, (WPARAM)&caretStart, (LPARAM)&caretEnd);
    SetWindowText(editHwnd, buffer);
    SendMessage(editHwnd, EM_SETSEL, caretStart, caretEnd);

    g_isUpdatingTimeEdit = false;
}

void ReadTimeText()
{
    HWND editHwnd = GetDlgItem(g_hDlg, IDC_TIME);

    wchar_t buffer[10] = {};
    Edit_GetText(editHwnd, buffer, ARRAYSIZE(buffer));

    unsigned int seconds = TwoDigitAtoi(&buffer[0]) * 60 * 60
                         + TwoDigitAtoi(&buffer[3]) * 60
                         + TwoDigitAtoi(&buffer[6]);
    g_accumulatedTickCount = seconds * 1000;
}

void ZeroTime()
{
    g_accumulatedTickCount = 0;
}

void ResetStartTickCount()
{
    g_startTickCount = GetTickCount();
}

void AccumulateTickCount()
{
    unsigned int currentTickCount = GetTickCount();
    g_accumulatedTickCount += currentTickCount - g_startTickCount;
    g_startTickCount = currentTickCount;
}

VOID CALLBACK TimerProc(
    HWND hwnd,          // handle to window for timer messages 
    UINT message,       // WM_TIMER message 
    UINT_PTR idTimer,   // timer identifier 
    DWORD dwTime        // current system time 
)     
{
    AccumulateTickCount();
    if (g_timerInterval != g_defaultTimerInterval)
    {
        SetTimer(g_hDlg, 0, g_defaultTimerInterval, &TimerProc);
        g_timerInterval = g_defaultTimerInterval;
    }
    UpdateDisplayedText();
}

bool SimulateEditOverstrikeMode(HWND hWnd, unsigned int negativeCaretAdjustment)
{
    unsigned int caretRange = Edit_GetSel(hWnd);
    unsigned int caretBegin = LOWORD(caretRange);
    unsigned int caretEnd   = HIWORD(caretRange);
    if (caretEnd == caretBegin)
    {
        unsigned int textLength = Edit_GetTextLength(hWnd);
        caretBegin = (negativeCaretAdjustment < caretBegin) ? caretBegin - negativeCaretAdjustment : 0;
        caretBegin = (caretBegin >= textLength && textLength > 0) ? textLength - 1 : caretBegin;
        if (caretBegin < textLength)
        {
            Edit_SetSel(hWnd, caretBegin, caretBegin + 1);
            caretEnd = caretBegin + 1;
        }
    }
    return caretEnd != caretBegin;
}

LRESULT CALLBACK TimeEditProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData
)
{
    switch (uMsg)
    {
    case WM_CHAR:
        if ((wParam >= '0' && wParam <= '9') || (wParam  == ':'))
        {
            SimulateEditOverstrikeMode(hwnd, 0);
            DefSubclassProc(hwnd, uMsg, wParam, lParam);
            SimulateEditOverstrikeMode(hwnd, 0);
        }
        else if (wParam < 32 && wParam != 8)
        {
            // Pressing Ctrl+C or Ctrl+V generate control characters (Ctrl+C = 3, Ctrl+V = 22),
            // and these must be permitted through, or else copy and paste do not work, since
            // the edit control heeds these rather than the WM_KEYDOWN message. So let every
            // control character through except for backspace.
            return DefSubclassProc(hwnd, uMsg, wParam, lParam);
        }
        // Ignore any other characters.
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_BACK || wParam == VK_DELETE)
        {
            if (SimulateEditOverstrikeMode(hwnd, wParam == VK_BACK ? 1 : 0))
            {
                DefSubclassProc(hwnd, WM_CHAR, '0', 1);
                SimulateEditOverstrikeMode(hwnd, wParam == VK_BACK ? 2 : 0);
            }
            return 0;
        }
        break;
    } 
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void StopTimer()
{
    if (g_isTimerActive)
    {
        g_isTimerActive = false;
        AccumulateTickCount();

        KillTimer(g_hDlg, 0);

        SetWindowText(g_hState, L"⏸️");
    }
}

void StartTimer()
{
    if (!g_isTimerActive)
    {
        g_isTimerActive = true;
        g_startTickCount = GetTickCount();
        g_timerInterval = 1000 - (g_accumulatedTickCount % 1000);

        SetTimer(g_hDlg, 0, g_timerInterval, &TimerProc);

        SetWindowText(g_hState, L"▶️");
        UpdateDisplayedText();
    }
    else
    {
        StopTimer();
    }
}

void ResetTimer()
{
    ZeroTime();
    ResetStartTickCount();
    if (g_isTimerActive)
    {
        SetTimer(g_hDlg, 0, g_defaultTimerInterval, &TimerProc);
    }
    else
    {
        SetWindowText(g_hState, L"⏹️");
    }
    UpdateDisplayedText();
}

void SetWindowTranslucency(HWND windowHandle, unsigned int alpha)
{
    unsigned int previousExStyle = GetWindowExStyle(windowHandle);
    if (alpha >= 255) // Opaque.
    {
        SetWindowExStyle(windowHandle, previousExStyle & ~WS_EX_LAYERED);
    }
    else // Translucent.
    {
        SetWindowExStyle(windowHandle, previousExStyle | WS_EX_LAYERED);
        SetLayeredWindowAttributes(
            windowHandle,
            0,
            alpha,
            LWA_ALPHA
        );
    }
}

INT_PTR CALLBACK DialogMessageHandler(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
        {
            g_hDlg = hDlg;
            g_hState = GetDlgItem(g_hDlg, IDC_STATE);
            g_hLogo = GetDlgItem(g_hDlg, IDC_LOGO);
            g_hTimeEdit = GetDlgItem(g_hDlg, IDC_TIME);

            // Get screen dimensions.
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            // Compute new dialog position.
            RECT rect;
            GetWindowRect(g_hDlg, &rect);
            int dialogWidth = rect.right - rect.left;
            int dialogHeight = rect.bottom - rect.top;
            int dialogX = (screenWidth - dialogWidth) / 2;
            int dialogY = (screenHeight - dialogHeight) / 2;
            SetWindowPos(g_hDlg, nullptr, dialogX, dialogY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

            // Set icon.
            HICON icon = (HICON)LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCEW(IDI_ICONMAIN), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR | LR_DEFAULTSIZE);
            SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)icon);

            // Set label font.
            g_hTimeFont = CreateFont(44, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, L"MS Shell Dlg");
            SetWindowFont(g_hTimeEdit, g_hTimeFont, FALSE);
            SetWindowFont(g_hState, g_hTimeFont, FALSE);
            SetWindowFont(g_hLogo, g_hTimeFont, FALSE);

            UpdateDisplayedText();

            SetWindowSubclass(g_hTimeEdit, &TimeEditProc, 0, 0);

            HMENU systemMenu = GetSystemMenu(g_hDlg, FALSE);
            if (systemMenu)
            {
                InsertMenu(systemMenu, 0, MF_BYPOSITION | MF_STRING, IDM_MINIMIZE, L"Mi&nimize");
                InsertMenu(systemMenu, 0, MF_BYPOSITION | MF_STRING, IDM_TOGGLE_ALWAYS_ON_TOP, L"Toggle &Always On Top");
                InsertMenu(systemMenu, 0, MF_BYPOSITION | MF_STRING, IDM_TOGGLE_TRANSPARENCY, L"Toggle &Transparency");
                InsertMenu(systemMenu, 2, MF_BYPOSITION | MF_SEPARATOR, 0, L"");
            }

            return (INT_PTR)TRUE;
        }

        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            if (HIWORD(wParam) == 1) // Accelerator pressed.
            {
                wParam &= 0xFFFF; // Ignore accelerator vs menu difference.
            }
            switch (wParam)
            {
            case IDC_START:
            case IDOK:
                StartTimer();
                break;
            case IDC_STOP:
                StopTimer();
                break;
            case IDC_RESET:
                ResetTimer();
                break;
            case IDCANCEL:
                if (g_isTimerActive)
                    StopTimer();
                else
                    ResetTimer();
                break;
            case ((EN_CHANGE << 16) | IDC_TIME):
                if (!g_isUpdatingTimeEdit) // // Don't react to an update cause by setting the text programmatically.
                {
                    g_isUpdatingTimeEdit = true; // Tis silly that EN_CHANGE occurs when programmatically set too :/.
                    ReadTimeText();
                    UpdateDisplayedText();
                    g_isUpdatingTimeEdit = false;
                }
                break;
            }
            break;
        }
        case WM_SYSCOMMAND:
            switch (wParam)
            {
            case IDM_TOGGLE_TRANSPARENCY:
                SetWindowTranslucency(hDlg, (GetWindowExStyle(hDlg) & WS_EX_LAYERED) ? 255 : 164);
                break;
            case IDM_TOGGLE_ALWAYS_ON_TOP:
            {
                bool isTopmost = GetWindowExStyle(hDlg) & WS_EX_TOPMOST;
                SetWindowPos(hDlg, isTopmost ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
                break;
            }
            case IDM_MINIMIZE:
                CloseWindow(hDlg);
                break;
            }
            break;

        case WM_CLOSE:
        {
            EndDialog(hDlg, LOWORD(wParam));
            DeleteFont(g_hTimeFont);
            g_hDlg = nullptr;
            return (INT_PTR)TRUE;
        }

        case WM_DESTROY:
        {
            RemoveWindowSubclass(g_hTimeEdit, &TimeEditProc, 0);
            return 0;
        }
    } // switch
    return (INT_PTR)FALSE;
}
