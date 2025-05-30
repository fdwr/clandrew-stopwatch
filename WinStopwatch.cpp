#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <windows.h>
#include <windowsx.h>
#include "resource.h"

unsigned int g_accumulatedTickCount = 0; // Any previous accumulated time before pausing.
unsigned int g_startTickCount = 0; // Last start time (set to GetTickCount).
unsigned int g_timerInterval = 0;
constexpr unsigned int g_defaultTimerInterval = 1000;

HWND g_hDlg = nullptr;
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

            // Set label font.
            HWND timeWindow = GetDlgItem(g_hDlg, IDC_TIME);
            g_hTimeFont = CreateFont(44, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, L"MS Shell Dlg");
            SetWindowFont(timeWindow, g_hTimeFont, FALSE);
            SetWindowFont(g_hState, g_hTimeFont, FALSE);
            SetWindowFont(g_hLogo, g_hTimeFont, FALSE);

            UpdateDisplayedText();

            return (INT_PTR)TRUE;
        }

        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            if (id == IDC_START || id == IDOK)
            {
                StartTimer();
            }
            else if (id == IDC_STOP)
            {
                StopTimer();
            }
            else if (id == IDC_RESET)
            {
                ResetTimer();
            }
            else if (id == IDCANCEL)
            {
                if (g_isTimerActive)
                    StopTimer();
                else
                    ResetTimer();
            }
            else if (wParam == ((EN_CHANGE << 16) | IDC_TIME))
            {
                if (!g_isUpdatingTimeEdit) // // Don't react to an update cause by setting the text programmatically.
                {
                    g_isUpdatingTimeEdit = true; // Tis silly that EN_CHANGE occurs when programmatically set too :/.
                    ReadTimeText();
                    UpdateDisplayedText();
                    g_isUpdatingTimeEdit = false;
                }
            }
            break;
        }

        case WM_CLOSE:
        {
            EndDialog(hDlg, LOWORD(wParam));
            g_hDlg = nullptr;
            return (INT_PTR)TRUE;
        }
    }
    return (INT_PTR)FALSE;
}
