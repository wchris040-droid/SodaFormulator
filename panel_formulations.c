#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "database.h"
#include "formulation.h"
#include "version.h"
#include "sqlite3.h"

/* =========================================================================
   File-scope state
   ========================================================================= */
static HWND g_hPanel    = NULL;
static HWND g_hListView = NULL;
static HWND g_hBtnNew, g_hBtnEdit, g_hBtnDelete, g_hBtnHistory, g_hBtnValidate;

/* Dialog state */
static BOOL        g_dlgDone;
static BOOL        g_dlgSaved;

typedef struct {
    BOOL        is_edit;
    Formulation form;           /* populated by caller for edit, or zeroed for new */
} FormDlgData;

static FormDlgData g_dlgData;
static HWND        g_hFormDlg   = NULL;
static HWND        g_hDlgCpdLV  = NULL; /* compound ListView inside the dialog */

/* =========================================================================
   Helper: add a column to a ListView
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

/* =========================================================================
   Helper: set a cell in a ListView
   ========================================================================= */
static void LV_SetCell(HWND hLV, int row, int col, const char* text)
{
    ListView_SetItemText(hLV, row, col, (char*)text);
}

/* =========================================================================
   Helper: insert a blank row
   ========================================================================= */
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

/* =========================================================================
   Refresh the dialog's compound ListView from g_dlgData.form
   ========================================================================= */
static void DlgRefreshCpdList(void)
{
    int i;
    char buf[32];

    if (!g_hDlgCpdLV) return;

    ListView_DeleteAllItems(g_hDlgCpdLV);

    for (i = 0; i < g_dlgData.form.compound_count; i++) {
        LV_InsertRow(g_hDlgCpdLV, i,
                     g_dlgData.form.compounds[i].compound_name);
        sprintf(buf, "%.2f", g_dlgData.form.compounds[i].concentration_ppm);
        LV_SetCell(g_hDlgCpdLV, i, 1, buf);
    }
}

/* =========================================================================
   Populate compound ComboBox from compound_library
   ========================================================================= */
static void DlgPopulateCpdCombo(HWND hCombo)
{
    sqlite3*      db   = db_get_handle();
    sqlite3_stmt* stmt = NULL;

    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);

    if (!db) return;

    if (sqlite3_prepare_v2(db,
            "SELECT compound_name FROM compound_library ORDER BY compound_name;",
            -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)name);
    }
    sqlite3_finalize(stmt);

    SendMessage(hCombo, CB_SETCURSEL, 0, 0);
}

/* =========================================================================
   Dialog WndProc
   ========================================================================= */
static LRESULT CALLBACK FormDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        int y = 10;
        int lx = 10, lw = 90, ex = 105, ew = 210, rh = 22;
        HWND hCode, hName, hPh, hBrix;

        /* Code */
        CreateWindowEx(0, "STATIC", "Flavor Code:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 2, lw, 18, hWnd, NULL, g_hInst, NULL);
        hCode = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", g_dlgData.form.flavor_code,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            ex, y, ew, rh, hWnd, (HMENU)(INT_PTR)IDC_DLG_CODE, g_hInst, NULL);
        if (g_dlgData.is_edit)
            SendMessage(hCode, EM_SETREADONLY, TRUE, 0);
        y += 32;

        /* Name */
        CreateWindowEx(0, "STATIC", "Flavor Name:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 2, lw, 18, hWnd, NULL, g_hInst, NULL);
        hName = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", g_dlgData.form.flavor_name,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            ex, y, ew, rh, hWnd, (HMENU)(INT_PTR)IDC_DLG_NAME, g_hInst, NULL);
        (void)hName;
        y += 32;

        /* pH */
        {
            char buf[16];
            sprintf(buf, "%.2f", g_dlgData.form.target_ph);
            CreateWindowEx(0, "STATIC", "Target pH:",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                lx, y + 2, lw, 18, hWnd, NULL, g_hInst, NULL);
            hPh = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", buf,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                ex, y, 80, rh, hWnd, (HMENU)(INT_PTR)IDC_DLG_PH, g_hInst, NULL);
            (void)hPh;
            y += 32;
        }

        /* Brix */
        {
            char buf[16];
            sprintf(buf, "%.2f", g_dlgData.form.target_brix);
            CreateWindowEx(0, "STATIC", "Target Brix:",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                lx, y + 2, lw, 18, hWnd, NULL, g_hInst, NULL);
            hBrix = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", buf,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                ex, y, 80, rh, hWnd, (HMENU)(INT_PTR)IDC_DLG_BRIX, g_hInst, NULL);
            (void)hBrix;
            y += 36;
        }

        /* Compounds label */
        CreateWindowEx(0, "STATIC", "Compounds:",
            WS_CHILD | WS_VISIBLE,
            lx, y, 100, 18, hWnd, NULL, g_hInst, NULL);
        y += 20;

        /* Compound ListView */
        g_hDlgCpdLV = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            lx, y, 520, 160, hWnd, (HMENU)(INT_PTR)IDC_DLG_CPD_LIST,
            g_hInst, NULL);
        LV_AddCol(g_hDlgCpdLV, 0, "Compound Name", 260);
        LV_AddCol(g_hDlgCpdLV, 1, "ppm",           120);
        DlgRefreshCpdList();
        y += 168;

        /* Add row */
        {
            HWND hCombo;
            CreateWindowEx(0, "STATIC", "Add:",
                WS_CHILD | WS_VISIBLE,
                lx, y + 3, 30, 18, hWnd, NULL, g_hInst, NULL);
            hCombo = CreateWindowEx(0, "COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                lx + 35, y, 200, 200, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_CPD_COMBO, g_hInst, NULL);
            DlgPopulateCpdCombo(hCombo);

            CreateWindowEx(0, "STATIC", "ppm:",
                WS_CHILD | WS_VISIBLE,
                lx + 242, y + 3, 30, 18, hWnd, NULL, g_hInst, NULL);
            CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0.0",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                lx + 277, y, 70, rh, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_CPD_PPM, g_hInst, NULL);
            CreateWindowEx(0, "BUTTON", "Add",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                lx + 353, y, 60, rh, hWnd,
                (HMENU)(INT_PTR)IDC_BTN_ADD_CPD, g_hInst, NULL);
        }
        y += 30;

        /* Remove Selected */
        CreateWindowEx(0, "BUTTON", "Remove Selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx, y, 120, 25, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_REMOVE_CPD, g_hInst, NULL);
        y += 35;

        /* Save / Cancel */
        CreateWindowEx(0, "BUTTON", "Save",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx, y, 90, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_SAVE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx + 100, y, 90, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_CANCEL, g_hInst, NULL);
    }
    return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {

        case IDC_BTN_ADD_CPD:
        {
            char name[128], ppmStr[32];
            float ppm;
            HWND hCombo, hPpmEdit;

            if (g_dlgData.form.compound_count >= MAX_COMPOUNDS) {
                MessageBox(hWnd, "Maximum compounds reached.", "Limit", MB_OK);
                break;
            }

            hCombo   = GetDlgItem(hWnd, IDC_DLG_CPD_COMBO);
            hPpmEdit = GetDlgItem(hWnd, IDC_DLG_CPD_PPM);

            {
                int sel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                if (sel == CB_ERR) break;
                SendMessage(hCombo, CB_GETLBTEXT, sel, (LPARAM)name);
            }

            GetWindowText(hPpmEdit, ppmStr, sizeof(ppmStr));
            ppm = (float)atof(ppmStr);
            if (ppm <= 0.0f) {
                MessageBox(hWnd, "Enter a positive ppm value.", "Input", MB_OK);
                break;
            }

            strncpy(g_dlgData.form.compounds[g_dlgData.form.compound_count].compound_name,
                    name, 63);
            g_dlgData.form.compounds[g_dlgData.form.compound_count].compound_name[63] = '\0';
            g_dlgData.form.compounds[g_dlgData.form.compound_count].concentration_ppm = ppm;
            g_dlgData.form.compound_count++;

            DlgRefreshCpdList();
            SetWindowText(hPpmEdit, "0.0");
        }
        break;

        case IDC_BTN_REMOVE_CPD:
        {
            int sel = ListView_GetNextItem(g_hDlgCpdLV, -1, LVNI_SELECTED);
            int last;
            if (sel < 0) break;

            last = g_dlgData.form.compound_count - 1;
            if (sel < last)
                g_dlgData.form.compounds[sel] = g_dlgData.form.compounds[last];
            g_dlgData.form.compound_count--;

            DlgRefreshCpdList();
        }
        break;

        case IDC_BTN_SAVE:
        {
            char code[MAX_FLAVOR_CODE];
            char name[MAX_FLAVOR_NAME];
            char phStr[16], brixStr[16];

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_CODE), code, sizeof(code));
            GetWindowText(GetDlgItem(hWnd, IDC_DLG_NAME), name, sizeof(name));
            GetWindowText(GetDlgItem(hWnd, IDC_DLG_PH),   phStr, sizeof(phStr));
            GetWindowText(GetDlgItem(hWnd, IDC_DLG_BRIX), brixStr, sizeof(brixStr));

            if (!code[0] || !name[0]) {
                MessageBox(hWnd, "Flavor Code and Name are required.", "Validation", MB_OK);
                break;
            }

            strncpy(g_dlgData.form.flavor_code, code, MAX_FLAVOR_CODE - 1);
            g_dlgData.form.flavor_code[MAX_FLAVOR_CODE - 1] = '\0';
            strncpy(g_dlgData.form.flavor_name, name, MAX_FLAVOR_NAME - 1);
            g_dlgData.form.flavor_name[MAX_FLAVOR_NAME - 1] = '\0';
            g_dlgData.form.target_ph   = (float)atof(phStr);
            g_dlgData.form.target_brix = (float)atof(brixStr);

            if (g_dlgData.is_edit)
                g_dlgData.form.version =
                    increment_version(g_dlgData.form.version, INCREMENT_PATCH);

            if (db_save_formulation(&g_dlgData.form) != 0) {
                MessageBox(hWnd,
                    "Save failed. Version may already exist or a compound limit was exceeded.",
                    "Error", MB_ICONERROR);
                break;
            }

            g_dlgSaved = TRUE;
            g_dlgDone  = TRUE;
            DestroyWindow(hWnd);
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
   OpenFormDialog — run modal dialog loop
   ========================================================================= */
static void OpenFormDialog(HWND hParent)
{
    RECT  pr;
    int   dw = 580, dh = 520;
    int   cx, cy;
    MSG   msg;

    GetWindowRect(hParent, &pr);
    cx = pr.left + (pr.right  - pr.left - dw) / 2;
    cy = pr.top  + (pr.bottom - pr.top  - dh) / 2;

    g_dlgDone  = FALSE;
    g_dlgSaved = FALSE;

    g_hFormDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "FormulationDlgClass",
        g_dlgData.is_edit ? "Edit Formulation" : "New Formulation",
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
    if (IsWindow(g_hFormDlg)) DestroyWindow(g_hFormDlg);
    g_hFormDlg  = NULL;
    g_hDlgCpdLV = NULL;
    SetForegroundWindow(hParent);
}

/* =========================================================================
   Panel WndProc
   ========================================================================= */
static LRESULT CALLBACK FormPanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        int bw = 90, bh = 28, bx = 4, by = 4, gap = 6;

        /* Buttons at top */
        g_hBtnNew = CreateWindowEx(0, "BUTTON", "New",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx, by, bw, bh, hWnd, (HMENU)(INT_PTR)IDC_BTN_NEW, g_hInst, NULL);
        bx += bw + gap;
        g_hBtnEdit = CreateWindowEx(0, "BUTTON", "Edit",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx, by, bw, bh, hWnd, (HMENU)(INT_PTR)IDC_BTN_EDIT, g_hInst, NULL);
        bx += bw + gap;
        g_hBtnDelete = CreateWindowEx(0, "BUTTON", "Delete",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx, by, bw, bh, hWnd, (HMENU)(INT_PTR)IDC_BTN_DELETE, g_hInst, NULL);
        bx += bw + gap;
        g_hBtnHistory = CreateWindowEx(0, "BUTTON", "Version History",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx, by, 120, bh, hWnd, (HMENU)(INT_PTR)IDC_BTN_HISTORY, g_hInst, NULL);
        bx += 120 + gap;
        g_hBtnValidate = CreateWindowEx(0, "BUTTON", "Validate",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx, by, bw, bh, hWnd, (HMENU)(INT_PTR)IDC_BTN_VALIDATE, g_hInst, NULL);

        /* ListView below buttons */
        g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE |
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, bh + 8, 100, 100, hWnd,
            (HMENU)(INT_PTR)IDC_LISTVIEW, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LV_AddCol(g_hListView, 0, "Code",       90);
        LV_AddCol(g_hListView, 1, "Name",       160);
        LV_AddCol(g_hListView, 2, "Version",    70);
        LV_AddCol(g_hListView, 3, "pH",         50);
        LV_AddCol(g_hListView, 4, "Brix",       50);
        LV_AddCol(g_hListView, 5, "Compounds",  90);
        LV_AddCol(g_hListView, 6, "Saved At",   155);
    }
    return 0;

    case WM_SIZE:
    {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        int top = 36;
        MoveWindow(g_hListView, 0, top, w, h - top, TRUE);
    }
    return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {

        case IDC_BTN_NEW:
        {
            ZeroMemory(&g_dlgData, sizeof(g_dlgData));
            g_dlgData.is_edit       = FALSE;
            g_dlgData.form.version  = create_version(1, 0, 0);
            OpenFormDialog(hWnd);
            if (g_dlgSaved) Panel_Formulations_Refresh();
        }
        break;

        case IDC_BTN_EDIT:
        {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            char code[MAX_FLAVOR_CODE];

            if (sel < 0) { MessageBox(hWnd, "Select a formulation to edit.", "Edit", MB_OK); break; }

            ListView_GetItemText(g_hListView, sel, 0, code, sizeof(code));

            ZeroMemory(&g_dlgData, sizeof(g_dlgData));
            g_dlgData.is_edit = TRUE;
            if (db_load_latest(code, &g_dlgData.form) != 0) {
                MessageBox(hWnd, "Failed to load formulation.", "Error", MB_ICONERROR);
                break;
            }
            OpenFormDialog(hWnd);
            if (g_dlgSaved) Panel_Formulations_Refresh();
        }
        break;

        case IDC_BTN_DELETE:
            MessageBox(hWnd,
                "Delete is not supported. Version history is preserved in the database.",
                "Delete", MB_ICONINFORMATION);
            break;

        case IDC_BTN_HISTORY:
        {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            char code[MAX_FLAVOR_CODE];
            char buf[2048];
            char line[64];
            sqlite3* db;
            sqlite3_stmt* stmt;

            if (sel < 0) { MessageBox(hWnd, "Select a formulation.", "History", MB_OK); break; }

            ListView_GetItemText(g_hListView, sel, 0, code, sizeof(code));
            db = db_get_handle();
            if (!db) break;

            sprintf(buf, "Version history for %s:\n\n", code);

            if (sqlite3_prepare_v2(db,
                    "SELECT ver_major, ver_minor, ver_patch, saved_at "
                    "FROM formulations WHERE flavor_code=? "
                    "ORDER BY ver_major, ver_minor, ver_patch;",
                    -1, &stmt, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(stmt, 1, code, -1, SQLITE_STATIC);
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    sprintf(line, "  v%d.%d.%d   saved %s\n",
                        sqlite3_column_int(stmt, 0),
                        sqlite3_column_int(stmt, 1),
                        sqlite3_column_int(stmt, 2),
                        (const char*)sqlite3_column_text(stmt, 3));
                    strcat(buf, line);
                }
                sqlite3_finalize(stmt);
            }

            MessageBox(hWnd, buf, "Version History", MB_OK);
        }
        break;

        case IDC_BTN_VALIDATE:
        {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            char code[MAX_FLAVOR_CODE];
            Formulation f;
            int violations;
            char msg[256];

            if (sel < 0) { MessageBox(hWnd, "Select a formulation.", "Validate", MB_OK); break; }

            ListView_GetItemText(g_hListView, sel, 0, code, sizeof(code));
            if (db_load_latest(code, &f) != 0) break;

            violations = db_validate_formulation(&f);
            if (violations == 0)
                sprintf(msg, "%s v%d.%d.%d: All compounds within FEMA limits.",
                    code, f.version.major, f.version.minor, f.version.patch);
            else
                sprintf(msg, "%s v%d.%d.%d: %d safety limit violation(s). Check stderr for details.",
                    code, f.version.major, f.version.minor, f.version.patch, violations);

            MessageBox(hWnd, msg, "Validation Result", violations ? MB_ICONWARNING : MB_ICONINFORMATION);
        }
        break;
        }
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   Panel_Formulations_Create
   ========================================================================= */
HWND Panel_Formulations_Create(HWND hParent)
{
    WNDCLASSEX wc;

    /* Register panel class */
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = FormPanelWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "FormulationsPanel";
    RegisterClassEx(&wc);

    /* Register dialog class */
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = FormDlgWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "FormulationDlgClass";
    RegisterClassEx(&wc);

    g_hPanel = CreateWindowEx(0,
        "FormulationsPanel", NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}

/* =========================================================================
   Panel_Formulations_Refresh
   ========================================================================= */
void Panel_Formulations_Refresh(void)
{
    sqlite3*      db   = db_get_handle();
    sqlite3_stmt* stmt = NULL;
    int           row  = 0;
    char          buf[64];

    if (!g_hListView || !db) return;

    ListView_DeleteAllItems(g_hListView);

    /* Latest version of each flavor with compound count */
    if (sqlite3_prepare_v2(db,
            "SELECT f.flavor_code, f.flavor_name, "
            "       f.ver_major, f.ver_minor, f.ver_patch, "
            "       f.target_ph, f.target_brix, f.saved_at, "
            "       (SELECT COUNT(*) FROM formulation_compounds fc "
            "        WHERE fc.formulation_id = f.id) AS cnt "
            "FROM formulations f "
            "WHERE f.id = ("
            "    SELECT id FROM formulations "
            "    WHERE flavor_code = f.flavor_code "
            "    ORDER BY ver_major DESC, ver_minor DESC, ver_patch DESC LIMIT 1"
            ") "
            "ORDER BY f.flavor_code;",
            -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* code    = (const char*)sqlite3_column_text(stmt, 0);
        const char* name    = (const char*)sqlite3_column_text(stmt, 1);
        int         vmaj    = sqlite3_column_int   (stmt, 2);
        int         vmin    = sqlite3_column_int   (stmt, 3);
        int         vpat    = sqlite3_column_int   (stmt, 4);
        double      ph      = sqlite3_column_double(stmt, 5);
        double      brix    = sqlite3_column_double(stmt, 6);
        const char* saved   = (const char*)sqlite3_column_text(stmt, 7);
        int         cnt     = sqlite3_column_int   (stmt, 8);

        LV_InsertRow(g_hListView, row, code ? code : "");

        sprintf(buf, "%d.%d.%d", vmaj, vmin, vpat);
        LV_SetCell(g_hListView, row, 1, name  ? name : "");
        LV_SetCell(g_hListView, row, 2, buf);

        sprintf(buf, "%.2f", ph);
        LV_SetCell(g_hListView, row, 3, buf);

        sprintf(buf, "%.2f", brix);
        LV_SetCell(g_hListView, row, 4, buf);

        sprintf(buf, "%d", cnt);
        LV_SetCell(g_hListView, row, 5, buf);

        LV_SetCell(g_hListView, row, 6, saved ? saved : "");

        row++;
    }

    sqlite3_finalize(stmt);
}
