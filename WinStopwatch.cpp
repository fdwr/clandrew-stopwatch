#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include "resource.h"

int g_seconds = 0;
int g_minutes = 0;
HWND g_hDlg = NULL;
HWND g_hDot = NULL;

INT_PTR CALLBACK DialogMessageHandler(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), nullptr, DialogMessageHandler);
    return TRUE;
}

void TwoDigitItoa(int n, char* pBuf, int* pDestIndex)
{
    int high{};
    int low{};

    if (n >= 10)
    {
        high = n / 10;
        low = n % 10;

    }
    else
    {
        high = 0;
        low = n;
    }

    pBuf[*pDestIndex] = '0' + high;
    (*pDestIndex)++;

    pBuf[*pDestIndex] = '0' + low;
    (*pDestIndex)++;

}

void UpdateDisplayedText()
{
    char buf[32]{};
    int destIndex = 0;

    TwoDigitItoa(g_minutes, buf, &destIndex);

    buf[destIndex] = ':';
    destIndex++;

    TwoDigitItoa(g_seconds, buf, &destIndex);

    buf[destIndex] = '\0';
    destIndex++;

    SetDlgItemTextA(g_hDlg, IDC_EDIT1, buf);
}

VOID CALLBACK TimerProc(
    HWND hwnd,        // handle to window for timer messages 
    UINT message,     // WM_TIMER message 
    UINT_PTR idTimer, // timer identifier 
    DWORD dwTime)     // current system time 
{
    g_seconds++;
    if (g_seconds >= 60)
    {
        g_seconds = 0;

        g_minutes++;
        if (g_minutes >= 60) // Timer only goes up to 60 minutes, then automatically stops
        {
            g_minutes = 0;
            ShowWindow(g_hDot, SW_HIDE);
            KillTimer(g_hDlg, 0);
        }
    }
    UpdateDisplayedText();
}

void ZeroTime()
{
    g_seconds = 0;
    g_minutes = 0;
}

INT_PTR CALLBACK DialogMessageHandler(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
        {
            ZeroTime();
            g_hDlg = hDlg;
            g_hDot = GetDlgItem(g_hDlg, IDC_DOT);
            ShowWindow(g_hDot, SW_HIDE);
            UpdateDisplayedText();
            return (INT_PTR)TRUE;
        }

        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            if (id == IDC_START)
            {
                ShowWindow(g_hDot, SW_SHOW);
                SetTimer(hDlg, 0, 1000, &TimerProc);
            }
            else if (id == IDC_STOP)
            {
                ShowWindow(g_hDot, SW_HIDE);
                KillTimer(hDlg, 0);
            }
            else if (id == IDC_RESET)
            {
                ShowWindow(g_hDot, SW_HIDE);
                ZeroTime();
                UpdateDisplayedText();
                KillTimer(hDlg, 0);
            }
            break;
        }
        case WM_CLOSE:
        {
            EndDialog(hDlg, LOWORD(wParam));
            g_hDlg = NULL;
            return (INT_PTR)TRUE;
        }
    }
    return (INT_PTR)FALSE;
}
