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
static HWND g_hBtnUpdateStock;
static HWND g_hBtnUpdateCost;

/* Dialog state */
static BOOL g_dlgDone;
static BOOL g_dlgSaved;
static char g_dlgCompoundName[128];
static HWND g_hInvDlg = NULL;

/* Which dialog is open: 0 = stock, 1 = cost */
static int  g_dlgMode;

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
   Inventory update dialog WndProc
   Handles both stock update (mode=0) and cost update (mode=1)
   ========================================================================= */
static LRESULT CALLBACK InvDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        char label[160];
        sqlite3*      db   = db_get_handle();
        sqlite3_stmt* stmt = NULL;
        char          curStock[32]   = "";
        char          curReorder[32] = "";
        char          curCost[32]    = "";

        sprintf(label, "Compound: %s", g_dlgCompoundName);
        CreateWindowEx(0, "STATIC", label,
            WS_CHILD | WS_VISIBLE,
            10, 10, 340, 18, hWnd, NULL, g_hInst, NULL);

        /* Fetch current values */
        if (db) {
            if (sqlite3_prepare_v2(db,
                    "SELECT ci.stock_grams, ci.reorder_threshold_grams, cl.cost_per_gram "
                    "FROM compound_inventory ci "
                    "JOIN compound_library cl ON cl.id = ci.compound_library_id "
                    "WHERE cl.compound_name = ?;",
                    -1, &stmt, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(stmt, 1, g_dlgCompoundName, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    sprintf(curStock,   "%.4f", sqlite3_column_double(stmt, 0));
                    sprintf(curReorder, "%.4f", sqlite3_column_double(stmt, 1));
                    sprintf(curCost,    "%.4f", sqlite3_column_double(stmt, 2));
                }
                sqlite3_finalize(stmt);
            }
        }

        if (g_dlgMode == 0) {
            /* Update Stock */
            int y = 36;
            CreateWindowEx(0, "STATIC", "Stock (g):",
                WS_CHILD | WS_VISIBLE,
                10, y + 3, 90, 18, hWnd, NULL, g_hInst, NULL);
            CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", curStock,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                105, y, 120, 22, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_STOCK_FIELD, g_hInst, NULL);
            y += 32;

            CreateWindowEx(0, "STATIC", "Reorder (g):",
                WS_CHILD | WS_VISIBLE,
                10, y + 3, 90, 18, hWnd, NULL, g_hInst, NULL);
            CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", curReorder,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                105, y, 120, 22, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_REORDER, g_hInst, NULL);
            y += 40;

            CreateWindowEx(0, "BUTTON", "Save",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, y, 80, 26, hWnd,
                (HMENU)(INT_PTR)IDC_BTN_SAVE, g_hInst, NULL);
            CreateWindowEx(0, "BUTTON", "Cancel",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                100, y, 80, 26, hWnd,
                (HMENU)(INT_PTR)IDC_BTN_CANCEL, g_hInst, NULL);
        } else {
            /* Update Cost */
            int y = 36;
            CreateWindowEx(0, "STATIC", "Cost ($/g):",
                WS_CHILD | WS_VISIBLE,
                10, y + 3, 90, 18, hWnd, NULL, g_hInst, NULL);
            CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", curCost,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                105, y, 120, 22, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_COST_FIELD, g_hInst, NULL);
            y += 40;

            CreateWindowEx(0, "BUTTON", "Save",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, y, 80, 26, hWnd,
                (HMENU)(INT_PTR)IDC_BTN_SAVE, g_hInst, NULL);
            CreateWindowEx(0, "BUTTON", "Cancel",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                100, y, 80, 26, hWnd,
                (HMENU)(INT_PTR)IDC_BTN_CANCEL, g_hInst, NULL);
        }
    }
    return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {

        case IDC_BTN_SAVE:
        {
            if (g_dlgMode == 0) {
                /* Update stock + reorder */
                char stockStr[32], reorderStr[32];
                float stock, reorder;
                sqlite3*      db   = db_get_handle();
                sqlite3_stmt* stmt = NULL;

                GetWindowText(GetDlgItem(hWnd, IDC_DLG_STOCK_FIELD),  stockStr,   sizeof(stockStr));
                GetWindowText(GetDlgItem(hWnd, IDC_DLG_REORDER),       reorderStr, sizeof(reorderStr));
                stock   = (float)atof(stockStr);
                reorder = (float)atof(reorderStr);

                if (stock < 0.0f || reorder < 0.0f) {
                    MessageBox(hWnd, "Values cannot be negative.", "Input", MB_OK);
                    break;
                }

                if (db && sqlite3_prepare_v2(db,
                        "UPDATE compound_inventory "
                        "SET stock_grams = ?, reorder_threshold_grams = ?, "
                        "    last_updated = DATETIME('now','localtime') "
                        "WHERE compound_library_id = "
                        "    (SELECT id FROM compound_library WHERE compound_name = ?);",
                        -1, &stmt, NULL) == SQLITE_OK)
                {
                    sqlite3_bind_double(stmt, 1, (double)stock);
                    sqlite3_bind_double(stmt, 2, (double)reorder);
                    sqlite3_bind_text  (stmt, 3, g_dlgCompoundName, -1, SQLITE_STATIC);

                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        g_dlgSaved = TRUE;
                        g_dlgDone  = TRUE;
                    } else {
                        MessageBox(hWnd, "Failed to update stock.", "Error", MB_ICONERROR);
                    }
                    sqlite3_finalize(stmt);
                }

                if (g_dlgSaved) DestroyWindow(hWnd);
            } else {
                /* Update cost */
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
   OpenInvDialog
   ========================================================================= */
static void OpenInvDialog(HWND hParent, int mode)
{
    RECT pr;
    int  dw = 380, dh = (mode == 0) ? 165 : 135;
    int  cx, cy;
    MSG  msg;

    g_dlgMode = mode;

    GetWindowRect(hParent, &pr);
    cx = pr.left + (pr.right  - pr.left - dw) / 2;
    cy = pr.top  + (pr.bottom - pr.top  - dh) / 2;

    g_dlgDone  = FALSE;
    g_dlgSaved = FALSE;

    g_hInvDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "InventoryDlgClass",
        (mode == 0) ? "Update Stock" : "Update Cost",
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
    if (IsWindow(g_hInvDlg)) DestroyWindow(g_hInvDlg);
    g_hInvDlg = NULL;
    SetForegroundWindow(hParent);
}

/* =========================================================================
   Panel WndProc
   ========================================================================= */
static LRESULT CALLBACK InvPanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        int bx = 4, by = 4;

        g_hBtnUpdateStock = CreateWindowEx(0, "BUTTON", "Update Stock",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx, by, 110, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_UPDATE_STOCK, g_hInst, NULL);
        bx += 116;

        g_hBtnUpdateCost = CreateWindowEx(0, "BUTTON", "Update Cost",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx, by, 110, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_UPDATE_COST, g_hInst, NULL);

        g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE |
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 36, 100, 100, hWnd,
            (HMENU)(INT_PTR)IDC_LISTVIEW, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LV_AddCol(g_hListView, 0, "Compound",   170);
        LV_AddCol(g_hListView, 1, "Stock (g)",   90);
        LV_AddCol(g_hListView, 2, "Reorder (g)", 90);
        LV_AddCol(g_hListView, 3, "$/g",          70);
        LV_AddCol(g_hListView, 4, "Status",       70);
        LV_AddCol(g_hListView, 5, "Last Updated", 160);
    }
    return 0;

    case WM_SIZE:
    {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        MoveWindow(g_hListView, 0, 36, w, h - 36, TRUE);
    }
    return 0;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == IDC_BTN_UPDATE_STOCK || id == IDC_BTN_UPDATE_COST) {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (sel < 0) {
                MessageBox(hWnd, "Select a compound first.", "Update", MB_OK);
                break;
            }
            ListView_GetItemText(g_hListView, sel, 0,
                                 g_dlgCompoundName, sizeof(g_dlgCompoundName));
            OpenInvDialog(hWnd, (id == IDC_BTN_UPDATE_STOCK) ? 0 : 1);
            if (g_dlgSaved) Panel_Inventory_Refresh();
        }
    }
    return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   Panel_Inventory_Create
   ========================================================================= */
HWND Panel_Inventory_Create(HWND hParent)
{
    WNDCLASSEX wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = InvPanelWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "InventoryPanel";
    RegisterClassEx(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = InvDlgWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "InventoryDlgClass";
    RegisterClassEx(&wc);

    g_hPanel = CreateWindowEx(0,
        "InventoryPanel", NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}

/* =========================================================================
   Panel_Inventory_Refresh
   ========================================================================= */
void Panel_Inventory_Refresh(void)
{
    sqlite3*      db   = db_get_handle();
    sqlite3_stmt* stmt = NULL;
    int           row  = 0;
    char          buf[32];

    if (!g_hListView || !db) return;

    ListView_DeleteAllItems(g_hListView);

    if (sqlite3_prepare_v2(db,
            "SELECT cl.compound_name, cl.cost_per_gram, "
            "       ci.stock_grams, ci.reorder_threshold_grams, ci.last_updated "
            "FROM compound_inventory ci "
            "JOIN compound_library cl ON cl.id = ci.compound_library_id "
            "ORDER BY cl.compound_name;",
            -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name    = (const char*)sqlite3_column_text(stmt, 0);
        double      cpg     = sqlite3_column_double(stmt, 1);
        double      stock   = sqlite3_column_double(stmt, 2);
        double      reorder = sqlite3_column_double(stmt, 3);
        const char* updated = (const char*)sqlite3_column_text(stmt, 4);
        const char* status;

        if (stock <= 0.0)
            status = "OUT";
        else if (stock <= reorder)
            status = "LOW";
        else
            status = "OK";

        LV_InsertRow(g_hListView, row, name ? name : "");

        sprintf(buf, "%.4f", stock);   LV_SetCell(g_hListView, row, 1, buf);
        sprintf(buf, "%.4f", reorder); LV_SetCell(g_hListView, row, 2, buf);
        sprintf(buf, "%.4f", cpg);     LV_SetCell(g_hListView, row, 3, buf);
        LV_SetCell(g_hListView, row, 4, status);
        LV_SetCell(g_hListView, row, 5, updated ? updated : "");

        row++;
    }

    sqlite3_finalize(stmt);
}
