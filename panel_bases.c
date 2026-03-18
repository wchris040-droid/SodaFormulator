#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "database.h"
#include "soda_base.h"
#include "ingredient.h"
#include "sqlite3.h"

/* =========================================================================
   File-scope state
   ========================================================================= */
static HWND g_hPanel    = NULL;
static HWND g_hLVBases  = NULL;
static HWND g_hBtnEdit  = NULL;
static HWND g_hBtnDel   = NULL;

static BOOL g_baseDlgDone;
static BOOL g_baseDlgSaved;
static int  g_selBaseId = 0;

/* In-progress base for the editor dialog */
static SodaBase g_baseDlgData;
static BOOL     g_baseDlgIsEdit    = FALSE;
static HWND     g_hBaseDlgCpdLV   = NULL;
static HWND     g_hBaseDlgIngLV   = NULL;
static BOOL     s_base_ing_filter  = FALSE;

/* Local IDs for the base editor dialog */
#define IDC_BBASE_CPD_LV       4300
#define IDC_BBASE_ING_LV       4301
#define IDC_BBASE_ADD_CPD      4302
#define IDC_BBASE_REM_CPD      4303
#define IDC_BBASE_ADD_ING      4304
#define IDC_BBASE_REM_ING      4305
#define IDC_BBASE_SAVE         4306
#define IDC_BBASE_CANCEL       4307
#define IDC_BBASE_ING_REF_VOL  4310
#define IDC_BBASE_ING_CONVERT  4311

/* =========================================================================
   ListView helpers
   ========================================================================= */
static void LV_AddCol(HWND hLV, int idx, const char *text, int width)
{
    LVCOLUMN lvc;
    ZeroMemory(&lvc, sizeof(lvc));
    lvc.mask     = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.cx       = width;
    lvc.pszText  = (char *)text;
    lvc.iSubItem = idx;
    ListView_InsertColumn(hLV, idx, &lvc);
}

static int LV_InsertRow(HWND hLV, int row, const char *col0, LPARAM lp)
{
    LVITEM lvi;
    ZeroMemory(&lvi, sizeof(lvi));
    lvi.mask     = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem    = row;
    lvi.iSubItem = 0;
    lvi.pszText  = (char *)col0;
    lvi.lParam   = lp;
    return ListView_InsertItem(hLV, &lvi);
}

static void LV_SetCell(HWND hLV, int row, int col, const char *text)
{
    ListView_SetItemText(hLV, row, col, (char *)text);
}

/* =========================================================================
   Refresh compound and ingredient ListViews in the base editor dialog
   ========================================================================= */
static void DlgRefreshBaseCpdList(void)
{
    int i;
    char buf[32];

    if (!g_hBaseDlgCpdLV) return;
    ListView_DeleteAllItems(g_hBaseDlgCpdLV);

    for (i = 0; i < g_baseDlgData.compound_count; i++) {
        LV_InsertRow(g_hBaseDlgCpdLV, i,
                     g_baseDlgData.compounds[i].compound_name, (LPARAM)i);
        snprintf(buf, sizeof(buf), "%.2f",
                 g_baseDlgData.compounds[i].concentration_ppm);
        LV_SetCell(g_hBaseDlgCpdLV, i, 1, buf);
    }
}

static void DlgRefreshBaseIngList(void)
{
    int i;
    char buf[32];

    if (!g_hBaseDlgIngLV) return;
    ListView_DeleteAllItems(g_hBaseDlgIngLV);

    for (i = 0; i < g_baseDlgData.ingredient_count; i++) {
        LV_InsertRow(g_hBaseDlgIngLV, i,
                     g_baseDlgData.ingredients[i].ingredient_name, (LPARAM)i);
        snprintf(buf, sizeof(buf), "%.4f", g_baseDlgData.ingredients[i].amount);
        LV_SetCell(g_hBaseDlgIngLV, i, 1, buf);
        LV_SetCell(g_hBaseDlgIngLV, i, 2, g_baseDlgData.ingredients[i].unit);
    }
}

/* =========================================================================
   Filter ingredient ComboBox by LIKE — drives autocomplete in base editor
   ========================================================================= */
static void FilterBaseIngCombo(HWND hCombo, const char *filter)
{
    sqlite3      *db   = db_get_handle();
    sqlite3_stmt *stmt = NULL;
    char          pat[MAX_INGREDIENT_NAME + 4];

    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    if (!db) return;

    snprintf(pat, sizeof(pat), "%%%s%%", filter ? filter : "");

    if (sqlite3_prepare_v2(db,
        "SELECT id, ingredient_name FROM ingredients "
        "WHERE ingredient_name LIKE ? ORDER BY ingredient_name;",
        -1, &stmt, NULL) != SQLITE_OK)
        return;

    sqlite3_bind_text(stmt, 1, pat, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int         id = sqlite3_column_int(stmt, 0);
        const char *nm = (const char *)sqlite3_column_text(stmt, 1);
        int ci = (int)SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)(nm ? nm : ""));
        SendMessage(hCombo, CB_SETITEMDATA, (WPARAM)ci, (LPARAM)id);
    }
    sqlite3_finalize(stmt);
}

/* =========================================================================
   Auto-calculate ppm from compound amount + unit + yield and update display
   ========================================================================= */
static void RefreshCpdPpmDisplay(HWND hWnd)
{
    char  amtStr[32], unitStr[16], yieldStr[32], dispStr[32];
    float amt, yield, ppm;
    int   unitSel;
    HWND  hUnit = GetDlgItem(hWnd, IDC_BASE_CPD_UNIT);

    GetWindowText(GetDlgItem(hWnd, IDC_BASE_CPD_PPM),  amtStr,   sizeof(amtStr));
    GetWindowText(GetDlgItem(hWnd, IDC_BASE_YIELD),    yieldStr, sizeof(yieldStr));
    unitSel = (int)SendMessage(hUnit, CB_GETCURSEL, 0, 0);
    if (unitSel == CB_ERR) return;
    SendMessage(hUnit, CB_GETLBTEXT, (WPARAM)unitSel, (LPARAM)unitStr);

    amt   = (float)atof(amtStr);
    yield = (float)atof(yieldStr);
    if (yield <= 0.0f) yield = 1.0f;

    if      (strcmp(unitStr, "ppm") == 0) ppm = amt;
    else if (strcmp(unitStr, "mg")  == 0) ppm = amt                / yield;
    else if (strcmp(unitStr, "g")   == 0) ppm = (amt * 1000.0f)    / yield;
    else if (strcmp(unitStr, "mL")  == 0) ppm = (amt * 1000.0f)    / yield;
    else if (strcmp(unitStr, "kg")  == 0) ppm = (amt * 1000000.0f) / yield;
    else if (strcmp(unitStr, "L")   == 0) ppm = (amt * 1000000.0f) / yield;
    else if (strcmp(unitStr, "%")   == 0) ppm = amt * 10000.0f;
    else ppm = 0.0f;

    snprintf(dispStr, sizeof(dispStr), "%.2f", ppm);
    SetWindowText(GetDlgItem(hWnd, IDC_BASE_CPD_PPM_DISP), dispStr);
}

/* =========================================================================
   Base editor dialog WndProc
   ========================================================================= */
static LRESULT CALLBACK BaseDlgWndProc(HWND hWnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        int  y  = 10;
        int  lx = 10, lw = 80, ex = 95, ew = 200, rh = 22;
        HWND hCode;
        char buf[32];
        sqlite3      *db;
        sqlite3_stmt *stmt;
        HWND hCombo;

        /* Code */
        CreateWindowEx(0, "STATIC", "Base Code:",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        hCode = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT",
            g_baseDlgData.base_code,
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            ex, y, 120, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BASE_CODE, g_hInst, NULL);
        if (g_baseDlgIsEdit)
            SendMessage(hCode, EM_SETREADONLY, TRUE, 0);
        y += 32;

        /* Name */
        CreateWindowEx(0, "STATIC", "Base Name:",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT",
            g_baseDlgData.base_name,
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            ex, y, ew, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BASE_NAME, g_hInst, NULL);
        y += 32;

        /* Yield */
        snprintf(buf, sizeof(buf), "%.2f", g_baseDlgData.yield_liters);
        CreateWindowEx(0, "STATIC", "Yield (L):",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", buf,
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            ex, y, 80, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BASE_YIELD, g_hInst, NULL);
        y += 36;

        /* ---- Aroma Compounds ---- */
        CreateWindowEx(0, "STATIC", "Aroma Compounds:",
            WS_CHILD|WS_VISIBLE, lx, y, 150, 18,
            hWnd, NULL, g_hInst, NULL);
        y += 20;

        g_hBaseDlgCpdLV = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
            lx, y, 580, 120,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_CPD_LV, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hBaseDlgCpdLV,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LV_AddCol(g_hBaseDlgCpdLV, 0, "Compound", 320);
        LV_AddCol(g_hBaseDlgCpdLV, 1, "ppm",      120);
        DlgRefreshBaseCpdList();
        y += 128;

        /* Compound add row */
        CreateWindowEx(0, "STATIC", "Add:",
            WS_CHILD|WS_VISIBLE, lx, y+3, 30, 18,
            hWnd, NULL, g_hInst, NULL);

        db   = db_get_handle();
        stmt = NULL;
        hCombo = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
            lx+35, y, 200, 250,
            hWnd, (HMENU)(INT_PTR)IDC_BASE_CPD_COMBO, g_hInst, NULL);
        if (db && sqlite3_prepare_v2(db,
            "SELECT compound_name FROM compound_library ORDER BY compound_name;",
            -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *n = (const char *)sqlite3_column_text(stmt, 0);
                SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)(n ? n : ""));
            }
            sqlite3_finalize(stmt);
        }
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);

        /* amount + unit */
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0.0",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            lx+240, y, 60, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BASE_CPD_PPM, g_hInst, NULL);
        {
            HWND hUnitC = CreateWindowEx(0, "COMBOBOX", NULL,
                WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
                lx+305, y, 55, 130,
                hWnd, (HMENU)(INT_PTR)IDC_BASE_CPD_UNIT, g_hInst, NULL);
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"ppm");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"mg");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"g");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"mL");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"kg");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"L");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"%");
            SendMessage(hUnitC, CB_SETCURSEL, 0, 0);
        }
        CreateWindowEx(0, "BUTTON", "Add",
            WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            lx+365, y, 45, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_ADD_CPD, g_hInst, NULL);

        /* auto ppm display */
        CreateWindowEx(0, "STATIC", "=",
            WS_CHILD|WS_VISIBLE,
            lx+418, y+3, 10, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0.00",
            WS_CHILD|WS_VISIBLE|ES_READONLY|ES_RIGHT,
            lx+432, y, 75, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BASE_CPD_PPM_DISP, g_hInst, NULL);
        CreateWindowEx(0, "STATIC", "ppm",
            WS_CHILD|WS_VISIBLE,
            lx+511, y+3, 28, 18,
            hWnd, NULL, g_hInst, NULL);
        y += 30;

        CreateWindowEx(0, "BUTTON", "Remove Selected",
            WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            lx, y, 120, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_REM_CPD, g_hInst, NULL);
        y += 34;

        /* ---- General Ingredients ---- */
        CreateWindowEx(0, "STATIC", "General Ingredients:",
            WS_CHILD|WS_VISIBLE, lx, y, 160, 18,
            hWnd, NULL, g_hInst, NULL);
        y += 20;

        g_hBaseDlgIngLV = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
            lx, y, 580, 120,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_ING_LV, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hBaseDlgIngLV,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LV_AddCol(g_hBaseDlgIngLV, 0, "Ingredient", 280);
        LV_AddCol(g_hBaseDlgIngLV, 1, "Amount",     120);
        LV_AddCol(g_hBaseDlgIngLV, 2, "Unit",        80);
        DlgRefreshBaseIngList();
        y += 128;

        /* Ingredient add row */
        CreateWindowEx(0, "STATIC", "Add:",
            WS_CHILD|WS_VISIBLE, lx, y+3, 30, 18,
            hWnd, NULL, g_hInst, NULL);

        stmt = NULL;
        hCombo = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWN|WS_VSCROLL,
            lx+35, y, 220, 250,
            hWnd, (HMENU)(INT_PTR)IDC_BASE_ING_COMBO, g_hInst, NULL);
        if (db && sqlite3_prepare_v2(db,
            "SELECT id, ingredient_name FROM ingredients ORDER BY ingredient_name;",
            -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int         iid = sqlite3_column_int(stmt, 0);
                const char *nm  = (const char *)sqlite3_column_text(stmt, 1);
                int ci = (int)SendMessage(hCombo, CB_ADDSTRING, 0,
                                         (LPARAM)(nm ? nm : ""));
                SendMessage(hCombo, CB_SETITEMDATA, (WPARAM)ci, (LPARAM)iid);
            }
            sqlite3_finalize(stmt);
        }
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);

        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0.0",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            lx+262, y, 70, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BASE_ING_AMOUNT, g_hInst, NULL);

        {
            HWND hUnitC = CreateWindowEx(0, "COMBOBOX", NULL,
                WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
                lx+338, y, 60, 120,
                hWnd, (HMENU)(INT_PTR)IDC_BASE_ING_UNIT, g_hInst, NULL);
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"g");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"mL");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"kg");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"L");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"%");
            SendMessage(hUnitC, CB_ADDSTRING, 0, (LPARAM)"ppm");
            SendMessage(hUnitC, CB_SETCURSEL, 0, 0);
        }

        CreateWindowEx(0, "BUTTON", "Add",
            WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            lx+404, y, 50, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_ADD_ING, g_hInst, NULL);

        /* ppm / volume conversion for ingredients */
        CreateWindowEx(0, "STATIC", "@ (L):",
            WS_CHILD|WS_VISIBLE, lx+462, y+3, 30, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "10.0",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            lx+496, y, 38, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_ING_REF_VOL, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "\xAB ppm",
            WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            lx+538, y, 38, rh,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_ING_CONVERT, g_hInst, NULL);
        y += 30;

        CreateWindowEx(0, "BUTTON", "Remove Selected",
            WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            lx, y, 120, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_REM_ING, g_hInst, NULL);
        y += 34;

        /* Save / Cancel */
        CreateWindowEx(0, "BUTTON", "Save",
            WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,
            lx, y, 90, 28,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_SAVE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Cancel",
            WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            lx + 100, y, 90, 28,
            hWnd, (HMENU)(INT_PTR)IDC_BBASE_CANCEL, g_hInst, NULL);
    }
    return 0;

    case WM_COMMAND:
    {
        int ctl = LOWORD(wParam);

        /* ---- Auto-refresh ppm display ---- */
        if ((ctl == IDC_BASE_CPD_PPM  && HIWORD(wParam) == EN_CHANGE)    ||
            (ctl == IDC_BASE_CPD_UNIT && HIWORD(wParam) == CBN_SELCHANGE) ||
            (ctl == IDC_BASE_YIELD    && HIWORD(wParam) == EN_CHANGE))
        {
            RefreshCpdPpmDisplay(hWnd);
            return 0;
        }

        /* ---- Ingredient autocomplete ---- */
        if (ctl == IDC_BASE_ING_COMBO &&
            HIWORD(wParam) == CBN_EDITCHANGE &&
            !s_base_ing_filter)
        {
            char text[MAX_INGREDIENT_NAME];
            HWND hCombo = GetDlgItem(hWnd, IDC_BASE_ING_COMBO);
            int  len;
            s_base_ing_filter = TRUE;
            GetWindowText(hCombo, text, sizeof(text));
            FilterBaseIngCombo(hCombo, text);
            SetWindowText(hCombo, text);
            len = (int)strlen(text);
            SendMessage(hCombo, CB_SETEDITSEL, 0, MAKELONG(len, len));
            if ((int)SendMessage(hCombo, CB_GETCOUNT, 0, 0) > 0)
                SendMessage(hCombo, CB_SHOWDROPDOWN, TRUE, 0);
            s_base_ing_filter = FALSE;
            return 0;
        }

        /* ---- Add compound ---- */
        if (ctl == IDC_BBASE_ADD_CPD) {
            char name[64], amtStr[32], unitStr[16], yieldStr[32];
            float amt, yield, ppm;
            HWND hCombo, hAmt, hUnit;
            int  sel;

            if (g_baseDlgData.compound_count >= MAX_BASE_COMPOUNDS) {
                MessageBox(hWnd, "Maximum compounds reached.", "Limit", MB_OK);
                return 0;
            }
            hCombo = GetDlgItem(hWnd, IDC_BASE_CPD_COMBO);
            hAmt   = GetDlgItem(hWnd, IDC_BASE_CPD_PPM);
            hUnit  = GetDlgItem(hWnd, IDC_BASE_CPD_UNIT);
            sel    = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) return 0;
            SendMessage(hCombo, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)name);
            GetWindowText(hAmt, amtStr, sizeof(amtStr));
            amt = (float)atof(amtStr);
            sel = (int)SendMessage(hUnit, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) sel = 0;
            SendMessage(hUnit, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)unitStr);
            GetWindowText(GetDlgItem(hWnd, IDC_BASE_YIELD), yieldStr, sizeof(yieldStr));
            yield = (float)atof(yieldStr);
            if (yield <= 0.0f) yield = 1.0f;

            if      (strcmp(unitStr, "ppm") == 0) ppm = amt;
            else if (strcmp(unitStr, "mg")  == 0) ppm = amt                / yield;
            else if (strcmp(unitStr, "g")   == 0) ppm = (amt * 1000.0f)    / yield;
            else if (strcmp(unitStr, "mL")  == 0) ppm = (amt * 1000.0f)    / yield;
            else if (strcmp(unitStr, "kg")  == 0) ppm = (amt * 1000000.0f) / yield;
            else if (strcmp(unitStr, "L")   == 0) ppm = (amt * 1000000.0f) / yield;
            else if (strcmp(unitStr, "%")   == 0) ppm = amt * 10000.0f;
            else ppm = 0.0f;

            if (ppm <= 0.0f) {
                MessageBox(hWnd, "Enter a positive amount.", "Input", MB_OK);
                return 0;
            }
            strncpy(g_baseDlgData.compounds[g_baseDlgData.compound_count].compound_name,
                    name, 63);
            g_baseDlgData.compounds[g_baseDlgData.compound_count].compound_name[63] = '\0';
            g_baseDlgData.compounds[g_baseDlgData.compound_count].concentration_ppm = ppm;
            g_baseDlgData.compounds[g_baseDlgData.compound_count].compound_library_id = 0;
            g_baseDlgData.compound_count++;
            DlgRefreshBaseCpdList();
            SetWindowText(hAmt, "0.0");
            RefreshCpdPpmDisplay(hWnd);
            return 0;
        }

        /* ---- Remove compound ---- */
        if (ctl == IDC_BBASE_REM_CPD) {
            int sel  = ListView_GetNextItem(g_hBaseDlgCpdLV, -1, LVNI_SELECTED);
            int last;
            if (sel < 0) return 0;
            last = g_baseDlgData.compound_count - 1;
            if (sel < last)
                g_baseDlgData.compounds[sel] = g_baseDlgData.compounds[last];
            g_baseDlgData.compound_count--;
            DlgRefreshBaseCpdList();
            return 0;
        }

        /* ---- Convert ingredient ppm <-> volume ---- */
        if (ctl == IDC_BBASE_ING_CONVERT) {
            char amtStr[32], unitStr[16], refStr[16];
            float amt, ref;
            int unitSel, newSel;
            HWND hAmt  = GetDlgItem(hWnd, IDC_BASE_ING_AMOUNT);
            HWND hUnit = GetDlgItem(hWnd, IDC_BASE_ING_UNIT);
            HWND hRef  = GetDlgItem(hWnd, IDC_BBASE_ING_REF_VOL);
            GetWindowText(hAmt,  amtStr, sizeof(amtStr));
            GetWindowText(hRef,  refStr, sizeof(refStr));
            unitSel = (int)SendMessage(hUnit, CB_GETCURSEL, 0, 0);
            if (unitSel == CB_ERR) return 0;
            SendMessage(hUnit, CB_GETLBTEXT, (WPARAM)unitSel, (LPARAM)unitStr);
            amt = (float)atof(amtStr);
            ref = (float)atof(refStr);
            if (ref <= 0.0f) {
                MessageBox(hWnd, "Enter a positive reference batch volume.", "Convert", MB_OK);
                return 0;
            }
            if (strcmp(unitStr, "mL") == 0) {
                snprintf(amtStr, sizeof(amtStr), "%.2f", amt * 1000.0f / ref);
                SetWindowText(hAmt, amtStr);
                newSel = (int)SendMessage(hUnit, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"ppm");
                if (newSel != CB_ERR) SendMessage(hUnit, CB_SETCURSEL, (WPARAM)newSel, 0);
            } else if (strcmp(unitStr, "L") == 0) {
                snprintf(amtStr, sizeof(amtStr), "%.2f", amt * 1000000.0f / ref);
                SetWindowText(hAmt, amtStr);
                newSel = (int)SendMessage(hUnit, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"ppm");
                if (newSel != CB_ERR) SendMessage(hUnit, CB_SETCURSEL, (WPARAM)newSel, 0);
            } else if (strcmp(unitStr, "ppm") == 0) {
                snprintf(amtStr, sizeof(amtStr), "%.4f", amt * ref / 1000.0f);
                SetWindowText(hAmt, amtStr);
                newSel = (int)SendMessage(hUnit, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)"mL");
                if (newSel != CB_ERR) SendMessage(hUnit, CB_SETCURSEL, (WPARAM)newSel, 0);
            } else {
                MessageBox(hWnd, "Select mL, L, or ppm to convert.", "Convert", MB_OK);
            }
            return 0;
        }

        /* ---- Add ingredient ---- */
        if (ctl == IDC_BBASE_ADD_ING) {
            char amtStr[32], unitStr[16], name[64];
            float amt;
            HWND hCombo, hAmt, hUnit;
            int sel, iid;

            if (g_baseDlgData.ingredient_count >= MAX_BASE_INGREDIENTS) {
                MessageBox(hWnd, "Maximum ingredients reached.", "Limit", MB_OK);
                return 0;
            }
            hCombo = GetDlgItem(hWnd, IDC_BASE_ING_COMBO);
            hAmt   = GetDlgItem(hWnd, IDC_BASE_ING_AMOUNT);
            hUnit  = GetDlgItem(hWnd, IDC_BASE_ING_UNIT);
            sel    = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) {
                char edText[64];
                GetWindowText(hCombo, edText, sizeof(edText));
                sel = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT,
                                       (WPARAM)-1, (LPARAM)edText);
            }
            if (sel == CB_ERR) {
                MessageBox(hWnd, "Select an ingredient from the dropdown list.", "Info", MB_OK);
                return 0;
            }
            iid = (int)SendMessage(hCombo, CB_GETITEMDATA, (WPARAM)sel, 0);
            SendMessage(hCombo, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)name);
            GetWindowText(hAmt, amtStr, sizeof(amtStr));
            amt = (float)atof(amtStr);
            if (amt <= 0.0f) {
                MessageBox(hWnd, "Enter a positive amount.", "Input", MB_OK);
                return 0;
            }
            sel = (int)SendMessage(hUnit, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) sel = 0;
            SendMessage(hUnit, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)unitStr);

            {
                int idx = g_baseDlgData.ingredient_count;
                g_baseDlgData.ingredients[idx].ingredient_id = iid;
                strncpy(g_baseDlgData.ingredients[idx].ingredient_name, name, 63);
                g_baseDlgData.ingredients[idx].ingredient_name[63] = '\0';
                g_baseDlgData.ingredients[idx].amount = amt;
                strncpy(g_baseDlgData.ingredients[idx].unit, unitStr, 15);
                g_baseDlgData.ingredients[idx].unit[15] = '\0';
                g_baseDlgData.ingredient_count++;
            }
            DlgRefreshBaseIngList();
            SetWindowText(GetDlgItem(hWnd, IDC_BASE_ING_AMOUNT), "0.0");
            return 0;
        }

        /* ---- Remove ingredient ---- */
        if (ctl == IDC_BBASE_REM_ING) {
            int sel  = ListView_GetNextItem(g_hBaseDlgIngLV, -1, LVNI_SELECTED);
            int last;
            if (sel < 0) return 0;
            last = g_baseDlgData.ingredient_count - 1;
            if (sel < last)
                g_baseDlgData.ingredients[sel] = g_baseDlgData.ingredients[last];
            g_baseDlgData.ingredient_count--;
            DlgRefreshBaseIngList();
            return 0;
        }

        /* ---- Save ---- */
        if (ctl == IDC_BBASE_SAVE) {
            char code[MAX_BASE_CODE], name[MAX_BASE_NAME], yieldStr[32];
            int rc;

            GetWindowText(GetDlgItem(hWnd, IDC_BASE_CODE),  code,     sizeof(code));
            GetWindowText(GetDlgItem(hWnd, IDC_BASE_NAME),  name,     sizeof(name));
            GetWindowText(GetDlgItem(hWnd, IDC_BASE_YIELD), yieldStr, sizeof(yieldStr));

            if (!code[0] || !name[0]) {
                MessageBox(hWnd, "Code and Name are required.", "Validation", MB_OK);
                return 0;
            }

            strncpy(g_baseDlgData.base_code, code, MAX_BASE_CODE - 1);
            g_baseDlgData.base_code[MAX_BASE_CODE - 1] = '\0';
            strncpy(g_baseDlgData.base_name, name, MAX_BASE_NAME - 1);
            g_baseDlgData.base_name[MAX_BASE_NAME - 1] = '\0';
            g_baseDlgData.yield_liters = (float)atof(yieldStr);
            if (g_baseDlgData.yield_liters <= 0.0f)
                g_baseDlgData.yield_liters = 1.0f;

            if (g_baseDlgIsEdit)
                g_baseDlgData.version =
                    increment_version(g_baseDlgData.version, INCREMENT_PATCH);

            rc = db_save_soda_base(&g_baseDlgData);
            if (rc == -1) {
                MessageBox(hWnd,
                    "Save failed: this version already exists.",
                    "Error", MB_ICONERROR);
                return 0;
            }
            if (rc != 0) {
                MessageBox(hWnd, "Save failed.", "Error", MB_ICONERROR);
                return 0;
            }

            g_baseDlgSaved = TRUE;
            g_baseDlgDone  = TRUE;
            DestroyWindow(hWnd);
            return 0;
        }

        /* ---- Cancel ---- */
        if (ctl == IDC_BBASE_CANCEL) {
            g_baseDlgDone = TRUE;
            DestroyWindow(hWnd);
            return 0;
        }
    }
    return 0;

    case WM_CLOSE:
        g_baseDlgDone = TRUE;
        DestroyWindow(hWnd);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   OpenBaseDialog
   ========================================================================= */
static void OpenBaseDialog(HWND hParent)
{
    RECT pr;
    int  dw = 640, dh = 680;
    int  cx, cy;
    MSG  m;
    HWND hDlg;
    static BOOL s_reg = FALSE;

    if (!s_reg) {
        WNDCLASSEX wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.lpfnWndProc   = BaseDlgWndProc;
        wc.hInstance     = g_hInst;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "BaseDlgClass";
        RegisterClassEx(&wc);
        s_reg = TRUE;
    }

    GetWindowRect(hParent, &pr);
    cx = pr.left + (pr.right  - pr.left - dw) / 2;
    cy = pr.top  + (pr.bottom - pr.top  - dh) / 2;

    g_baseDlgDone  = FALSE;
    g_baseDlgSaved = FALSE;

    hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "BaseDlgClass",
        g_baseDlgIsEdit ? "Edit Soda Base" : "New Soda Base",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        cx, cy, dw, dh,
        hParent, NULL, g_hInst, NULL);

    EnableWindow(hParent, FALSE);

    while (!g_baseDlgDone) {
        if (!GetMessage(&m, NULL, 0, 0)) { PostQuitMessage((int)m.wParam); break; }
        TranslateMessage(&m);
        DispatchMessage(&m);
    }

    EnableWindow(hParent, TRUE);
    if (IsWindow(hDlg)) DestroyWindow(hDlg);
    g_hBaseDlgCpdLV = NULL;
    g_hBaseDlgIngLV = NULL;
    SetForegroundWindow(hParent);
}

/* =========================================================================
   Bases panel WndProc
   ========================================================================= */
static LRESULT CALLBACK BasesPanelWndProc(HWND hWnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        CreateWindowEx(0, "BUTTON", "New Base",
            WS_CHILD|WS_VISIBLE, 4, 4, 90, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_NEW_BASE, g_hInst, NULL);

        g_hBtnEdit = CreateWindowEx(0, "BUTTON", "Edit Base",
            WS_CHILD|WS_VISIBLE, 102, 4, 90, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_EDIT_BASE, g_hInst, NULL);
        EnableWindow(g_hBtnEdit, FALSE);

        g_hBtnDel = CreateWindowEx(0, "BUTTON", "Delete Base",
            WS_CHILD|WS_VISIBLE, 200, 4, 90, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_DEL_BASE, g_hInst, NULL);
        EnableWindow(g_hBtnDel, FALSE);

        g_hLVBases = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
            0, 36, 800, 400,
            hWnd, (HMENU)(INT_PTR)IDC_LV_BASES, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hLVBases,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LV_AddCol(g_hLVBases, 0, "Code",     90);
        LV_AddCol(g_hLVBases, 1, "Name",    160);
        LV_AddCol(g_hLVBases, 2, "Version",  70);
        LV_AddCol(g_hLVBases, 3, "Yield L",  70);
        LV_AddCol(g_hLVBases, 4, "Cpds",     60);
        LV_AddCol(g_hLVBases, 5, "Ings",     60);
        LV_AddCol(g_hLVBases, 6, "Saved At",155);
    }
    return 0;

    case WM_SIZE:
    {
        WORD w = LOWORD(lParam), h = HIWORD(lParam);
        if (g_hLVBases)
            MoveWindow(g_hLVBases, 0, 36, w, h > 36 ? h - 36 : 0, TRUE);
    }
    return 0;

    case WM_COMMAND:
    {
        int ctl = LOWORD(wParam);

        if (ctl == IDC_BTN_NEW_BASE) {
            ZeroMemory(&g_baseDlgData, sizeof(g_baseDlgData));
            g_baseDlgData.version    = create_version(1, 0, 0);
            g_baseDlgData.yield_liters = 1.0f;
            g_baseDlgIsEdit          = FALSE;
            OpenBaseDialog(hWnd);
            if (g_baseDlgSaved) Panel_Bases_Refresh();
            return 0;
        }

        if (ctl == IDC_BTN_EDIT_BASE) {
            if (g_selBaseId > 0) {
                char base_code[MAX_BASE_CODE];
                /* Read base_code from first column of selected row */
                int sel = ListView_GetNextItem(g_hLVBases, -1, LVNI_SELECTED);
                if (sel < 0) return 0;
                ListView_GetItemText(g_hLVBases, sel, 0,
                                     base_code, sizeof(base_code));
                ZeroMemory(&g_baseDlgData, sizeof(g_baseDlgData));
                if (db_load_latest_base(base_code, &g_baseDlgData) != 0) {
                    MessageBox(hWnd, "Failed to load soda base.", "Error",
                               MB_ICONERROR);
                    return 0;
                }
                g_baseDlgIsEdit = TRUE;
                OpenBaseDialog(hWnd);
                if (g_baseDlgSaved) Panel_Bases_Refresh();
            }
            return 0;
        }

        if (ctl == IDC_BTN_DEL_BASE) {
            if (g_selBaseId > 0) {
                char base_code[MAX_BASE_CODE];
                int sel = ListView_GetNextItem(g_hLVBases, -1, LVNI_SELECTED);
                int rc;
                if (sel < 0) return 0;
                ListView_GetItemText(g_hLVBases, sel, 0,
                                     base_code, sizeof(base_code));
                if (MessageBox(hWnd,
                        "Delete this soda base (all versions)?",
                        "Confirm Delete",
                        MB_YESNO | MB_ICONWARNING) != IDYES)
                    return 0;
                rc = db_delete_soda_base(base_code);
                if (rc == 1) {
                    MessageBox(hWnd,
                        "Cannot delete: base is referenced by a formulation.",
                        "In Use", MB_ICONWARNING);
                } else if (rc != 0) {
                    MessageBox(hWnd, "Delete failed.", "Error", MB_ICONERROR);
                } else {
                    g_selBaseId = 0;
                    Panel_Bases_Refresh();
                }
            }
            return 0;
        }
    }
    return 0;

    case WM_NOTIFY:
    {
        NMHDR *pnm = (NMHDR *)lParam;
        if (pnm->idFrom == IDC_LV_BASES &&
            pnm->code   == LVN_ITEMCHANGED)
        {
            NMLISTVIEW *pnlv = (NMLISTVIEW *)lParam;
            if (pnlv->uNewState & LVIS_SELECTED) {
                LVITEM lvi;
                ZeroMemory(&lvi, sizeof(lvi));
                lvi.mask  = LVIF_PARAM;
                lvi.iItem = pnlv->iItem;
                ListView_GetItem(g_hLVBases, &lvi);
                g_selBaseId = (int)lvi.lParam;
                EnableWindow(g_hBtnEdit, TRUE);
                EnableWindow(g_hBtnDel,  TRUE);
            }
            return 0;
        }
    }
    return CDRF_DODEFAULT;

    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   Panel_Bases_Create
   ========================================================================= */
HWND Panel_Bases_Create(HWND hParent)
{
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.lpfnWndProc   = BasesPanelWndProc;
    wc.hInstance     = g_hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "BasesPanel";
    RegisterClassEx(&wc);

    g_hPanel = CreateWindowEx(0,
        "BasesPanel", NULL,
        WS_CHILD,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}

/* =========================================================================
   Panel_Bases_Refresh
   ========================================================================= */
void Panel_Bases_Refresh(void)
{
    sqlite3      *db   = db_get_handle();
    sqlite3_stmt *stmt = NULL;
    int           row  = 0;

    if (!g_hLVBases || !db) return;

    ListView_DeleteAllItems(g_hLVBases);
    g_selBaseId = 0;
    EnableWindow(g_hBtnEdit, FALSE);
    EnableWindow(g_hBtnDel,  FALSE);

    if (sqlite3_prepare_v2(db,
        "SELECT sb.id, sb.base_code, sb.base_name, "
        "       sb.ver_major, sb.ver_minor, sb.ver_patch, "
        "       sb.yield_liters, sb.saved_at, "
        "       (SELECT COUNT(*) FROM soda_base_compounds WHERE soda_base_id=sb.id), "
        "       (SELECT COUNT(*) FROM soda_base_ingredients WHERE soda_base_id=sb.id) "
        "FROM soda_bases sb "
        "WHERE sb.id = ("
        "    SELECT id FROM soda_bases WHERE base_code = sb.base_code "
        "    ORDER BY ver_major DESC, ver_minor DESC, ver_patch DESC LIMIT 1"
        ") "
        "ORDER BY sb.base_code;",
        -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int         id      = sqlite3_column_int(stmt, 0);
        const char *code    = (const char *)sqlite3_column_text(stmt, 1);
        const char *name    = (const char *)sqlite3_column_text(stmt, 2);
        int         vmaj    = sqlite3_column_int(stmt, 3);
        int         vmin    = sqlite3_column_int(stmt, 4);
        int         vpat    = sqlite3_column_int(stmt, 5);
        double      yield   = sqlite3_column_double(stmt, 6);
        const char *saved   = (const char *)sqlite3_column_text(stmt, 7);
        int         ncpd    = sqlite3_column_int(stmt, 8);
        int         ning    = sqlite3_column_int(stmt, 9);
        char ver[16], yieldbuf[16], cpdbuf[8], ingbuf[8];
        int actual;

        snprintf(ver,      sizeof(ver),      "%d.%d.%d", vmaj, vmin, vpat);
        snprintf(yieldbuf, sizeof(yieldbuf), "%.2f",     yield);
        snprintf(cpdbuf,   sizeof(cpdbuf),   "%d",       ncpd);
        snprintf(ingbuf,   sizeof(ingbuf),   "%d",       ning);

        actual = LV_InsertRow(g_hLVBases, row, code ? code : "", (LPARAM)id);
        LV_SetCell(g_hLVBases, actual, 1, name  ? name  : "");
        LV_SetCell(g_hLVBases, actual, 2, ver);
        LV_SetCell(g_hLVBases, actual, 3, yieldbuf);
        LV_SetCell(g_hLVBases, actual, 4, cpdbuf);
        LV_SetCell(g_hLVBases, actual, 5, ingbuf);
        LV_SetCell(g_hLVBases, actual, 6, saved ? saved : "");
        row++;
    }
    sqlite3_finalize(stmt);
}
