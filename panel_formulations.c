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
static BOOL        s_ing_filtering = FALSE;

typedef struct {
    BOOL           is_edit;
    Formulation    form;
    FormBase       bases[MAX_FORM_BASES];
    int            base_count;
    FormIngredient ingredients[MAX_FORM_INGREDIENTS];
    int            ingredient_count;
} FormDlgData;

static FormDlgData g_dlgData;
static HWND        g_hFormDlg   = NULL;
static HWND        g_hDlgCpdLV  = NULL;
static HWND        g_hDlgIngLV  = NULL;  /* unified bases+ingredients list */

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
   Unified combo + list helpers (bases encoded as negative id, ings positive)
   ========================================================================= */

/* Populate or filter the unified item combo.
   Bases use itemdata = -(base_id), ingredients use itemdata = ingredient_id. */
static void FilterItemCombo(HWND hCombo, const char *filter)
{
    sqlite3      *db   = db_get_handle();
    sqlite3_stmt *stmt = NULL;
    char          pat[MAX_INGREDIENT_NAME + 4];

    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    if (!db) return;

    snprintf(pat, sizeof(pat), "%%%s%%", filter ? filter : "");

    /* Soda bases */
    if (sqlite3_prepare_v2(db,
        "SELECT sb.id, sb.base_name, sb.base_code, "
        "       sb.ver_major, sb.ver_minor, sb.ver_patch "
        "FROM soda_bases sb "
        "WHERE sb.id = ("
        "    SELECT id FROM soda_bases WHERE base_code = sb.base_code "
        "    ORDER BY ver_major DESC, ver_minor DESC, ver_patch DESC LIMIT 1"
        ") AND sb.base_name LIKE ? "
        "ORDER BY sb.base_name;",
        -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, pat, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int         id   = sqlite3_column_int(stmt, 0);
            const char *nm   = (const char *)sqlite3_column_text(stmt, 1);
            const char *code = (const char *)sqlite3_column_text(stmt, 2);
            int vmaj = sqlite3_column_int(stmt, 3);
            int vmin = sqlite3_column_int(stmt, 4);
            int vpat = sqlite3_column_int(stmt, 5);
            char display[96];
            int  ci;
            snprintf(display, sizeof(display), "[Base] %s (%s v%d.%d.%d)",
                     nm ? nm : "", code ? code : "", vmaj, vmin, vpat);
            ci = (int)SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)display);
            SendMessage(hCombo, CB_SETITEMDATA, (WPARAM)ci, (LPARAM)(-(INT_PTR)id));
        }
        sqlite3_finalize(stmt);
    }

    /* Ingredients */
    stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT id, ingredient_name FROM ingredients "
        "WHERE ingredient_name LIKE ? ORDER BY ingredient_name;",
        -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, pat, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int         id = sqlite3_column_int(stmt, 0);
            const char *nm = (const char *)sqlite3_column_text(stmt, 1);
            int ci = (int)SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)(nm ? nm : ""));
            SendMessage(hCombo, CB_SETITEMDATA, (WPARAM)ci, (LPARAM)(INT_PTR)id);
        }
        sqlite3_finalize(stmt);
    }
}

static void DlgPopulateItemCombo(HWND hCombo)
{
    FilterItemCombo(hCombo, "");
    if ((int)SendMessage(hCombo, CB_GETCOUNT, 0, 0) > 0)
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
}

/* Refresh the unified list.
   lParam encoding: bases → -(index+1), ingredients → index */
static void DlgRefreshItemList(void)
{
    int  i, row;
    char buf[32];

    if (!g_hDlgIngLV) return;
    ListView_DeleteAllItems(g_hDlgIngLV);

    for (i = 0; i < g_dlgData.base_count; i++) {
        LVITEM lvi;
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask    = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem   = i;
        lvi.pszText = g_dlgData.bases[i].base_name;
        lvi.lParam  = (LPARAM)(-(i + 1));
        row = ListView_InsertItem(g_hDlgIngLV, &lvi);
        ListView_SetItemText(g_hDlgIngLV, row, 1, "Base");
        snprintf(buf, sizeof(buf), "%.4f", g_dlgData.bases[i].amount);
        ListView_SetItemText(g_hDlgIngLV, row, 2, buf);
        ListView_SetItemText(g_hDlgIngLV, row, 3, g_dlgData.bases[i].unit);
    }

    for (i = 0; i < g_dlgData.ingredient_count; i++) {
        LVITEM lvi;
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask    = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem   = g_dlgData.base_count + i;
        lvi.pszText = g_dlgData.ingredients[i].ingredient_name;
        lvi.lParam  = (LPARAM)i;
        row = ListView_InsertItem(g_hDlgIngLV, &lvi);
        ListView_SetItemText(g_hDlgIngLV, row, 1, "Ingredient");
        snprintf(buf, sizeof(buf), "%.4f", g_dlgData.ingredients[i].amount);
        ListView_SetItemText(g_hDlgIngLV, row, 2, buf);
        ListView_SetItemText(g_hDlgIngLV, row, 3, g_dlgData.ingredients[i].unit);
    }
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

        /* Remove Selected (compound) */
        CreateWindowEx(0, "BUTTON", "Remove Selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx, y, 120, 25, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_REMOVE_CPD, g_hInst, NULL);
        y += 35;

        /* ---- Ingredients & Bases (unified) ---- */
        CreateWindowEx(0, "STATIC", "Ingredients & Bases:",
            WS_CHILD | WS_VISIBLE,
            lx, y, 160, 18, hWnd, NULL, g_hInst, NULL);
        y += 20;

        g_hDlgIngLV = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            lx, y, 520, 120, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_ING_LIST, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hDlgIngLV,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LV_AddCol(g_hDlgIngLV, 0, "Name",   240);
        LV_AddCol(g_hDlgIngLV, 1, "Type",   100);
        LV_AddCol(g_hDlgIngLV, 2, "Amount",  80);
        LV_AddCol(g_hDlgIngLV, 3, "Unit",    60);
        DlgRefreshItemList();
        y += 128;

        /* Unified add row */
        {
            HWND hItemCombo, hItemUnit;
            CreateWindowEx(0, "STATIC", "Add:",
                WS_CHILD | WS_VISIBLE,
                lx, y+3, 30, 18, hWnd, NULL, g_hInst, NULL);
            hItemCombo = CreateWindowEx(0, "COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
                lx+35, y, 220, 220, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_ING_COMBO, g_hInst, NULL);
            DlgPopulateItemCombo(hItemCombo);

            CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0.0",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                lx+262, y, 60, rh, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_ING_AMOUNT, g_hInst, NULL);

            hItemUnit = CreateWindowEx(0, "COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                lx+328, y, 55, 130, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_ING_UNIT, g_hInst, NULL);
            SendMessage(hItemUnit, CB_ADDSTRING, 0, (LPARAM)"g");
            SendMessage(hItemUnit, CB_ADDSTRING, 0, (LPARAM)"mL");
            SendMessage(hItemUnit, CB_ADDSTRING, 0, (LPARAM)"kg");
            SendMessage(hItemUnit, CB_ADDSTRING, 0, (LPARAM)"L");
            SendMessage(hItemUnit, CB_ADDSTRING, 0, (LPARAM)"%");
            SendMessage(hItemUnit, CB_SETCURSEL, 0, 0);

            CreateWindowEx(0, "BUTTON", "Add",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                lx+390, y, 50, rh, hWnd,
                (HMENU)(INT_PTR)IDC_BTN_ADD_ING, g_hInst, NULL);
        }
        y += 30;

        CreateWindowEx(0, "BUTTON", "Remove Selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx, y, 120, 25, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_REMOVE_ING, g_hInst, NULL);
        y += 35;

        /* Production Instructions */
        CreateWindowEx(0, "STATIC", "Production Instructions:",
            WS_CHILD | WS_VISIBLE,
            lx, y, 180, 18, hWnd, NULL, g_hInst, NULL);
        y += 20;
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT",
            g_dlgData.form.production_instructions,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
            WS_VSCROLL | ES_WANTRETURN,
            lx, y, 520, 110, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_PROD_INSTR, g_hInst, NULL);
        y += 120;

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

        case IDC_DLG_ING_COMBO:
            if (HIWORD(wParam) == CBN_EDITCHANGE && !s_ing_filtering) {
                char text[MAX_INGREDIENT_NAME];
                HWND hCombo = GetDlgItem(hWnd, IDC_DLG_ING_COMBO);
                int  len;
                s_ing_filtering = TRUE;
                GetWindowText(hCombo, text, sizeof(text));
                FilterItemCombo(hCombo, text);
                SetWindowText(hCombo, text);
                len = (int)strlen(text);
                SendMessage(hCombo, CB_SETEDITSEL, 0, MAKELONG(len, len));
                if ((int)SendMessage(hCombo, CB_GETCOUNT, 0, 0) > 0)
                    SendMessage(hCombo, CB_SHOWDROPDOWN, TRUE, 0);
                s_ing_filtering = FALSE;
            }
            break;

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

        case IDC_BTN_ADD_ING:
        {
            char     amtStr[32], unitStr[16], dispName[128];
            float    amt;
            HWND     hItemCombo, hAmt, hUnit;
            int      sel;
            INT_PTR  itemdata;

            hItemCombo = GetDlgItem(hWnd, IDC_DLG_ING_COMBO);
            hAmt       = GetDlgItem(hWnd, IDC_DLG_ING_AMOUNT);
            hUnit      = GetDlgItem(hWnd, IDC_DLG_ING_UNIT);

            sel = (int)SendMessage(hItemCombo, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) {
                char edText[128];
                GetWindowText(hItemCombo, edText, sizeof(edText));
                sel = (int)SendMessage(hItemCombo, CB_FINDSTRINGEXACT,
                                       (WPARAM)-1, (LPARAM)edText);
            }
            if (sel == CB_ERR) {
                MessageBox(hWnd, "Select an item from the dropdown list.", "Info", MB_OK);
                break;
            }
            itemdata = (INT_PTR)SendMessage(hItemCombo, CB_GETITEMDATA, (WPARAM)sel, 0);
            SendMessage(hItemCombo, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)dispName);

            GetWindowText(hAmt, amtStr, sizeof(amtStr));
            amt = (float)atof(amtStr);
            if (amt <= 0.0f) {
                MessageBox(hWnd, "Enter a positive amount.", "Input", MB_OK);
                break;
            }
            sel = (int)SendMessage(hUnit, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) sel = 0;
            SendMessage(hUnit, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)unitStr);

            if (itemdata <= 0) {
                /* Soda base — encoded as -(base_id) */
                int bid = (int)(-(itemdata));
                int idx;
                if (g_dlgData.base_count >= MAX_FORM_BASES) {
                    MessageBox(hWnd, "Maximum bases reached.", "Limit", MB_OK);
                    break;
                }
                idx = g_dlgData.base_count;
                g_dlgData.bases[idx].soda_base_id = bid;
                strncpy(g_dlgData.bases[idx].base_name, dispName, MAX_BASE_NAME - 1);
                g_dlgData.bases[idx].base_name[MAX_BASE_NAME - 1] = '\0';
                g_dlgData.bases[idx].amount = amt;
                strncpy(g_dlgData.bases[idx].unit, unitStr, 15);
                g_dlgData.bases[idx].unit[15] = '\0';
                g_dlgData.base_count++;
            } else {
                /* Ingredient — encoded as ingredient_id */
                int iid = (int)itemdata;
                int idx;
                if (g_dlgData.ingredient_count >= MAX_FORM_INGREDIENTS) {
                    MessageBox(hWnd, "Maximum ingredients reached.", "Limit", MB_OK);
                    break;
                }
                idx = g_dlgData.ingredient_count;
                g_dlgData.ingredients[idx].ingredient_id = iid;
                strncpy(g_dlgData.ingredients[idx].ingredient_name, dispName,
                        MAX_INGREDIENT_NAME - 1);
                g_dlgData.ingredients[idx].ingredient_name[MAX_INGREDIENT_NAME - 1] = '\0';
                g_dlgData.ingredients[idx].amount = amt;
                strncpy(g_dlgData.ingredients[idx].unit, unitStr, 15);
                g_dlgData.ingredients[idx].unit[15] = '\0';
                g_dlgData.ingredient_count++;
            }
            DlgRefreshItemList();
            SetWindowText(GetDlgItem(hWnd, IDC_DLG_ING_AMOUNT), "0.0");
        }
        break;

        case IDC_BTN_REMOVE_ING:
        {
            LVITEM  lvi;
            INT_PTR lp;
            int     sel = ListView_GetNextItem(g_hDlgIngLV, -1, LVNI_SELECTED);
            if (sel < 0) break;
            ZeroMemory(&lvi, sizeof(lvi));
            lvi.mask  = LVIF_PARAM;
            lvi.iItem = sel;
            ListView_GetItem(g_hDlgIngLV, &lvi);
            lp = (INT_PTR)lvi.lParam;

            if (lp < 0) {
                /* Base at index -(lp+1) */
                int idx  = (int)(-(lp + 1));
                int last = g_dlgData.base_count - 1;
                if (idx >= 0 && idx <= last) {
                    if (idx < last)
                        g_dlgData.bases[idx] = g_dlgData.bases[last];
                    g_dlgData.base_count--;
                }
            } else {
                /* Ingredient at index lp */
                int idx  = (int)lp;
                int last = g_dlgData.ingredient_count - 1;
                if (idx >= 0 && idx <= last) {
                    if (idx < last)
                        g_dlgData.ingredients[idx] = g_dlgData.ingredients[last];
                    g_dlgData.ingredient_count--;
                }
            }
            DlgRefreshItemList();
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
            GetWindowText(GetDlgItem(hWnd, IDC_DLG_PROD_INSTR),
                          g_dlgData.form.production_instructions,
                          sizeof(g_dlgData.form.production_instructions));

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

            /* Save associated bases and ingredients */
            db_save_formulation_extras(
                g_dlgData.form.flavor_code,
                g_dlgData.form.version.major,
                g_dlgData.form.version.minor,
                g_dlgData.form.version.patch,
                g_dlgData.bases,       g_dlgData.base_count,
                g_dlgData.ingredients, g_dlgData.ingredient_count);

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
   HTML-escape a string into a FILE (handles & < > " in free-text fields)
   ========================================================================= */
static void fwrite_escaped(FILE *fp, const char *s)
{
    for (; *s; s++) {
        switch (*s) {
        case '&':  fputs("&amp;",  fp); break;
        case '<':  fputs("&lt;",   fp); break;
        case '>':  fputs("&gt;",   fp); break;
        case '"':  fputs("&quot;", fp); break;
        case '\r': break;  /* strip CR; keep LF for pre-wrap */
        default:   fputc(*s, fp);       break;
        }
    }
}

/* =========================================================================
   FormulationPreview — generate HTML and open in default browser
   ========================================================================= */
static void FormulationPreview(HWND hParent, const char *flavor_code)
{
    Formulation    f;
    FormBase       bases[MAX_FORM_BASES];
    FormIngredient ings[MAX_FORM_INGREDIENTS];
    int            base_count = 0, ing_count = 0;
    char           htmlPath[MAX_PATH];
    char           tempDir[MAX_PATH];
    FILE          *fp;
    int            i;

    ZeroMemory(&f, sizeof(f));
    if (db_load_latest(flavor_code, &f) != 0) {
        MessageBox(hParent, "Failed to load formulation.", "Error", MB_ICONERROR);
        return;
    }
    db_load_formulation_extras(
        flavor_code,
        f.version.major, f.version.minor, f.version.patch,
        bases, &base_count, ings, &ing_count);

    GetTempPathA(sizeof(tempDir), tempDir);
    _snprintf(htmlPath, sizeof(htmlPath) - 1,
              "%sformulation_%s_v%d_%d_%d.html",
              tempDir, flavor_code,
              f.version.major, f.version.minor, f.version.patch);

    fp = fopen(htmlPath, "w");
    if (!fp) {
        MessageBox(hParent, "Failed to create preview file.", "Error", MB_ICONERROR);
        return;
    }

    /* ---- HTML head + CSS ---- */
    fprintf(fp,
        "<!DOCTYPE html>\n"
        "<html lang='en'><head><meta charset='UTF-8'>\n"
        "<title>%s v%d.%d.%d</title>\n"
        "<style>\n"
        "  body{font-family:Arial,sans-serif;font-size:11pt;margin:40px;color:#222}\n"
        "  h1{font-size:18pt;margin-bottom:4px}\n"
        "  .sub{color:#555;margin:0 0 18px}\n"
        "  h2{font-size:12pt;margin:26px 0 6px;border-bottom:1px solid #bbb;padding-bottom:3px}\n"
        "  table{border-collapse:collapse;width:100%%;margin-bottom:8px}\n"
        "  th{background:#2c5f8a;color:#fff;padding:6px 10px;text-align:left;font-size:10pt}\n"
        "  td{padding:5px 10px;border-bottom:1px solid #e0e0e0;font-size:10pt}\n"
        "  tr:nth-child(even) td{background:#f4f8fc}\n"
        "  .meta td:first-child{font-weight:bold;width:150px}\n"
        "  .instr{white-space:pre-wrap;background:#fafafa;border:1px solid #ddd;\n"
        "          padding:14px;border-radius:3px;min-height:50px;font-size:10pt}\n"
        "  .none{color:#999;font-style:italic}\n"
        "  @media print{\n"
        "    body{margin:15mm;font-size:10pt}\n"
        "    h2{page-break-after:avoid}\n"
        "    table{page-break-inside:avoid}\n"
        "    .instr{page-break-inside:avoid}\n"
        "  }\n"
        "</style></head><body>\n",
        f.flavor_code, f.version.major, f.version.minor, f.version.patch);

    /* ---- Header ---- */
    fprintf(fp, "<h1>"); fwrite_escaped(fp, f.flavor_name); fprintf(fp, "</h1>\n");
    fprintf(fp, "<p class='sub'>Code: <strong>");
    fwrite_escaped(fp, f.flavor_code);
    fprintf(fp, "</strong> &nbsp;&nbsp; Version: <strong>v%d.%d.%d</strong></p>\n",
            f.version.major, f.version.minor, f.version.patch);

    /* ---- Parameters ---- */
    fprintf(fp, "<h2>Parameters</h2>\n<table class='meta'>\n");
    fprintf(fp, "<tr><td>Target pH</td><td>%.2f</td></tr>\n",   (double)f.target_ph);
    fprintf(fp, "<tr><td>Target Brix</td><td>%.2f</td></tr>\n", (double)f.target_brix);
    fprintf(fp, "</table>\n");

    /* ---- Aroma Compounds ---- */
    if (f.compound_count > 0) {
        fprintf(fp, "<h2>Aroma Compounds</h2>\n<table>\n"
                    "<tr><th>Compound</th><th>ppm</th></tr>\n");
        for (i = 0; i < f.compound_count; i++) {
            fprintf(fp, "<tr><td>"); fwrite_escaped(fp, f.compounds[i].compound_name);
            fprintf(fp, "</td><td>%.4f</td></tr>\n",
                    (double)f.compounds[i].concentration_ppm);
        }
        fprintf(fp, "</table>\n");
    }

    /* ---- Ingredients & Bases ---- */
    if (base_count > 0 || ing_count > 0) {
        fprintf(fp, "<h2>Ingredients &amp; Bases</h2>\n<table>\n"
                    "<tr><th>Name</th><th>Type</th><th>Amount</th><th>Unit</th></tr>\n");
        for (i = 0; i < base_count; i++) {
            fprintf(fp, "<tr><td>"); fwrite_escaped(fp, bases[i].base_name);
            fprintf(fp, "</td><td>Base</td><td>%.4f</td><td>%s</td></tr>\n",
                    (double)bases[i].amount, bases[i].unit);
        }
        for (i = 0; i < ing_count; i++) {
            fprintf(fp, "<tr><td>"); fwrite_escaped(fp, ings[i].ingredient_name);
            fprintf(fp, "</td><td>Ingredient</td><td>%.4f</td><td>%s</td></tr>\n",
                    (double)ings[i].amount, ings[i].unit);
        }
        fprintf(fp, "</table>\n");
    }

    /* ---- Production Instructions ---- */
    fprintf(fp, "<h2>Production Instructions</h2>\n");
    if (f.production_instructions[0]) {
        fprintf(fp, "<div class='instr'>");
        fwrite_escaped(fp, f.production_instructions);
        fprintf(fp, "</div>\n");
    } else {
        fprintf(fp, "<div class='instr none'>No production instructions recorded.</div>\n");
    }

    fprintf(fp, "</body></html>\n");
    fclose(fp);

    ShellExecuteA(hParent, "open", htmlPath, NULL, NULL, SW_SHOW);
}

/* =========================================================================
   OpenFormDialog — run modal dialog loop
   ========================================================================= */
static void OpenFormDialog(HWND hParent)
{
    RECT  pr;
    int   dw = 580, dh = 820;
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
    g_hDlgIngLV = NULL;
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
        bx += bw + gap;
        CreateWindowEx(0, "BUTTON", "Preview / Print",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx, by, 110, bh, hWnd, (HMENU)(INT_PTR)IDC_BTN_PREVIEW_PRINT, g_hInst, NULL);

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
            db_load_formulation_extras(
                g_dlgData.form.flavor_code,
                g_dlgData.form.version.major,
                g_dlgData.form.version.minor,
                g_dlgData.form.version.patch,
                g_dlgData.bases,       &g_dlgData.base_count,
                g_dlgData.ingredients, &g_dlgData.ingredient_count);
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

        case IDC_BTN_PREVIEW_PRINT:
        {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            char code[MAX_FLAVOR_CODE];

            if (sel < 0) { MessageBox(hWnd, "Select a formulation to preview.", "Preview", MB_OK); break; }

            ListView_GetItemText(g_hListView, sel, 0, code, sizeof(code));
            FormulationPreview(hWnd, code);
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
