#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "database.h"
#include "sqlite3.h"

/* =========================================================================
   File-scope state
   ========================================================================= */
static HWND g_hPanel    = NULL;
static HWND g_hListView = NULL;
static HWND g_hBtnEditCost;

/* Dialog state */
static BOOL g_dlgDone;
static BOOL g_dlgSaved;
static char g_dlgCompoundName[128];
static HWND g_hCostDlg = NULL;

/* =========================================================================
   Helpers
   ========================================================================= */
static void LV_AddCol(HWND hLV, int idx, const char* text, int width)
{
    LVCOLUMN lvc;
    ZeroMemory(&lvc, sizeof(lvc));
    lvc.mask     = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.cx       = width;
    lvc.pszText  = (char*)text;
    lvc.iSubItem = idx;
    ListView_InsertColumn(hLV, idx, &lvc);
}

static int LV_InsertRow(HWND hLV, int row, const char* col0)
{
    LVITEM lvi;
    ZeroMemory(&lvi, sizeof(lvi));
    lvi.mask     = LVIF_TEXT;
    lvi.iItem    = row;
    lvi.iSubItem = 0;
    lvi.pszText  = (char*)col0;
    return ListView_InsertItem(hLV, &lvi);
}

static void LV_SetCell(HWND hLV, int row, int col, const char* text)
{
    ListView_SetItemText(hLV, row, col, (char*)text);
}

/* =========================================================================
   Edit Cost dialog WndProc
   ========================================================================= */
static LRESULT CALLBACK CostDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        char label[160];
        char curCost[32];
        sqlite3*      db   = db_get_handle();
        sqlite3_stmt* stmt = NULL;

        sprintf(label, "Compound: %s", g_dlgCompoundName);
        CreateWindowEx(0, "STATIC", label,
            WS_CHILD | WS_VISIBLE,
            10, 12, 320, 18, hWnd, NULL, g_hInst, NULL);

        /* Fetch current cost */
        curCost[0] = '\0';
        if (db) {
            if (sqlite3_prepare_v2(db,
                    "SELECT cost_per_gram FROM compound_library WHERE compound_name=?;",
                    -1, &stmt, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(stmt, 1, g_dlgCompoundName, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW)
                    sprintf(curCost, "%.4f", sqlite3_column_double(stmt, 0));
                sqlite3_finalize(stmt);
            }
        }

        CreateWindowEx(0, "STATIC", "Cost ($/g):",
            WS_CHILD | WS_VISIBLE,
            10, 38, 90, 18, hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", curCost,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            105, 36, 120, 22, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_COST_FIELD, g_hInst, NULL);

        CreateWindowEx(0, "BUTTON", "Save",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 72, 80, 26, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_SAVE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            100, 72, 80, 26, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_CANCEL, g_hInst, NULL);
    }
    return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_SAVE:
        {
            char costStr[32];
            float cost;
            GetWindowText(GetDlgItem(hWnd, IDC_DLG_COST_FIELD), costStr, sizeof(costStr));
            cost = (float)atof(costStr);
            if (cost < 0.0f) {
                MessageBox(hWnd, "Cost cannot be negative.", "Input", MB_OK);
                break;
            }
            if (db_set_compound_cost(g_dlgCompoundName, cost) == 0) {
                g_dlgSaved = TRUE;
                g_dlgDone  = TRUE;
                DestroyWindow(hWnd);
            } else {
                MessageBox(hWnd, "Failed to update cost.", "Error", MB_ICONERROR);
            }
        }
        break;
        case IDC_BTN_CANCEL:
            g_dlgDone = TRUE;
            DestroyWindow(hWnd);
            break;
        }
        return 0;

    case WM_CLOSE:
        g_dlgDone = TRUE;
        DestroyWindow(hWnd);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   OpenCostDialog
   ========================================================================= */
static void OpenCostDialog(HWND hParent)
{
    RECT pr;
    int  dw = 360, dh = 130;
    int  cx, cy;
    MSG  msg;

    GetWindowRect(hParent, &pr);
    cx = pr.left + (pr.right  - pr.left - dw) / 2;
    cy = pr.top  + (pr.bottom - pr.top  - dh) / 2;

    g_dlgDone  = FALSE;
    g_dlgSaved = FALSE;

    g_hCostDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "CompoundCostDlgClass",
        "Edit Compound Cost",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        cx, cy, dw, dh,
        hParent, NULL, g_hInst, NULL);

    EnableWindow(hParent, FALSE);

    while (!g_dlgDone) {
        if (!GetMessage(&msg, NULL, 0, 0)) { PostQuitMessage((int)msg.wParam); break; }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(hParent, TRUE);
    if (IsWindow(g_hCostDlg)) DestroyWindow(g_hCostDlg);
    g_hCostDlg = NULL;
    SetForegroundWindow(hParent);
}

/* =========================================================================
   Panel WndProc
   ========================================================================= */
static LRESULT CALLBACK CompPanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        g_hBtnEditCost = CreateWindowEx(0, "BUTTON", "Edit Cost",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            4, 4, 90, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_EDIT_COST, g_hInst, NULL);

        g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE |
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 36, 100, 100, hWnd,
            (HMENU)(INT_PTR)IDC_LISTVIEW, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LV_AddCol(g_hListView, 0, "Name",        170);
        LV_AddCol(g_hListView, 1, "FEMA",         60);
        LV_AddCol(g_hListView, 2, "Max ppm",       80);
        LV_AddCol(g_hListView, 3, "Rec Min",       75);
        LV_AddCol(g_hListView, 4, "Rec Max",       75);
        LV_AddCol(g_hListView, 5, "$/g",           70);
        LV_AddCol(g_hListView, 6, "Solubilizer",   85);
        LV_AddCol(g_hListView, 7, "Storage",       70);
    }
    return 0;

    case WM_SIZE:
    {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        MoveWindow(g_hListView, 0, 36, w, h - 36, TRUE);
    }
    return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_EDIT_COST) {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (sel < 0) {
                MessageBox(hWnd, "Select a compound to edit its cost.", "Edit Cost", MB_OK);
                break;
            }
            ListView_GetItemText(g_hListView, sel, 0,
                                 g_dlgCompoundName, sizeof(g_dlgCompoundName));
            OpenCostDialog(hWnd);
            if (g_dlgSaved) Panel_Compounds_Refresh();
        }
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   Panel_Compounds_Create
   ========================================================================= */
HWND Panel_Compounds_Create(HWND hParent)
{
    WNDCLASSEX wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = CompPanelWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "CompoundsPanel";
    RegisterClassEx(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = CostDlgWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "CompoundCostDlgClass";
    RegisterClassEx(&wc);

    g_hPanel = CreateWindowEx(0,
        "CompoundsPanel", NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}

/* =========================================================================
   Panel_Compounds_Refresh
   ========================================================================= */
void Panel_Compounds_Refresh(void)
{
    sqlite3*      db   = db_get_handle();
    sqlite3_stmt* stmt = NULL;
    int           row  = 0;
    char          buf[64];

    if (!g_hListView || !db) return;

    ListView_DeleteAllItems(g_hListView);

    if (sqlite3_prepare_v2(db,
            "SELECT compound_name, fema_number, max_use_ppm, "
            "       rec_min_ppm, rec_max_ppm, cost_per_gram, "
            "       requires_solubilizer, storage_temp "
            "FROM compound_library ORDER BY compound_name;",
            -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name   = (const char*)sqlite3_column_text(stmt, 0);
        int         fema   = sqlite3_column_int   (stmt, 1);
        double      maxppm = sqlite3_column_double(stmt, 2);
        double      recmin = sqlite3_column_double(stmt, 3);
        double      recmax = sqlite3_column_double(stmt, 4);
        double      cpg    = sqlite3_column_double(stmt, 5);
        int         solub  = sqlite3_column_int   (stmt, 6);
        const char* stor   = (const char*)sqlite3_column_text(stmt, 7);

        LV_InsertRow(g_hListView, row, name ? name : "");

        sprintf(buf, "%d",     fema);   LV_SetCell(g_hListView, row, 1, buf);
        sprintf(buf, "%.1f",   maxppm); LV_SetCell(g_hListView, row, 2, buf);
        sprintf(buf, "%.2f",   recmin); LV_SetCell(g_hListView, row, 3, buf);
        sprintf(buf, "%.2f",   recmax); LV_SetCell(g_hListView, row, 4, buf);
        sprintf(buf, "%.4f",   cpg);    LV_SetCell(g_hListView, row, 5, buf);
        LV_SetCell(g_hListView, row, 6, solub ? "Yes" : "No");
        LV_SetCell(g_hListView, row, 7, stor ? stor : "");

        row++;
    }

    sqlite3_finalize(stmt);
}
