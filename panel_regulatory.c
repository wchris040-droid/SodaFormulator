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
static HWND g_hBtnAdd   = NULL;

/* Dialog state */
static BOOL g_dlgDone;
static BOOL g_dlgSaved;
static HWND g_hRegDlg = NULL;

#define TOP_BAR_H 36

/* Dialog control IDs (local) */
#define IDC_REG_CPD_COMBO  4001
#define IDC_REG_SOURCE     4002
#define IDC_REG_PPM        4003
#define IDC_REG_DATE       4004
#define IDC_REG_NOTES      4005
#define IDC_REG_SAVE       4006
#define IDC_REG_CANCEL     4007
#define IDC_REG_CUR_LBL    4008

/* =========================================================================
   ListView helpers
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

static int LV_InsertRow(HWND hLV, int row, const char* col0, LPARAM lp)
{
    LVITEM lvi;
    ZeroMemory(&lvi, sizeof(lvi));
    lvi.mask     = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem    = row;
    lvi.iSubItem = 0;
    lvi.pszText  = (char*)col0;
    lvi.lParam   = lp;
    return ListView_InsertItem(hLV, &lvi);
}

static void LV_SetCell(HWND hLV, int row, int col, const char* text)
{
    ListView_SetItemText(hLV, row, col, (char*)text);
}

/* =========================================================================
   Add Override dialog WndProc
   ========================================================================= */
static LRESULT CALLBACK RegLimitDlgWndProc(HWND hWnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        SYSTEMTIME st;
        char       dateStr[16];
        sqlite3*      db   = db_get_handle();
        sqlite3_stmt* stmt = NULL;
        HWND hCombo, hSrc, hPpm, hDate, hNotes, hCurLbl;

        /* Compound label + combobox */
        CreateWindowEx(0, "STATIC", "Compound:",
            WS_CHILD | WS_VISIBLE,
            10, 10, 80, 18, hWnd, NULL, g_hInst, NULL);

        hCombo = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            95, 8, 200, 200,
            hWnd, (HMENU)(INT_PTR)IDC_REG_CPD_COMBO, g_hInst, NULL);

        /* Populate compound names from library */
        if (db && sqlite3_prepare_v2(db,
                "SELECT compound_name FROM compound_library "
                "ORDER BY compound_name ASC;",
                -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* name = (const char*)sqlite3_column_text(stmt, 0);
                SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)name);
            }
            sqlite3_finalize(stmt);
        }
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);

        /* Current active limit label */
        CreateWindowEx(0, "STATIC", "Current active limit:",
            WS_CHILD | WS_VISIBLE,
            10, 38, 130, 18, hWnd, NULL, g_hInst, NULL);

        hCurLbl = CreateWindowEx(0, "STATIC", "(select compound)",
            WS_CHILD | WS_VISIBLE,
            145, 38, 200, 18,
            hWnd, (HMENU)(INT_PTR)IDC_REG_CUR_LBL, g_hInst, NULL);

        /* Source */
        CreateWindowEx(0, "STATIC", "Source:",
            WS_CHILD | WS_VISIBLE,
            10, 64, 80, 18, hWnd, NULL, g_hInst, NULL);

        hSrc = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            95, 62, 200, 20,
            hWnd, (HMENU)(INT_PTR)IDC_REG_SOURCE, g_hInst, NULL);
        SetWindowText(hSrc, "e.g. FEMA 29");

        /* New max ppm */
        CreateWindowEx(0, "STATIC", "New max ppm:",
            WS_CHILD | WS_VISIBLE,
            10, 92, 80, 18, hWnd, NULL, g_hInst, NULL);

        hPpm = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            95, 90, 100, 20,
            hWnd, (HMENU)(INT_PTR)IDC_REG_PPM, g_hInst, NULL);

        /* Effective date */
        CreateWindowEx(0, "STATIC", "Eff. Date:",
            WS_CHILD | WS_VISIBLE,
            10, 120, 80, 18, hWnd, NULL, g_hInst, NULL);

        hDate = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            95, 118, 120, 20,
            hWnd, (HMENU)(INT_PTR)IDC_REG_DATE, g_hInst, NULL);

        /* Pre-fill today's date */
        GetLocalTime(&st);
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
                 st.wYear, st.wMonth, st.wDay);
        SetWindowText(hDate, dateStr);

        /* Notes */
        CreateWindowEx(0, "STATIC", "Notes (opt):",
            WS_CHILD | WS_VISIBLE,
            10, 148, 80, 18, hWnd, NULL, g_hInst, NULL);

        hNotes = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            95, 146, 290, 20,
            hWnd, (HMENU)(INT_PTR)IDC_REG_NOTES, g_hInst, NULL);

        /* Buttons */
        CreateWindowEx(0, "BUTTON", "Save",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            95, 180, 80, 26,
            hWnd, (HMENU)(INT_PTR)IDC_REG_SAVE, g_hInst, NULL);

        CreateWindowEx(0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE,
            185, 180, 80, 26,
            hWnd, (HMENU)(INT_PTR)IDC_REG_CANCEL, g_hInst, NULL);

        (void)hSrc; (void)hPpm; (void)hDate; (void)hNotes;
        (void)hCurLbl;

        /* Trigger CBN_SELCHANGE to pre-fill current ppm for first item */
        SendMessage(hWnd, WM_COMMAND,
            MAKEWPARAM(IDC_REG_CPD_COMBO, CBN_SELCHANGE),
            (LPARAM)hCombo);
    }
    return 0;

    case WM_COMMAND:
    {
        int ctl = LOWORD(wParam);
        int ntf = HIWORD(wParam);

        if (ctl == IDC_REG_CPD_COMBO && ntf == CBN_SELCHANGE) {
            /* Pre-fill current active ppm */
            HWND hCombo = GetDlgItem(hWnd, IDC_REG_CPD_COMBO);
            HWND hPpm   = GetDlgItem(hWnd, IDC_REG_PPM);
            HWND hLbl   = GetDlgItem(hWnd, IDC_REG_CUR_LBL);
            char name[128];
            float cur_max = 0.0f;
            int idx = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (idx != CB_ERR) {
                SendMessage(hCombo, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)name);
                int src = db_get_active_limit(name, &cur_max);
                if (src >= 0 && cur_max > 0.0f) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.2f ppm (%s)",
                             cur_max,
                             src == 0 ? "override" : "library");
                    SetWindowText(hLbl, buf);
                    snprintf(buf, sizeof(buf), "%.2f", cur_max);
                    SetWindowText(hPpm, buf);
                } else {
                    SetWindowText(hLbl, "(not found in library)");
                    SetWindowText(hPpm, "");
                }
            }
            return 0;
        }

        if (ctl == IDC_REG_SAVE) {
            char name[128], source[128], ppmStr[32], date[16], notes[256];
            HWND hCombo = GetDlgItem(hWnd, IDC_REG_CPD_COMBO);
            int  idx    = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);

            if (idx == CB_ERR) {
                MessageBox(hWnd, "Select a compound.", "Error", MB_ICONWARNING);
                return 0;
            }
            SendMessage(hCombo, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)name);

            GetWindowText(GetDlgItem(hWnd, IDC_REG_SOURCE), source, sizeof(source));
            GetWindowText(GetDlgItem(hWnd, IDC_REG_PPM),    ppmStr, sizeof(ppmStr));
            GetWindowText(GetDlgItem(hWnd, IDC_REG_DATE),   date,   sizeof(date));
            GetWindowText(GetDlgItem(hWnd, IDC_REG_NOTES),  notes,  sizeof(notes));

            if (!source[0]) {
                MessageBox(hWnd, "Source is required.", "Error", MB_ICONWARNING);
                return 0;
            }
            float ppm = (float)atof(ppmStr);
            if (ppm <= 0.0f) {
                MessageBox(hWnd, "Max ppm must be greater than 0.", "Error",
                           MB_ICONWARNING);
                return 0;
            }
            if (!date[0]) {
                MessageBox(hWnd, "Effective date is required.", "Error",
                           MB_ICONWARNING);
                return 0;
            }

            if (db_add_regulatory_limit(name, source, ppm, date,
                                        notes[0] ? notes : NULL) != 0) {
                MessageBox(hWnd, "Failed to save regulatory limit.",
                           "Error", MB_ICONERROR);
                return 0;
            }

            g_dlgSaved = TRUE;
            g_dlgDone  = TRUE;
            EnableWindow(GetParent(hWnd), TRUE);
            DestroyWindow(hWnd);
            return 0;
        }

        if (ctl == IDC_REG_CANCEL) {
            g_dlgDone = TRUE;
            EnableWindow(GetParent(hWnd), TRUE);
            DestroyWindow(hWnd);
            return 0;
        }
    }
    return 0;

    case WM_DESTROY:
        g_hRegDlg = NULL;
        g_dlgDone = TRUE;
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   Regulatory panel WndProc
   ========================================================================= */
static LRESULT CALLBACK RegPanelWndProc(HWND hWnd, UINT msg,
                                         WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);

        /* Top bar: Add Override button */
        g_hBtnAdd = CreateWindowEx(0, "BUTTON", "Add Override",
            WS_CHILD | WS_VISIBLE,
            4, 6, 100, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_ADD_LIMIT, g_hInst, NULL);

        /* ListView */
        g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, TOP_BAR_H,
            rc.right, rc.bottom - TOP_BAR_H,
            hWnd, (HMENU)(INT_PTR)IDC_LISTVIEW, g_hInst, NULL);

        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LV_AddCol(g_hListView, 0, "Compound",     150);
        LV_AddCol(g_hListView, 1, "Source",         80);
        LV_AddCol(g_hListView, 2, "Override ppm",   90);
        LV_AddCol(g_hListView, 3, "Lib ppm",         75);
        LV_AddCol(g_hListView, 4, "Effective Date",  95);
        LV_AddCol(g_hListView, 5, "Status",           85);
        LV_AddCol(g_hListView, 6, "Notes",           200);
    }
    return 0;

    case WM_SIZE:
    {
        WORD w = LOWORD(lParam);
        WORD h = HIWORD(lParam);
        if (g_hListView)
            MoveWindow(g_hListView, 0, TOP_BAR_H,
                       w, h - TOP_BAR_H, TRUE);
    }
    return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_ADD_LIMIT) {
            /* Register dialog class if first time */
            static BOOL s_registered = FALSE;
            if (!s_registered) {
                WNDCLASSEX wc;
                ZeroMemory(&wc, sizeof(wc));
                wc.cbSize        = sizeof(WNDCLASSEX);
                wc.lpfnWndProc   = RegLimitDlgWndProc;
                wc.hInstance     = g_hInst;
                wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
                wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
                wc.lpszClassName = "RegLimitDlgClass";
                RegisterClassEx(&wc);
                s_registered = TRUE;
            }

            /* Open modal dialog */
            g_dlgDone  = FALSE;
            g_dlgSaved = FALSE;
            EnableWindow(hWnd, FALSE);

            g_hRegDlg = CreateWindowEx(
                WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
                "RegLimitDlgClass",
                "Add Regulatory Override",
                WS_POPUP | WS_CAPTION | WS_SYSMENU,
                CW_USEDEFAULT, CW_USEDEFAULT,
                420, 220,
                hWnd, NULL, g_hInst, NULL);

            ShowWindow(g_hRegDlg, SW_SHOW);
            UpdateWindow(g_hRegDlg);

            /* Modal loop */
            {
                MSG m;
                while (!g_dlgDone && GetMessage(&m, NULL, 0, 0)) {
                    if (!IsDialogMessage(g_hRegDlg, &m)) {
                        TranslateMessage(&m);
                        DispatchMessage(&m);
                    }
                }
            }

            EnableWindow(hWnd, TRUE);
            SetFocus(hWnd);

            if (g_dlgSaved)
                Panel_Regulatory_Refresh();
        }
        return 0;

    case WM_NOTIFY:
    {
        LPNMHDR pnm = (LPNMHDR)lParam;
        if (pnm->hwndFrom == g_hListView &&
            pnm->code == NM_CUSTOMDRAW)
        {
            LPNMLVCUSTOMDRAW pcd = (LPNMLVCUSTOMDRAW)lParam;
            if (pcd->nmcd.dwDrawStage == CDDS_PREPAINT)
                return CDRF_NOTIFYITEMDRAW;
            if (pcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                /* lParam stores is_active: 0 = Superseded, 1 = Active */
                if (pcd->nmcd.lItemlParam == 0) {
                    pcd->clrText = RGB(160, 160, 160);
                    return CDRF_NEWFONT;
                }
            }
        }
    }
    return CDRF_DODEFAULT;

    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   Panel_Regulatory_Refresh
   ========================================================================= */
void Panel_Regulatory_Refresh(void)
{
    sqlite3*      db   = db_get_handle();
    sqlite3_stmt* stmt = NULL;
    int           row  = 0;

    if (!g_hListView || !db) return;

    ListView_DeleteAllItems(g_hListView);

    if (sqlite3_prepare_v2(db,
        "SELECT rl.compound_name, rl.source, rl.max_use_ppm, "
        "       COALESCE(cl.max_use_ppm, 0.0), "
        "       rl.effective_date, "
        "       CASE WHEN rl.id = ("
        "           SELECT id FROM regulatory_limits r2 "
        "           WHERE r2.compound_name = rl.compound_name "
        "           ORDER BY r2.effective_date DESC, r2.id DESC LIMIT 1"
        "       ) THEN 1 ELSE 0 END AS is_active, "
        "       COALESCE(rl.notes, '') "
        "FROM regulatory_limits rl "
        "LEFT JOIN compound_library cl "
        "       ON cl.compound_name = rl.compound_name "
        "ORDER BY rl.compound_name ASC, rl.effective_date DESC, rl.id DESC;",
        -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* cpd      = (const char*)sqlite3_column_text(stmt, 0);
        const char* src      = (const char*)sqlite3_column_text(stmt, 1);
        double      ovr_ppm  = sqlite3_column_double(stmt, 2);
        double      lib_ppm  = sqlite3_column_double(stmt, 3);
        const char* eff_date = (const char*)sqlite3_column_text(stmt, 4);
        int         is_act   = sqlite3_column_int(stmt, 5);
        const char* notes    = (const char*)sqlite3_column_text(stmt, 6);

        char ovr_buf[32], lib_buf[32];
        snprintf(ovr_buf, sizeof(ovr_buf), "%.2f", ovr_ppm);
        if (lib_ppm > 0.0)
            snprintf(lib_buf, sizeof(lib_buf), "%.2f", lib_ppm);
        else
            strncpy(lib_buf, "--", sizeof(lib_buf));

        int actual = LV_InsertRow(g_hListView, row, cpd ? cpd : "", (LPARAM)is_act);
        LV_SetCell(g_hListView, actual, 1, src      ? src      : "");
        LV_SetCell(g_hListView, actual, 2, ovr_buf);
        LV_SetCell(g_hListView, actual, 3, lib_buf);
        LV_SetCell(g_hListView, actual, 4, eff_date ? eff_date : "");
        LV_SetCell(g_hListView, actual, 5, is_act   ? "Active" : "Superseded");
        LV_SetCell(g_hListView, actual, 6, notes    ? notes    : "");

        row++;
    }

    sqlite3_finalize(stmt);
}

/* =========================================================================
   Panel_Regulatory_Create
   ========================================================================= */
HWND Panel_Regulatory_Create(HWND hParent)
{
    static BOOL s_registered = FALSE;

    if (!s_registered) {
        WNDCLASSEX wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = RegPanelWndProc;
        wc.hInstance     = g_hInst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "RegulatoryPanel";
        RegisterClassEx(&wc);
        s_registered = TRUE;
    }

    g_hPanel = CreateWindowEx(0, "RegulatoryPanel", NULL,
        WS_CHILD,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}
