// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//

#include <windows.h>
#include <strsafe.h>
#include <shobjidl.h>   // For ITaskbarList3
#include "resource.h"

#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define MAX_PROGRESS_IND     50
#define MAX_PROGRESS_NORMAL  200

HINSTANCE g_hInstance = NULL;

ITaskbarList3 *g_pTaskbarList = NULL;   // careful, COM objects should only be accessed from apartment they are created in
UINT_PTR g_nTimerId = 0;
int g_nProgress = 0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static UINT s_uTBBC = WM_NULL;

    if (s_uTBBC == WM_NULL)
    {
        // Compute the value for the TaskbarButtonCreated message
        s_uTBBC = RegisterWindowMessage(L"TaskbarButtonCreated");

        // In case the application is run elevated, allow the
        // TaskbarButtonCreated message through.
        ChangeWindowMessageFilter(s_uTBBC, MSGFLT_ADD);
    }

    if (message == s_uTBBC)
    {
        // Once we get the TaskbarButtonCreated message, we can call methods
        // specific to our window on a TaskbarList instance. Note that it's
        // possible this message can be received multiple times over the lifetime
        // of this window (if explorer terminates and restarts, for example).
        if (!g_pTaskbarList)
        {
            HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_pTaskbarList));
            if (SUCCEEDED(hr))
            {
                hr = g_pTaskbarList->HrInit();
                if (FAILED(hr))
                {
                    g_pTaskbarList->Release();
                    g_pTaskbarList = NULL;
                }
            }
        }
    }
    else switch (message)
    {
        case WM_COMMAND:
        {
            int const wmId = LOWORD(wParam);
            switch (wmId)
            {
                case IDM_OVERLAY1:
                    {
                    HICON icon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_OVERLAY1));
                    g_pTaskbarList->SetOverlayIcon(hWnd, icon, L"Green");
                    DestroyIcon(icon);
                    }
                    break;

                case IDM_OVERLAY2:
                    {
                    HICON icon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_OVERLAY2));
                    g_pTaskbarList->SetOverlayIcon(hWnd, icon, L"Red");
                    DestroyIcon(icon);
                    }
                    break;   

                case IDM_OVERLAY_CLEAR:
                    g_pTaskbarList->SetOverlayIcon(hWnd, 0, NULL);                                 
                    break;

                case IDM_SIMULATEPROGRESS:
                    // If simulated progress isn't underway, start it
                    if (g_pTaskbarList && g_nTimerId == 0)
                    {
                        g_nTimerId = SetTimer(hWnd, 1, 50, NULL);
                        g_nProgress = 0;
                    }
                    break;

                case IDM_PROGRESS_NONE:
                    g_pTaskbarList->SetProgressState(hWnd, TBPF_NOPROGRESS);
                    break;

                case IDM_PROGRESS_INDETERMINATE:
                    g_pTaskbarList->SetProgressState(hWnd, TBPF_INDETERMINATE);
                    break;

                case IDM_PROGRESS_NORMAL:
                    g_pTaskbarList->SetProgressState(hWnd, TBPF_NORMAL);
                    break;

                case IDM_PROGRESS_ERROR:
                    g_pTaskbarList->SetProgressState(hWnd, TBPF_ERROR);
                    break;

                case IDM_PROGRESS_PAUSED:
                    g_pTaskbarList->SetProgressState(hWnd, TBPF_PAUSED);
                    break;

                case IDM_PROGRESS_STOP:
                    KillTimer(hWnd, g_nTimerId);
                    g_nTimerId = 0;
                    break;

                case IDM_PROGRESS_RESUME:
                    if (g_pTaskbarList && g_nTimerId == 0)
                    {
                        g_nTimerId = SetTimer(hWnd, 1, 50, NULL);
                    }
                    break;

                case IDM_FLASH_MINIMIZE:
                    ShowWindow(hWnd, SW_MINIMIZE);
                    FlashWindow(hWnd,true);
                    break;

                case IDM_FLASH_BACKGROUND:
                    {
                    //HWND next = GetWindow(hWnd, GW_HWNDNEXT);
                    // Funny enough doing this keeps keyboard focus to this window and it is still returned by GetForegroundWindow
                    // This state Windows 11 taskbar and Cairo both need two clicks to bring that window back to the foreground
                    SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                    //                    
                    //SetForegroundWindow(GetTopWindow(NULL));
                    //SetForegroundWindow(next);
                    //SetForegroundWindow(GetWindow(NULL, GW_HWNDFIRST));
                    //SetWindowPos(GetTopWindow(NULL), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

                    //PostMessage(next, WM_SYSCOMMAND, SC_RESTORE, 0);
                    //SetForegroundWindow(GetLastActivePopup(next));

                    FlashWindow(hWnd, true);
                    }
                    break;

                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;

                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        }

        case WM_TIMER:
            g_nProgress++;
            if (g_nProgress == 1)
            {
                // First time through, so we'll set our progress state to
                // be indeterminate - this simulates a background computation
                // to figure out how much progress we'll need.
                g_pTaskbarList->SetProgressState(hWnd, TBPF_INDETERMINATE);
            }
            else if (g_nProgress == MAX_PROGRESS_IND)
            {
                // Now set the progress state to indicate we have some normal
                // progress to show.
                g_pTaskbarList->SetProgressValue(hWnd, 0, MAX_PROGRESS_NORMAL);
                g_pTaskbarList->SetProgressState(hWnd, TBPF_NORMAL);
            }
            else if (g_nProgress > MAX_PROGRESS_IND)
            {
                if (g_nProgress - MAX_PROGRESS_IND <= MAX_PROGRESS_NORMAL)
                {
                    // Now show normal progress to simulate a background operation
                    g_pTaskbarList->SetProgressValue(hWnd, g_nProgress - MAX_PROGRESS_IND, MAX_PROGRESS_NORMAL);
                }
                else
                {
                    // Progress is done, stop the timer and reset progress state
                    KillTimer(hWnd, g_nTimerId);
                    g_nTimerId = 0;
                    //FlashWindow(hWnd,true);
                    MessageBox(hWnd, L"Done!", L"Progress Complete", MB_OK);
                    g_pTaskbarList->SetProgressState(hWnd, TBPF_NOPROGRESS);
                }
            }
            break;

        case WM_DESTROY:
            if (g_nTimerId)
            {
                KillTimer(hWnd, g_nTimerId);
                g_nTimerId = 0;
            }
            if (g_pTaskbarList)
            {
                g_pTaskbarList->Release();
                g_pTaskbarList = NULL;
            }
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    g_hInstance = hInstance;

    HRESULT hrInit = CoInitialize(NULL);    // Initialize COM so we can call CoCreateInstance
    if (SUCCEEDED(hrInit))
    {
        WCHAR const szWindowClass[] = L"TaskbarStatusWnd";   // The main window class name

        WNDCLASSEX wc = { sizeof(wc) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        wc.lpszMenuName = MAKEINTRESOURCE(IDC_PERIPHERALSTATUS);
        wc.lpszClassName = szWindowClass;

        RegisterClassEx(&wc);

        WCHAR szTitle[100];
        LoadString(hInstance, IDS_APP_TITLE, szTitle, ARRAYSIZE(szTitle));

        HWND hWnd = CreateWindowEx(0, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 400, 400, NULL, NULL, hInstance, NULL);
        if (hWnd)
        {
            ShowWindow(hWnd, nCmdShow);
            UpdateWindow(hWnd);

            // Main message loop:
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        CoUninitialize();
    }

    return 0;
}
