#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "database.h"
#include "tasting.h"
#include "sqlite3.h"

/* =========================================================================
   File-scope state
   ========================================================================= */
static HWND g_hPanel      = NULL;
static HWND g_hListView   = NULL;
static HWND g_hFilterCombo = NULL;
static HWND g_hBtnNew;

/* Dialog state */
static BOOL g_dlgDone;
static BOOL g_dlgSaved;
static HWND g_hTastingDlg = NULL;

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
   Populate flavor filter combo from DB
   ========================================================================= */
static void PopulateFlavorCombo(HWND hCombo)
{
    sqlite3*      db   = db_get_handle();
    sqlite3_stmt* stmt = NULL;
    int           prev;

    if (!db || !hCombo) return;

    prev = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"(All)");

    if (sqlite3_prepare_v2(db,
            "SELECT DISTINCT flavor_code FROM formulations ORDER BY flavor_code;",
            -1, &stmt, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* code = (const char*)sqlite3_column_text(stmt, 0);
            if (code) SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)code);
        }
        sqlite3_finalize(stmt);
    }

    SendMessage(hCombo, CB_SETCURSEL, (prev >= 0 ? prev : 0), 0);
}

/* =========================================================================
   Populate version combo for a given flavor in the dialog
   ========================================================================= */
static void DlgPopulateVersionCombo(HWND hVerCombo, const char* flavor)
{
    sqlite3*      db   = db_get_handle();
    sqlite3_stmt* stmt = NULL;

    if (!db || !hVerCombo || !flavor || !flavor[0]) return;

    SendMessage(hVerCombo, CB_RESETCONTENT, 0, 0);

    if (sqlite3_prepare_v2(db,
            "SELECT ver_major, ver_minor, ver_patch FROM formulations "
            "WHERE flavor_code=? ORDER BY ver_major, ver_minor, ver_patch;",
            -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, flavor, -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            char buf[16];
            sprintf(buf, "%d.%d.%d",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2));
            SendMessage(hVerCombo, CB_ADDSTRING, 0, (LPARAM)buf);
        }
        sqlite3_finalize(stmt);
    }

    SendMessage(hVerCombo, CB_SETCURSEL, 0, 0);
}

/* =========================================================================
   Tasting dialog WndProc
   ========================================================================= */
static LRESULT CALLBACK TastingDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        HWND hFlvCombo, hVerCombo;
        int  y  = 10;
        int  lx = 10, lw = 100, ex = 115, ew = 200, rh = 22;

        /* Flavor ComboBox */
        CreateWindowEx(0, "STATIC", "Flavor:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        hFlvCombo = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            ex, y, ew, 200, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_FLV_COMBO, g_hInst, NULL);
        PopulateFlavorCombo(hFlvCombo);
        /* Remove "(All)" item — only real flavors */
        SendMessage(hFlvCombo, CB_DELETESTRING, 0, 0);
        SendMessage(hFlvCombo, CB_SETCURSEL, 0, 0);
        y += 32;

        /* Version ComboBox */
        CreateWindowEx(0, "STATIC", "Version:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        hVerCombo = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            ex, y, 100, 200, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_VER_COMBO, g_hInst, NULL);
        {
            char flvBuf[MAX_FLAVOR_CODE];
            int  flvSel = (int)SendMessage(hFlvCombo, CB_GETCURSEL, 0, 0);
            if (flvSel >= 0) {
                SendMessage(hFlvCombo, CB_GETLBTEXT, flvSel, (LPARAM)flvBuf);
                DlgPopulateVersionCombo(hVerCombo, flvBuf);
            }
        }
        y += 32;

        /* Taster */
        CreateWindowEx(0, "STATIC", "Taster:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            ex, y, ew, rh, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_TASTER, g_hInst, NULL);
        y += 32;

        /* Scores */
        {
            struct { const char* label; int id; } scores[] = {
                { "Overall (req):", IDC_DLG_OVERALL   },
                { "Aroma:",         IDC_DLG_AROMA     },
                { "Flavor:",        IDC_DLG_FLAVOR    },
                { "Mouthfeel:",     IDC_DLG_MOUTHFEEL },
                { "Finish:",        IDC_DLG_FINISH    },
                { "Sweetness:",     IDC_DLG_SWEETNESS },
            };
            int i;
            for (i = 0; i < 6; i++) {
                CreateWindowEx(0, "STATIC", scores[i].label,
                    WS_CHILD | WS_VISIBLE | SS_RIGHT,
                    lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
                CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT",
                    (i == 0) ? "7.0" : "",
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    ex, y, 60, rh, hWnd,
                    (HMENU)(INT_PTR)(scores[i].id), g_hInst, NULL);
                if (i > 0) {
                    CreateWindowEx(0, "STATIC", "(optional)",
                        WS_CHILD | WS_VISIBLE,
                        ex + 65, y + 3, 70, 18, hWnd, NULL, g_hInst, NULL);
                }
                y += 30;
            }
        }

        /* Notes */
        y += 4;
        CreateWindowEx(0, "STATIC", "Notes:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
            ex, y, 330, 80, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_NOTES, g_hInst, NULL);
        y += 90;

        /* Save / Cancel */
        CreateWindowEx(0, "BUTTON", "Save",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, y, 90, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_SAVE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            110, y, 90, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_CANCEL, g_hInst, NULL);
    }
    return 0;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int notify = HIWORD(wParam);

        /* When flavor changes, refresh version combo */
        if (id == IDC_DLG_FLV_COMBO && notify == CBN_SELCHANGE) {
            char flvBuf[MAX_FLAVOR_CODE];
            HWND hFlvCombo = GetDlgItem(hWnd, IDC_DLG_FLV_COMBO);
            HWND hVerCombo = GetDlgItem(hWnd, IDC_DLG_VER_COMBO);
            int  flvSel    = (int)SendMessage(hFlvCombo, CB_GETCURSEL, 0, 0);
            if (flvSel >= 0) {
                SendMessage(hFlvCombo, CB_GETLBTEXT, flvSel, (LPARAM)flvBuf);
                DlgPopulateVersionCombo(hVerCombo, flvBuf);
            }
            break;
        }

        if (id == IDC_BTN_CANCEL) {
            g_dlgDone = TRUE;
            DestroyWindow(hWnd);
            break;
        }

        if (id == IDC_BTN_SAVE) {
            char flvBuf[MAX_FLAVOR_CODE];
            char verBuf[16];
            char taster[MAX_TASTER_NAME];
            char noteBuf[MAX_TASTING_NOTES];
            char scoreBuf[16];
            TastingSession ts;
            int   major, minor, patch;
            HWND  hFlvCombo, hVerCombo;
            int   flvSel, verSel;

            hFlvCombo = GetDlgItem(hWnd, IDC_DLG_FLV_COMBO);
            hVerCombo = GetDlgItem(hWnd, IDC_DLG_VER_COMBO);

            flvSel = (int)SendMessage(hFlvCombo, CB_GETCURSEL, 0, 0);
            verSel = (int)SendMessage(hVerCombo, CB_GETCURSEL, 0, 0);

            if (flvSel < 0 || verSel < 0) {
                MessageBox(hWnd, "Select a flavor and version.", "Input", MB_OK);
                break;
            }

            SendMessage(hFlvCombo, CB_GETLBTEXT, flvSel, (LPARAM)flvBuf);
            SendMessage(hVerCombo, CB_GETLBTEXT, verSel, (LPARAM)verBuf);
            if (sscanf(verBuf, "%d.%d.%d", &major, &minor, &patch) != 3) {
                MessageBox(hWnd, "Invalid version.", "Input", MB_OK);
                break;
            }

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_TASTER), taster, sizeof(taster));
            if (!taster[0]) strncpy(taster, "unknown", sizeof(taster) - 1);

            tasting_create(&ts, 0, taster);

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_OVERALL), scoreBuf, sizeof(scoreBuf));
            ts.overall_score = (scoreBuf[0]) ? (float)atof(scoreBuf) : 5.0f;

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_AROMA),    scoreBuf, sizeof(scoreBuf));
            ts.aroma_score    = (scoreBuf[0]) ? (float)atof(scoreBuf) : -1.0f;

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_FLAVOR),   scoreBuf, sizeof(scoreBuf));
            ts.flavor_score   = (scoreBuf[0]) ? (float)atof(scoreBuf) : -1.0f;

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_MOUTHFEEL),scoreBuf, sizeof(scoreBuf));
            ts.mouthfeel_score = (scoreBuf[0]) ? (float)atof(scoreBuf) : -1.0f;

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_FINISH),   scoreBuf, sizeof(scoreBuf));
            ts.finish_score   = (scoreBuf[0]) ? (float)atof(scoreBuf) : -1.0f;

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_SWEETNESS),scoreBuf, sizeof(scoreBuf));
            ts.sweetness_score = (scoreBuf[0]) ? (float)atof(scoreBuf) : -1.0f;

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_NOTES), noteBuf, sizeof(noteBuf));
            strncpy(ts.notes, noteBuf, MAX_TASTING_NOTES - 1);

            if (db_save_tasting(flvBuf, major, minor, patch, &ts) == 0) {
                g_dlgSaved = TRUE;
                g_dlgDone  = TRUE;
                DestroyWindow(hWnd);
            } else {
                MessageBox(hWnd, "Failed to save tasting session.", "Error", MB_ICONERROR);
            }
        }
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
   OpenTastingDialog
   ========================================================================= */
static void OpenTastingDialog(HWND hParent)
{
    RECT pr;
    int  dw = 520, dh = 520;
    int  cx, cy;
    MSG  msg;

    GetWindowRect(hParent, &pr);
    cx = pr.left + (pr.right  - pr.left - dw) / 2;
    cy = pr.top  + (pr.bottom - pr.top  - dh) / 2;

    g_dlgDone  = FALSE;
    g_dlgSaved = FALSE;

    g_hTastingDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "TastingDlgClass",
        "New Tasting Session",
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
    if (IsWindow(g_hTastingDlg)) DestroyWindow(g_hTastingDlg);
    g_hTastingDlg = NULL;
    SetForegroundWindow(hParent);
}

/* =========================================================================
   Panel WndProc
   ========================================================================= */
static LRESULT CALLBACK TastingPanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        int bx = 4, by = 4;

        /* Filter combo */
        CreateWindowEx(0, "STATIC", "Flavor:",
            WS_CHILD | WS_VISIBLE,
            bx, by + 4, 50, 18, hWnd, NULL, g_hInst, NULL);
        g_hFilterCombo = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            bx + 55, by, 160, 200, hWnd,
            (HMENU)(INT_PTR)IDC_FILTER_COMBO, g_hInst, NULL);

        g_hBtnNew = CreateWindowEx(0, "BUTTON", "New Session",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx + 222, by, 100, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_NEW_SESSION, g_hInst, NULL);

        /* ListView */
        g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE |
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 36, 100, 100, hWnd,
            (HMENU)(INT_PTR)IDC_LISTVIEW, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LV_AddCol(g_hListView, 0, "Flavor",    90);
        LV_AddCol(g_hListView, 1, "Version",   70);
        LV_AddCol(g_hListView, 2, "Taster",   110);
        LV_AddCol(g_hListView, 3, "Date",      150);
        LV_AddCol(g_hListView, 4, "Overall",   65);
        LV_AddCol(g_hListView, 5, "Aroma",     65);
        LV_AddCol(g_hListView, 6, "Flavor",    65);
        LV_AddCol(g_hListView, 7, "Mouthfeel", 75);
        LV_AddCol(g_hListView, 8, "Finish",    65);
        LV_AddCol(g_hListView, 9, "Sweetness", 75);
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
        int id     = LOWORD(wParam);
        int notify = HIWORD(wParam);

        if (id == IDC_FILTER_COMBO && notify == CBN_SELCHANGE) {
            Panel_Tasting_Refresh();
            break;
        }
        if (id == IDC_BTN_NEW_SESSION) {
            OpenTastingDialog(hWnd);
            if (g_dlgSaved) Panel_Tasting_Refresh();
        }
    }
    return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   Panel_Tasting_Create
   ========================================================================= */
HWND Panel_Tasting_Create(HWND hParent)
{
    WNDCLASSEX wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = TastingPanelWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "TastingPanel";
    RegisterClassEx(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = TastingDlgWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "TastingDlgClass";
    RegisterClassEx(&wc);

    g_hPanel = CreateWindowEx(0,
        "TastingPanel", NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}

/* =========================================================================
   Panel_Tasting_Refresh
   ========================================================================= */
void Panel_Tasting_Refresh(void)
{
    sqlite3*      db    = db_get_handle();
    sqlite3_stmt* stmt  = NULL;
    char          filter[MAX_FLAVOR_CODE];
    int           fsel;
    int           row   = 0;
    char          buf[32];
    char          sql[512];

    if (!g_hListView || !db) return;

    PopulateFlavorCombo(g_hFilterCombo);

    ListView_DeleteAllItems(g_hListView);

    fsel = (int)SendMessage(g_hFilterCombo, CB_GETCURSEL, 0, 0);
    if (fsel > 0) {
        SendMessage(g_hFilterCombo, CB_GETLBTEXT, fsel, (LPARAM)filter);
    } else {
        filter[0] = '\0';
    }

    if (filter[0]) {
        sprintf(sql,
            "SELECT f.flavor_code, f.ver_major, f.ver_minor, f.ver_patch, "
            "       t.taster, t.tasted_at, "
            "       t.overall_score, t.aroma_score, t.flavor_score, "
            "       t.mouthfeel_score, t.finish_score, t.sweetness_score "
            "FROM tasting_sessions t "
            "JOIN formulations f ON f.id = t.formulation_id "
            "WHERE f.flavor_code = '%s' "
            "ORDER BY t.tasted_at ASC;", filter);
    } else {
        sprintf(sql,
            "SELECT f.flavor_code, f.ver_major, f.ver_minor, f.ver_patch, "
            "       t.taster, t.tasted_at, "
            "       t.overall_score, t.aroma_score, t.flavor_score, "
            "       t.mouthfeel_score, t.finish_score, t.sweetness_score "
            "FROM tasting_sessions t "
            "JOIN formulations f ON f.id = t.formulation_id "
            "ORDER BY t.tasted_at ASC;");
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* code = (const char*)sqlite3_column_text(stmt, 0);

        LV_InsertRow(g_hListView, row, code ? code : "");

        sprintf(buf, "%d.%d.%d",
            sqlite3_column_int(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3));
        LV_SetCell(g_hListView, row, 1, buf);

        {
            const char* t = (const char*)sqlite3_column_text(stmt, 4);
            LV_SetCell(g_hListView, row, 2, t ? t : "unknown");
        }
        {
            const char* d = (const char*)sqlite3_column_text(stmt, 5);
            LV_SetCell(g_hListView, row, 3, d ? d : "");
        }

        /* Scores */
        {
            int i;
            int colids[] = { 4, 5, 6, 7, 8, 9 };
            for (i = 0; i < 6; i++) {
                if (sqlite3_column_type(stmt, 6 + i) == SQLITE_NULL) {
                    LV_SetCell(g_hListView, row, colids[i], "--");
                } else {
                    sprintf(buf, "%.1f", sqlite3_column_double(stmt, 6 + i));
                    LV_SetCell(g_hListView, row, colids[i], buf);
                }
            }
        }

        row++;
    }

    sqlite3_finalize(stmt);
}
