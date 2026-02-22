#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "ui.h"
#include "database.h"

HINSTANCE g_hInst;

static HWND g_hStatus;
static HWND g_hNav;
static HWND g_hPanels[7];
static int  g_curPanel = -1;

typedef void (*PanelRefreshFn)(void);
static PanelRefreshFn g_refreshFns[7];

/* Forward declarations */
static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* -------------------------------------------------------------------------
   ShowPanel — hide the old panel, size and show the new one, then refresh.
   ------------------------------------------------------------------------- */
static void ShowPanel(HWND hMain, int idx)
{
    RECT rc;
    RECT srect;
    int  sh, i;
    int  pw, ph;

    if (idx < 0 || idx >= 7) return;

    GetClientRect(hMain, &rc);

    /* Height of status bar */
    GetWindowRect(g_hStatus, &srect);
    sh = srect.bottom - srect.top;

    pw = rc.right  - NAV_WIDTH;
    ph = rc.bottom - sh;

    for (i = 0; i < 7; i++)
        ShowWindow(g_hPanels[i], (i == idx) ? SW_SHOW : SW_HIDE);

    MoveWindow(g_hPanels[idx], NAV_WIDTH, 0, pw, ph, TRUE);

    g_curPanel = idx;
    g_refreshFns[idx]();
}

/* -------------------------------------------------------------------------
   MainWndProc
   ------------------------------------------------------------------------- */
static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        int i;

        /* Navigation list box */
        g_hNav = CreateWindowEx(0, "LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_BORDER,
            0, 0, NAV_WIDTH, 400,
            hWnd, (HMENU)(INT_PTR)IDC_NAV_LIST, g_hInst, NULL);

        SendMessage(g_hNav, LB_ADDSTRING, 0, (LPARAM)"Formulations");
        SendMessage(g_hNav, LB_ADDSTRING, 0, (LPARAM)"Compounds");
        SendMessage(g_hNav, LB_ADDSTRING, 0, (LPARAM)"Tasting Sessions");
        SendMessage(g_hNav, LB_ADDSTRING, 0, (LPARAM)"Batches");
        SendMessage(g_hNav, LB_ADDSTRING, 0, (LPARAM)"Inventory");
        SendMessage(g_hNav, LB_ADDSTRING, 0, (LPARAM)"Regulatory");
        SendMessage(g_hNav, LB_ADDSTRING, 0, (LPARAM)"Suppliers");

        /* Status bar */
        g_hStatus = CreateStatusWindow(WS_CHILD | WS_VISIBLE,
            "Ready", hWnd, IDC_STATUS_BAR);

        /* Create panels (hidden by default) */
        g_hPanels[NAV_FORMULATIONS] = Panel_Formulations_Create(hWnd);
        g_hPanels[NAV_COMPOUNDS]    = Panel_Compounds_Create(hWnd);
        g_hPanels[NAV_TASTING]      = Panel_Tasting_Create(hWnd);
        g_hPanels[NAV_BATCH]        = Panel_Batch_Create(hWnd);
        g_hPanels[NAV_INVENTORY]    = Panel_Inventory_Create(hWnd);
        g_hPanels[NAV_REGULATORY]   = Panel_Regulatory_Create(hWnd);
        g_hPanels[NAV_SUPPLIERS]    = Panel_Suppliers_Create(hWnd);

        g_refreshFns[NAV_FORMULATIONS] = Panel_Formulations_Refresh;
        g_refreshFns[NAV_COMPOUNDS]    = Panel_Compounds_Refresh;
        g_refreshFns[NAV_TASTING]      = Panel_Tasting_Refresh;
        g_refreshFns[NAV_BATCH]        = Panel_Batch_Refresh;
        g_refreshFns[NAV_INVENTORY]    = Panel_Inventory_Refresh;
        g_refreshFns[NAV_REGULATORY]   = Panel_Regulatory_Refresh;
        g_refreshFns[NAV_SUPPLIERS]    = Panel_Suppliers_Refresh;

        for (i = 0; i < 7; i++)
            ShowWindow(g_hPanels[i], SW_HIDE);

        /* Select the first nav item; post to self so WM_SIZE has fired first */
        SendMessage(g_hNav, LB_SETCURSEL, 0, 0);
        PostMessage(hWnd, WM_COMMAND,
            MAKEWPARAM(IDC_NAV_LIST, LBN_SELCHANGE), (LPARAM)g_hNav);

        SendMessage(g_hStatus, SB_SETTEXT, 0,
            (LPARAM)"formulations.db open");
    }
    return 0;

    case WM_SIZE:
    {
        WORD w = LOWORD(lParam);
        WORD h = HIWORD(lParam);
        RECT srect;
        int  sh;

        /* Resize status bar first */
        SendMessage(g_hStatus, WM_SIZE, 0, 0);
        GetWindowRect(g_hStatus, &srect);
        sh = srect.bottom - srect.top;

        /* Resize nav list */
        MoveWindow(g_hNav, 0, 0, NAV_WIDTH, h - sh, TRUE);

        /* Resize current content panel */
        if (g_curPanel >= 0 && g_hPanels[g_curPanel])
            MoveWindow(g_hPanels[g_curPanel],
                NAV_WIDTH, 0, w - NAV_WIDTH, h - sh, TRUE);
    }
    return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_NAV_LIST &&
            HIWORD(wParam) == LBN_SELCHANGE)
        {
            char buf[128];
            int  sel;
            const char* labels[] = {
                "Formulations", "Compounds",
                "Tasting Sessions", "Batches", "Inventory", "Regulatory",
                "Suppliers"
            };

            sel = (int)SendMessage(g_hNav, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                ShowPanel(hWnd, sel);
                sprintf(buf, "formulations.db  |  %s", labels[sel]);
                SendMessage(g_hStatus, SB_SETTEXT, 0, (LPARAM)buf);
            }
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* -------------------------------------------------------------------------
   WinMain
   ------------------------------------------------------------------------- */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    HWND       hWnd;
    MSG        msg;
    INITCOMMONCONTROLSEX icex;

    (void)hPrevInstance;
    (void)lpCmdLine;

    g_hInst = hInstance;

    /* Initialise common controls */
    icex.dwSize = sizeof(icex);
    icex.dwICC  = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    /* Open database */
    if (db_open("formulations.db") != 0) {
        MessageBox(NULL, "Failed to open formulations.db", "Error", MB_ICONERROR);
        return 1;
    }
    db_seed_compound_library();
    db_seed_inventory();

    /* Register main window class */
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "SodaFormulatorMain";
    RegisterClassEx(&wc);

    /* Create main window */
    hWnd = CreateWindowEx(0,
        "SodaFormulatorMain",
        "Soda Formulator v0.1.0",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 700,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        MessageBox(NULL, "Failed to create main window", "Error", MB_ICONERROR);
        db_close();
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    /* Message loop */
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    db_close();
    return (int)msg.wParam;
}
