#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "database.h"
#include "ingredient.h"
#include "sqlite3.h"

/* =========================================================================
   File-scope state
   ========================================================================= */
static HWND g_hPanel   = NULL;
static HWND g_hLV      = NULL;
static HWND g_hBtnEdit = NULL;
static HWND g_hBtnDel  = NULL;
static HWND g_hSearch  = NULL;

static BOOL g_dlgDone;
static BOOL g_dlgSaved;
static int  g_selIngId = 0;

/* Local control IDs */
#define IDC_ING_SEARCH     4100
#define IDC_ING_DLG_SAVE   4200
#define IDC_ING_DLG_CANCEL 4201

/* Category and unit string tables (parallel to combobox order) */
static const char *s_cats[]  = {
    "sweetener","acid","water","CO2","color","preservative","essential_oil","other"
};
static const char *s_units[] = { "g","mL","kg","L","%" };

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
   Ingredient dialog WndProc
   lpCreateParams: 0 = new, nonzero = ingredient id to edit
   ========================================================================= */
static LRESULT CALLBACK IngDlgWndProc(HWND hWnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        CREATESTRUCT *cs  = (CREATESTRUCT *)lParam;
        int           iid = (int)(INT_PTR)cs->lpCreateParams;
        int           y   = 10;
        int           lx  = 10, lw = 100, ex = 115, ew = 250, rh = 22;
        HWND hCat, hUnit, hSup;
        int  i;

        /* Name */
        CreateWindowEx(0, "STATIC", "Name:",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL, ex, y, ew, rh,
            hWnd, (HMENU)(INT_PTR)IDC_ING_NAME, g_hInst, NULL);
        y += 32;

        /* Category */
        CreateWindowEx(0, "STATIC", "Category:",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        hCat = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
            ex, y, ew, 200,
            hWnd, (HMENU)(INT_PTR)IDC_ING_CATEGORY, g_hInst, NULL);
        for (i = 0; i < 8; i++)
            SendMessage(hCat, CB_ADDSTRING, 0, (LPARAM)s_cats[i]);
        SendMessage(hCat, CB_SETCURSEL, 7, 0); /* default: other */
        y += 32;

        /* Unit */
        CreateWindowEx(0, "STATIC", "Unit:",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        hUnit = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
            ex, y, 100, 150,
            hWnd, (HMENU)(INT_PTR)IDC_ING_UNIT, g_hInst, NULL);
        for (i = 0; i < 5; i++)
            SendMessage(hUnit, CB_ADDSTRING, 0, (LPARAM)s_units[i]);
        SendMessage(hUnit, CB_SETCURSEL, 0, 0);
        y += 32;

        /* Cost/unit */
        CreateWindowEx(0, "STATIC", "Cost/unit ($):",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0.0000",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL, ex, y, 100, rh,
            hWnd, (HMENU)(INT_PTR)IDC_ING_COST, g_hInst, NULL);
        y += 32;

        /* Supplier */
        CreateWindowEx(0, "STATIC", "Supplier:",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        hSup = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
            ex, y, ew, 200,
            hWnd, (HMENU)(INT_PTR)IDC_ING_SUPPLIER, g_hInst, NULL);
        {
            int idx = (int)SendMessage(hSup, CB_ADDSTRING, 0, (LPARAM)"(none)");
            SendMessage(hSup, CB_SETITEMDATA, (WPARAM)idx, (LPARAM)0);
        }
        {
            sqlite3      *db  = db_get_handle();
            sqlite3_stmt *qst = NULL;
            if (db && sqlite3_prepare_v2(db,
                "SELECT id, supplier_name FROM suppliers ORDER BY supplier_name;",
                -1, &qst, NULL) == SQLITE_OK) {
                while (sqlite3_step(qst) == SQLITE_ROW) {
                    int         sid = sqlite3_column_int(qst, 0);
                    const char *nm  = (const char *)sqlite3_column_text(qst, 1);
                    int ci = (int)SendMessage(hSup, CB_ADDSTRING, 0,
                                             (LPARAM)(nm ? nm : ""));
                    SendMessage(hSup, CB_SETITEMDATA, (WPARAM)ci, (LPARAM)sid);
                }
                sqlite3_finalize(qst);
            }
        }
        SendMessage(hSup, CB_SETCURSEL, 0, 0);
        y += 32;

        /* Brand */
        CreateWindowEx(0, "STATIC", "Brand:",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL, ex, y, ew, rh,
            hWnd, (HMENU)(INT_PTR)IDC_ING_BRAND, g_hInst, NULL);
        y += 32;

        /* Notes */
        CreateWindowEx(0, "STATIC", "Notes:",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, lx, y+2, lw, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,
            ex, y, ew, 60,
            hWnd, (HMENU)(INT_PTR)IDC_ING_NOTES, g_hInst, NULL);
        y += 72;

        CreateWindowEx(0, "BUTTON", "Save",
            WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,
            ex, y, 80, 26,
            hWnd, (HMENU)(INT_PTR)IDC_ING_DLG_SAVE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Cancel",
            WS_CHILD|WS_VISIBLE,
            ex + 90, y, 80, 26,
            hWnd, (HMENU)(INT_PTR)IDC_ING_DLG_CANCEL, g_hInst, NULL);

        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)iid);

        /* Pre-fill if editing */
        if (iid != 0) {
            Ingredient ing;
            if (db_get_ingredient(iid, &ing) == 0) {
                char buf[32];
                HWND hCat2, hUnit2, hSup2;
                int  ci, cnt;

                SetWindowText(GetDlgItem(hWnd, IDC_ING_NAME),  ing.ingredient_name);
                SetWindowText(GetDlgItem(hWnd, IDC_ING_BRAND), ing.brand);
                SetWindowText(GetDlgItem(hWnd, IDC_ING_NOTES), ing.notes);
                snprintf(buf, sizeof(buf), "%.4f", ing.cost_per_unit);
                SetWindowText(GetDlgItem(hWnd, IDC_ING_COST), buf);

                hCat2 = GetDlgItem(hWnd, IDC_ING_CATEGORY);
                for (ci = 0; ci < 8; ci++) {
                    if (strcmp(s_cats[ci], ing.category) == 0) {
                        SendMessage(hCat2, CB_SETCURSEL, (WPARAM)ci, 0);
                        break;
                    }
                }

                hUnit2 = GetDlgItem(hWnd, IDC_ING_UNIT);
                for (ci = 0; ci < 5; ci++) {
                    if (strcmp(s_units[ci], ing.unit) == 0) {
                        SendMessage(hUnit2, CB_SETCURSEL, (WPARAM)ci, 0);
                        break;
                    }
                }

                if (ing.supplier_id > 0) {
                    hSup2 = GetDlgItem(hWnd, IDC_ING_SUPPLIER);
                    cnt   = (int)SendMessage(hSup2, CB_GETCOUNT, 0, 0);
                    for (ci = 0; ci < cnt; ci++) {
                        int sd = (int)SendMessage(hSup2, CB_GETITEMDATA, (WPARAM)ci, 0);
                        if (sd == ing.supplier_id) {
                            SendMessage(hSup2, CB_SETCURSEL, (WPARAM)ci, 0);
                            break;
                        }
                    }
                }
            }
            SetWindowText(hWnd, "Edit Ingredient");
        }
    }
    return 0;

    case WM_COMMAND:
    {
        int ctl = LOWORD(wParam);

        if (ctl == IDC_ING_DLG_SAVE) {
            char name[MAX_INGREDIENT_NAME];
            char costStr[32];
            char brand[MAX_INGREDIENT_BRAND];
            char notes[256];
            char catStr[MAX_INGREDIENT_CATEGORY];
            char unitStr[MAX_INGREDIENT_UNIT];
            int  iid = (int)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            Ingredient ing;
            int rc, ci;
            HWND hCat2, hUnit2, hSup2;

            GetWindowText(GetDlgItem(hWnd, IDC_ING_NAME),  name,    sizeof(name));
            GetWindowText(GetDlgItem(hWnd, IDC_ING_COST),  costStr, sizeof(costStr));
            GetWindowText(GetDlgItem(hWnd, IDC_ING_BRAND), brand,   sizeof(brand));
            GetWindowText(GetDlgItem(hWnd, IDC_ING_NOTES), notes,   sizeof(notes));

            if (!name[0]) {
                MessageBox(hWnd, "Name is required.", "Error", MB_ICONWARNING);
                return 0;
            }

            hCat2  = GetDlgItem(hWnd, IDC_ING_CATEGORY);
            hUnit2 = GetDlgItem(hWnd, IDC_ING_UNIT);
            hSup2  = GetDlgItem(hWnd, IDC_ING_SUPPLIER);

            ci = (int)SendMessage(hCat2, CB_GETCURSEL, 0, 0);
            if (ci == CB_ERR) ci = 7;
            SendMessage(hCat2, CB_GETLBTEXT, (WPARAM)ci, (LPARAM)catStr);

            ci = (int)SendMessage(hUnit2, CB_GETCURSEL, 0, 0);
            if (ci == CB_ERR) ci = 0;
            SendMessage(hUnit2, CB_GETLBTEXT, (WPARAM)ci, (LPARAM)unitStr);

            ZeroMemory(&ing, sizeof(ing));
            ing.id = iid;
            strncpy(ing.ingredient_name, name,    MAX_INGREDIENT_NAME - 1);
            strncpy(ing.category,        catStr,  MAX_INGREDIENT_CATEGORY - 1);
            strncpy(ing.unit,            unitStr, MAX_INGREDIENT_UNIT - 1);
            ing.cost_per_unit = (float)atof(costStr);

            ci = (int)SendMessage(hSup2, CB_GETCURSEL, 0, 0);
            ing.supplier_id = (ci != CB_ERR)
                ? (int)SendMessage(hSup2, CB_GETITEMDATA, (WPARAM)ci, 0) : 0;
            strncpy(ing.brand, brand, MAX_INGREDIENT_BRAND - 1);
            strncpy(ing.notes, notes, 255);

            rc = (iid == 0) ? db_add_ingredient(&ing) : db_update_ingredient(&ing);

            if (rc == 1) {
                MessageBox(hWnd, "Name already exists.", "Error", MB_ICONWARNING);
                return 0;
            }
            if (rc != 0) {
                MessageBox(hWnd, "Failed to save ingredient.", "Error", MB_ICONERROR);
                return 0;
            }

            g_dlgSaved = TRUE;
            g_dlgDone  = TRUE;
            EnableWindow(GetParent(hWnd), TRUE);
            DestroyWindow(hWnd);
            return 0;
        }

        if (ctl == IDC_ING_DLG_CANCEL) {
            g_dlgDone = TRUE;
            EnableWindow(GetParent(hWnd), TRUE);
            DestroyWindow(hWnd);
            return 0;
        }
    }
    return 0;

    case WM_DESTROY:
        g_dlgDone = TRUE;
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   OpenIngDialog — register class once, run modal loop
   ========================================================================= */
static void OpenIngDialog(HWND hParent, int iid)
{
    static BOOL s_reg = FALSE;
    HWND hDlg;
    MSG  m;

    if (!s_reg) {
        WNDCLASSEX wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.lpfnWndProc   = IngDlgWndProc;
        wc.hInstance     = g_hInst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "IngDlgClass";
        RegisterClassEx(&wc);
        s_reg = TRUE;
    }

    g_dlgDone  = FALSE;
    g_dlgSaved = FALSE;
    EnableWindow(hParent, FALSE);

    hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "IngDlgClass",
        iid ? "Edit Ingredient" : "New Ingredient",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 340,
        hParent, NULL, g_hInst,
        (LPVOID)(INT_PTR)iid);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    while (!g_dlgDone && GetMessage(&m, NULL, 0, 0)) {
        if (!IsDialogMessage(hDlg, &m)) {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
    }

    EnableWindow(hParent, TRUE);
    SetFocus(hParent);
}

/* =========================================================================
   Ingredients panel WndProc
   ========================================================================= */
static LRESULT CALLBACK IngPanelWndProc(HWND hWnd, UINT msg,
                                         WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        CreateWindowEx(0, "BUTTON", "New Ingredient",
            WS_CHILD|WS_VISIBLE, 4, 4, 120, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_NEW_ING, g_hInst, NULL);

        g_hBtnEdit = CreateWindowEx(0, "BUTTON", "Edit Ingredient",
            WS_CHILD|WS_VISIBLE, 132, 4, 120, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_EDIT_ING, g_hInst, NULL);
        EnableWindow(g_hBtnEdit, FALSE);

        g_hBtnDel = CreateWindowEx(0, "BUTTON", "Delete",
            WS_CHILD|WS_VISIBLE, 260, 4, 80, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_DEL_ING, g_hInst, NULL);
        EnableWindow(g_hBtnDel, FALSE);

        /* Search bar */
        CreateWindowEx(0, "STATIC", "Search:",
            WS_CHILD|WS_VISIBLE, 4, 38, 52, 20,
            hWnd, NULL, g_hInst, NULL);
        g_hSearch = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            58, 36, 320, 22,
            hWnd, (HMENU)(INT_PTR)IDC_ING_SEARCH, g_hInst, NULL);

        g_hLV = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
            0, 66, 800, 400,
            hWnd, (HMENU)(INT_PTR)IDC_LV_INGREDIENTS, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hLV,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LV_AddCol(g_hLV, 0, "Name",       180);
        LV_AddCol(g_hLV, 1, "Category",   100);
        LV_AddCol(g_hLV, 2, "Unit",        50);
        LV_AddCol(g_hLV, 3, "Cost/unit",   80);
        LV_AddCol(g_hLV, 4, "Supplier",   140);
        LV_AddCol(g_hLV, 5, "Brand",      120);
    }
    return 0;

    case WM_SIZE:
    {
        WORD w = LOWORD(lParam), h = HIWORD(lParam);
        if (g_hSearch)
            MoveWindow(g_hSearch, 58, 36, w > 380 ? w - 390 : 320, 22, TRUE);
        if (g_hLV)
            MoveWindow(g_hLV, 0, 66, w, h > 66 ? h - 66 : 0, TRUE);
    }
    return 0;

    case WM_COMMAND:
    {
        int ctl = LOWORD(wParam);

        if (ctl == IDC_ING_SEARCH && HIWORD(wParam) == EN_CHANGE) {
            Panel_Ingredients_Refresh();
            return 0;
        }

        if (ctl == IDC_BTN_NEW_ING) {
            OpenIngDialog(hWnd, 0);
            if (g_dlgSaved) Panel_Ingredients_Refresh();
            return 0;
        }
        if (ctl == IDC_BTN_EDIT_ING) {
            if (g_selIngId > 0) {
                OpenIngDialog(hWnd, g_selIngId);
                if (g_dlgSaved) Panel_Ingredients_Refresh();
            }
            return 0;
        }
        if (ctl == IDC_BTN_DEL_ING) {
            if (g_selIngId > 0) {
                int rc;
                if (MessageBox(hWnd, "Delete this ingredient?",
                        "Confirm Delete", MB_YESNO | MB_ICONWARNING) != IDYES)
                    return 0;
                rc = db_delete_ingredient(g_selIngId);
                if (rc == 1) {
                    MessageBox(hWnd,
                        "Cannot delete: ingredient is in use by a formulation or soda base.",
                        "In Use", MB_ICONWARNING);
                } else if (rc != 0) {
                    MessageBox(hWnd, "Delete failed.", "Error", MB_ICONERROR);
                } else {
                    g_selIngId = 0;
                    Panel_Ingredients_Refresh();
                }
            }
            return 0;
        }
    }
    return 0;

    case WM_NOTIFY:
    {
        NMHDR *pnm = (NMHDR *)lParam;
        if (pnm->idFrom == IDC_LV_INGREDIENTS &&
            pnm->code   == LVN_ITEMCHANGED)
        {
            NMLISTVIEW *pnlv = (NMLISTVIEW *)lParam;
            if (pnlv->uNewState & LVIS_SELECTED) {
                LVITEM lvi;
                ZeroMemory(&lvi, sizeof(lvi));
                lvi.mask  = LVIF_PARAM;
                lvi.iItem = pnlv->iItem;
                ListView_GetItem(g_hLV, &lvi);
                g_selIngId = (int)lvi.lParam;
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
   Panel_Ingredients_Create
   ========================================================================= */
HWND Panel_Ingredients_Create(HWND hParent)
{
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.lpfnWndProc   = IngPanelWndProc;
    wc.hInstance     = g_hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "IngredientsPanel";
    RegisterClassEx(&wc);

    g_hPanel = CreateWindowEx(0,
        "IngredientsPanel", NULL,
        WS_CHILD,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}

/* =========================================================================
   Panel_Ingredients_Refresh
   ========================================================================= */
void Panel_Ingredients_Refresh(void)
{
    sqlite3      *db   = db_get_handle();
    sqlite3_stmt *stmt = NULL;
    int           row  = 0;
    char          filter[256];
    char          pat[260];

    if (!g_hLV || !db) return;

    /* Read search bar text; build LIKE pattern */
    if (g_hSearch)
        GetWindowText(g_hSearch, filter, sizeof(filter));
    else
        filter[0] = '\0';

    snprintf(pat, sizeof(pat), "%%%s%%", filter);

    ListView_DeleteAllItems(g_hLV);
    g_selIngId = 0;
    EnableWindow(g_hBtnEdit, FALSE);
    EnableWindow(g_hBtnDel,  FALSE);

    if (sqlite3_prepare_v2(db,
        "SELECT i.id, i.ingredient_name, i.category, i.unit, i.cost_per_unit, "
        "       COALESCE(s.supplier_name,'--'), COALESCE(i.brand,'') "
        "FROM ingredients i "
        "LEFT JOIN suppliers s ON s.id = i.supplier_id "
        "WHERE i.ingredient_name LIKE ? "
        "ORDER BY i.ingredient_name;",
        -1, &stmt, NULL) != SQLITE_OK)
        return;

    sqlite3_bind_text(stmt, 1, pat, -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int         id    = sqlite3_column_int(stmt, 0);
        const char *nm    = (const char *)sqlite3_column_text(stmt, 1);
        const char *cat   = (const char *)sqlite3_column_text(stmt, 2);
        const char *unit  = (const char *)sqlite3_column_text(stmt, 3);
        double      cost  = sqlite3_column_double(stmt, 4);
        const char *sup   = (const char *)sqlite3_column_text(stmt, 5);
        const char *brand = (const char *)sqlite3_column_text(stmt, 6);
        char costbuf[32];
        int actual;

        snprintf(costbuf, sizeof(costbuf), "$%.4f", cost);
        actual = LV_InsertRow(g_hLV, row, nm ? nm : "", (LPARAM)id);
        LV_SetCell(g_hLV, actual, 1, cat   ? cat   : "");
        LV_SetCell(g_hLV, actual, 2, unit  ? unit  : "");
        LV_SetCell(g_hLV, actual, 3, costbuf);
        LV_SetCell(g_hLV, actual, 4, sup   ? sup   : "");
        LV_SetCell(g_hLV, actual, 5, brand ? brand : "");
        row++;
    }
    sqlite3_finalize(stmt);
}
