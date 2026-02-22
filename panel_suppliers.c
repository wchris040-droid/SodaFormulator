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
static HWND g_hPanel        = NULL;
static HWND g_hLVSupplier   = NULL;
static HWND g_hLVLinks      = NULL;
static HWND g_hBtnEditSup   = NULL;
static HWND g_hBtnDelSup    = NULL;
static HWND g_hBtnAddLink   = NULL;
static HWND g_hBtnEditLink  = NULL;
static HWND g_hBtnRemoveLink = NULL;

static BOOL g_dlgDone;
static BOOL g_dlgSaved;

static int g_selSupplierId = 0;
static int g_selLinkId     = 0;

/* Local dialog control IDs */
#define IDC_SUP_NAME      4100
#define IDC_SUP_WEBSITE   4101
#define IDC_SUP_EMAIL     4102
#define IDC_SUP_PHONE     4103
#define IDC_SUP_NOTES     4104

#define IDC_LINK_CPD      4110
#define IDC_LINK_PRICE    4111
#define IDC_LINK_MINORDER 4112
#define IDC_LINK_LEADTIME 4113
#define IDC_LINK_CATALOG  4114
#define IDC_LINK_UPDCOST  4115

/* Save/Cancel shared IDs for both dialogs */
#define IDC_DLG_SAVE   4190
#define IDC_DLG_CANCEL 4191

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
   RefreshLinks — repopulate the compound-links ListView for a supplier
   ========================================================================= */
static void RefreshLinks(int supplier_id)
{
    sqlite3      *db   = db_get_handle();
    sqlite3_stmt *stmt = NULL;
    int           row  = 0;

    ListView_DeleteAllItems(g_hLVLinks);
    EnableWindow(g_hBtnEditLink,   FALSE);
    EnableWindow(g_hBtnRemoveLink, FALSE);
    g_selLinkId = 0;

    if (supplier_id == 0 || !db) return;

    if (sqlite3_prepare_v2(db,
        "SELECT cs.id, cl.compound_name, cs.price_per_gram, "
        "       cs.min_order_grams, cs.lead_time_days, "
        "       COALESCE(cs.catalog_number, '') "
        "FROM compound_suppliers cs "
        "JOIN compound_library cl ON cl.id = cs.compound_library_id "
        "WHERE cs.supplier_id = ? "
        "ORDER BY cl.compound_name;",
        -1, &stmt, NULL) != SQLITE_OK)
        return;

    sqlite3_bind_int(stmt, 1, supplier_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int         cs_id   = sqlite3_column_int(stmt, 0);
        const char *name    = (const char *)sqlite3_column_text(stmt, 1);
        double      price   = sqlite3_column_double(stmt, 2);
        double      minord  = sqlite3_column_double(stmt, 3);
        int         lead    = sqlite3_column_int(stmt, 4);
        const char *cat     = (const char *)sqlite3_column_text(stmt, 5);
        char        pbuf[32], mbuf[32], lbuf[16];

        if (price > 0.0)
            snprintf(pbuf, sizeof(pbuf), "$%.4f", price);
        else
            strncpy(pbuf, "--", sizeof(pbuf));

        snprintf(mbuf, sizeof(mbuf), "%.0f g", minord);
        snprintf(lbuf, sizeof(lbuf), "%d d", lead);

        int actual = LV_InsertRow(g_hLVLinks, row, name ? name : "", (LPARAM)cs_id);
        LV_SetCell(g_hLVLinks, actual, 1, pbuf);
        LV_SetCell(g_hLVLinks, actual, 2, mbuf);
        LV_SetCell(g_hLVLinks, actual, 3, lbuf);
        LV_SetCell(g_hLVLinks, actual, 4, cat  ? cat  : "");
        row++;
    }
    sqlite3_finalize(stmt);
}

/* =========================================================================
   Supplier dialog (add / edit)
   lpCreateParams: 0 = add, nonzero = supplier id to edit
   ========================================================================= */
static LRESULT CALLBACK SupplierDlgWndProc(HWND hWnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        CREATESTRUCT *cs  = (CREATESTRUCT *)lParam;
        int           sid = (int)(INT_PTR)cs->lpCreateParams;

        /* Labels + edits */
        CreateWindowEx(0, "STATIC", "Name:",
            WS_CHILD | WS_VISIBLE, 10, 13, 110, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            120, 10, 300, 22,
            hWnd, (HMENU)(INT_PTR)IDC_SUP_NAME, g_hInst, NULL);

        CreateWindowEx(0, "STATIC", "Website:",
            WS_CHILD | WS_VISIBLE, 10, 45, 110, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            120, 42, 300, 22,
            hWnd, (HMENU)(INT_PTR)IDC_SUP_WEBSITE, g_hInst, NULL);

        CreateWindowEx(0, "STATIC", "Email:",
            WS_CHILD | WS_VISIBLE, 10, 77, 110, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            120, 74, 300, 22,
            hWnd, (HMENU)(INT_PTR)IDC_SUP_EMAIL, g_hInst, NULL);

        CreateWindowEx(0, "STATIC", "Phone:",
            WS_CHILD | WS_VISIBLE, 10, 109, 110, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            120, 106, 300, 22,
            hWnd, (HMENU)(INT_PTR)IDC_SUP_PHONE, g_hInst, NULL);

        CreateWindowEx(0, "STATIC", "Notes:",
            WS_CHILD | WS_VISIBLE, 10, 141, 110, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
            120, 138, 300, 60,
            hWnd, (HMENU)(INT_PTR)IDC_SUP_NOTES, g_hInst, NULL);

        CreateWindowEx(0, "BUTTON", "Save",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            120, 212, 80, 26,
            hWnd, (HMENU)(INT_PTR)IDC_DLG_SAVE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE,
            210, 212, 80, 26,
            hWnd, (HMENU)(INT_PTR)IDC_DLG_CANCEL, g_hInst, NULL);

        /* Pre-fill if editing */
        if (sid != 0) {
            sqlite3      *db   = db_get_handle();
            sqlite3_stmt *qst  = NULL;
            if (db && sqlite3_prepare_v2(db,
                "SELECT supplier_name, website, email, phone, notes "
                "FROM suppliers WHERE id=?;",
                -1, &qst, NULL) == SQLITE_OK) {
                sqlite3_bind_int(qst, 1, sid);
                if (sqlite3_step(qst) == SQLITE_ROW) {
                    const char *v;
                    v = (const char *)sqlite3_column_text(qst, 0);
                    SetWindowText(GetDlgItem(hWnd, IDC_SUP_NAME),    v ? v : "");
                    v = (const char *)sqlite3_column_text(qst, 1);
                    SetWindowText(GetDlgItem(hWnd, IDC_SUP_WEBSITE), v ? v : "");
                    v = (const char *)sqlite3_column_text(qst, 2);
                    SetWindowText(GetDlgItem(hWnd, IDC_SUP_EMAIL),   v ? v : "");
                    v = (const char *)sqlite3_column_text(qst, 3);
                    SetWindowText(GetDlgItem(hWnd, IDC_SUP_PHONE),   v ? v : "");
                    v = (const char *)sqlite3_column_text(qst, 4);
                    SetWindowText(GetDlgItem(hWnd, IDC_SUP_NOTES),   v ? v : "");
                }
                sqlite3_finalize(qst);
            }
            SetWindowText(hWnd, "Edit Supplier");
        }

        /* Store supplier id in window's user data */
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)sid);
    }
    return 0;

    case WM_COMMAND:
    {
        int ctl = LOWORD(wParam);

        if (ctl == IDC_DLG_SAVE) {
            char name[128], website[256], email[128], phone[64], notes[512];
            int  sid = (int)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            int  rc;

            GetWindowText(GetDlgItem(hWnd, IDC_SUP_NAME),    name,    sizeof(name));
            GetWindowText(GetDlgItem(hWnd, IDC_SUP_WEBSITE), website, sizeof(website));
            GetWindowText(GetDlgItem(hWnd, IDC_SUP_EMAIL),   email,   sizeof(email));
            GetWindowText(GetDlgItem(hWnd, IDC_SUP_PHONE),   phone,   sizeof(phone));
            GetWindowText(GetDlgItem(hWnd, IDC_SUP_NOTES),   notes,   sizeof(notes));

            if (!name[0]) {
                MessageBox(hWnd, "Supplier name is required.", "Error", MB_ICONWARNING);
                return 0;
            }

            if (sid == 0)
                rc = db_add_supplier(name, website, email, phone, notes);
            else
                rc = db_update_supplier(sid, name, website, email, phone, notes);

            if (rc == 1) {
                MessageBox(hWnd, "Name already exists.", "Error", MB_ICONWARNING);
                return 0;
            }
            if (rc != 0) {
                MessageBox(hWnd, "Failed to save supplier.", "Error", MB_ICONERROR);
                return 0;
            }

            g_dlgSaved = TRUE;
            g_dlgDone  = TRUE;
            EnableWindow(GetParent(hWnd), TRUE);
            DestroyWindow(hWnd);
            return 0;
        }

        if (ctl == IDC_DLG_CANCEL) {
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
   Compound-link dialog (add / edit)
   lpCreateParams: pointer to int[2] = { supplier_id, cs_id }
     cs_id == 0 → add mode, else → edit mode
   ========================================================================= */
static LRESULT CALLBACK LinkDlgWndProc(HWND hWnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        CREATESTRUCT *cs   = (CREATESTRUCT *)lParam;
        int          *args = (int *)cs->lpCreateParams;
        int           sid  = args[0];
        int           csid = args[1];

        /* Store both IDs */
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)sid);
        /* Store cs_id in a static variable via title hack — use extra bytes */
        /* We'll retrieve csid from IDC_LINK_CPD static text if edit mode */

        /* Compound row */
        CreateWindowEx(0, "STATIC", "Compound:",
            WS_CHILD | WS_VISIBLE, 10, 13, 120, 18,
            hWnd, NULL, g_hInst, NULL);

        if (csid == 0) {
            /* Add mode: dropdown */
            sqlite3      *db   = db_get_handle();
            sqlite3_stmt *qst  = NULL;
            HWND hCombo = CreateWindowEx(0, "COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                140, 10, 220, 200,
                hWnd, (HMENU)(INT_PTR)IDC_LINK_CPD, g_hInst, NULL);
            if (db && sqlite3_prepare_v2(db,
                "SELECT compound_name FROM compound_library ORDER BY compound_name;",
                -1, &qst, NULL) == SQLITE_OK) {
                while (sqlite3_step(qst) == SQLITE_ROW) {
                    const char *n = (const char *)sqlite3_column_text(qst, 0);
                    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)n);
                }
                sqlite3_finalize(qst);
            }
            SendMessage(hCombo, CB_SETCURSEL, 0, 0);
            SetWindowText(hWnd, "Add Compound Link");
        } else {
            /* Edit mode: static label */
            CreateWindowEx(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE,
                140, 13, 220, 18,
                hWnd, (HMENU)(INT_PTR)IDC_LINK_CPD, g_hInst, NULL);
            SetWindowText(hWnd, "Edit Compound Link");
        }

        CreateWindowEx(0, "STATIC", "Price/g ($):",
            WS_CHILD | WS_VISIBLE, 10, 45, 120, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "0.00",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 42, 120, 22,
            hWnd, (HMENU)(INT_PTR)IDC_LINK_PRICE, g_hInst, NULL);

        CreateWindowEx(0, "STATIC", "Min Order (g):",
            WS_CHILD | WS_VISIBLE, 10, 77, 120, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "25",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 74, 120, 22,
            hWnd, (HMENU)(INT_PTR)IDC_LINK_MINORDER, g_hInst, NULL);

        CreateWindowEx(0, "STATIC", "Lead Time (days):",
            WS_CHILD | WS_VISIBLE, 10, 109, 120, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "14",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 106, 120, 22,
            hWnd, (HMENU)(INT_PTR)IDC_LINK_LEADTIME, g_hInst, NULL);

        CreateWindowEx(0, "STATIC", "Catalog #:",
            WS_CHILD | WS_VISIBLE, 10, 141, 120, 18,
            hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 138, 200, 22,
            hWnd, (HMENU)(INT_PTR)IDC_LINK_CATALOG, g_hInst, NULL);

        CreateWindowEx(0, "BUTTON",
            "Update compound library cost to this price",
            WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_AUTOCHECKBOX,
            10, 172, 340, 20,
            hWnd, (HMENU)(INT_PTR)IDC_LINK_UPDCOST, g_hInst, NULL);

        CreateWindowEx(0, "BUTTON", "Save",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            140, 204, 80, 26,
            hWnd, (HMENU)(INT_PTR)IDC_DLG_SAVE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE,
            230, 204, 80, 26,
            hWnd, (HMENU)(INT_PTR)IDC_DLG_CANCEL, g_hInst, NULL);

        /* Pre-fill if editing */
        if (csid != 0) {
            sqlite3      *db  = db_get_handle();
            sqlite3_stmt *qst = NULL;
            if (db && sqlite3_prepare_v2(db,
                "SELECT cs.catalog_number, cs.price_per_gram, "
                "       cs.min_order_grams, cs.lead_time_days, "
                "       cl.compound_name "
                "FROM compound_suppliers cs "
                "JOIN compound_library cl ON cl.id = cs.compound_library_id "
                "WHERE cs.id=?;",
                -1, &qst, NULL) == SQLITE_OK) {
                sqlite3_bind_int(qst, 1, csid);
                if (sqlite3_step(qst) == SQLITE_ROW) {
                    char buf[64];
                    const char *cat  = (const char *)sqlite3_column_text(qst, 0);
                    double      pr   = sqlite3_column_double(qst, 1);
                    double      mo   = sqlite3_column_double(qst, 2);
                    int         lead = sqlite3_column_int(qst, 3);
                    const char *cpdname = (const char *)sqlite3_column_text(qst, 4);

                    SetWindowText(GetDlgItem(hWnd, IDC_LINK_CPD), cpdname ? cpdname : "");
                    snprintf(buf, sizeof(buf), "%.4f", pr);
                    SetWindowText(GetDlgItem(hWnd, IDC_LINK_PRICE), buf);
                    snprintf(buf, sizeof(buf), "%.0f", mo);
                    SetWindowText(GetDlgItem(hWnd, IDC_LINK_MINORDER), buf);
                    snprintf(buf, sizeof(buf), "%d", lead);
                    SetWindowText(GetDlgItem(hWnd, IDC_LINK_LEADTIME), buf);
                    SetWindowText(GetDlgItem(hWnd, IDC_LINK_CATALOG), cat ? cat : "");
                }
                sqlite3_finalize(qst);
            }
            /* Store csid in the extra bytes slot — use window title tag via GWLP */
            /* We'll pass csid via a second SetWindowLongPtr slot trick:
               store csid in the STATIC control's ID field isn't possible,
               so store in window's LONG extra (we registered with cbWndExtra=8) */
        }

        /* Store csid separately via SetProp */
        SetProp(hWnd, "csid", (HANDLE)(INT_PTR)csid);
    }
    return 0;

    case WM_COMMAND:
    {
        int ctl = LOWORD(wParam);

        if (ctl == IDC_DLG_SAVE) {
            char   prStr[32], moStr[32], ldStr[16], cat[128], cpdname[128];
            int    sid   = (int)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            int    csid  = (int)(INT_PTR)GetProp(hWnd, "csid");
            float  pr, mo;
            int    lead;
            BOOL   updCost;
            int    rc;

            GetWindowText(GetDlgItem(hWnd, IDC_LINK_PRICE),    prStr,   sizeof(prStr));
            GetWindowText(GetDlgItem(hWnd, IDC_LINK_MINORDER), moStr,   sizeof(moStr));
            GetWindowText(GetDlgItem(hWnd, IDC_LINK_LEADTIME), ldStr,   sizeof(ldStr));
            GetWindowText(GetDlgItem(hWnd, IDC_LINK_CATALOG),  cat,     sizeof(cat));

            pr   = (float)atof(prStr);
            mo   = (float)atof(moStr);
            lead = atoi(ldStr);
            updCost = (SendMessage(GetDlgItem(hWnd, IDC_LINK_UPDCOST),
                                   BM_GETCHECK, 0, 0) == BST_CHECKED);

            if (csid == 0) {
                /* Add mode — get compound from combo */
                HWND hCombo = GetDlgItem(hWnd, IDC_LINK_CPD);
                int  idx    = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                if (idx == CB_ERR) {
                    MessageBox(hWnd, "Select a compound.", "Error", MB_ICONWARNING);
                    return 0;
                }
                SendMessage(hCombo, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)cpdname);
                rc = db_add_compound_supplier(sid, cpdname, cat, pr, mo, lead);
                if (rc == 1) {
                    MessageBox(hWnd, "Compound not found in library.", "Error", MB_ICONWARNING);
                    return 0;
                }
                if (rc == 2) {
                    MessageBox(hWnd, "This compound is already linked to this supplier.", "Error", MB_ICONWARNING);
                    return 0;
                }
            } else {
                /* Edit mode */
                GetWindowText(GetDlgItem(hWnd, IDC_LINK_CPD), cpdname, sizeof(cpdname));
                rc = db_update_compound_supplier(csid, cat, pr, mo, lead);
            }

            if (rc != 0 && rc != 1 && rc != 2) {
                MessageBox(hWnd, "Failed to save compound link.", "Error", MB_ICONERROR);
                return 0;
            }

            if (updCost && pr > 0.0f && cpdname[0])
                db_set_compound_cost(cpdname, pr);

            g_dlgSaved = TRUE;
            g_dlgDone  = TRUE;
            RemoveProp(hWnd, "csid");
            EnableWindow(GetParent(hWnd), TRUE);
            DestroyWindow(hWnd);
            return 0;
        }

        if (ctl == IDC_DLG_CANCEL) {
            g_dlgDone = TRUE;
            RemoveProp(hWnd, "csid");
            EnableWindow(GetParent(hWnd), TRUE);
            DestroyWindow(hWnd);
            return 0;
        }
    }
    return 0;

    case WM_DESTROY:
        RemoveProp(hWnd, "csid");
        g_dlgDone = TRUE;
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   OpenSupplierDialog
   ========================================================================= */
static void OpenSupplierDialog(HWND hParent, int supplier_id)
{
    static BOOL s_reg = FALSE;
    HWND        hDlg;
    MSG         m;

    if (!s_reg) {
        WNDCLASSEX wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.lpfnWndProc   = SupplierDlgWndProc;
        wc.hInstance     = g_hInst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "SupplierDlgClass";
        RegisterClassEx(&wc);
        s_reg = TRUE;
    }

    g_dlgDone  = FALSE;
    g_dlgSaved = FALSE;
    EnableWindow(hParent, FALSE);

    hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "SupplierDlgClass",
        "Add Supplier",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 260,
        hParent, NULL, g_hInst,
        (LPVOID)(INT_PTR)supplier_id);

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
   OpenLinkDialog
   ========================================================================= */
static void OpenLinkDialog(HWND hParent, int supplier_id, int cs_id)
{
    static BOOL s_reg = FALSE;
    int         args[2];
    HWND        hDlg;
    MSG         m;

    if (!s_reg) {
        WNDCLASSEX wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.lpfnWndProc   = LinkDlgWndProc;
        wc.hInstance     = g_hInst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "LinkDlgClass";
        RegisterClassEx(&wc);
        s_reg = TRUE;
    }

    args[0] = supplier_id;
    args[1] = cs_id;

    g_dlgDone  = FALSE;
    g_dlgSaved = FALSE;
    EnableWindow(hParent, FALSE);

    hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "LinkDlgClass",
        "Compound Link",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 390, 250,
        hParent, NULL, g_hInst,
        (LPVOID)args);

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
   Suppliers panel WndProc
   ========================================================================= */
static LRESULT CALLBACK SuppliersPanelWndProc(HWND hWnd, UINT msg,
                                               WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        /* Top toolbar row */
        CreateWindowEx(0, "BUTTON", "Add Supplier",
            WS_CHILD | WS_VISIBLE,
            4, 4, 100, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_NEW_SUPPLIER, g_hInst, NULL);

        g_hBtnEditSup = CreateWindowEx(0, "BUTTON", "Edit Supplier",
            WS_CHILD | WS_VISIBLE,
            112, 4, 100, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_EDIT_SUPPLIER, g_hInst, NULL);
        EnableWindow(g_hBtnEditSup, FALSE);

        g_hBtnDelSup = CreateWindowEx(0, "BUTTON", "Delete Supplier",
            WS_CHILD | WS_VISIBLE,
            220, 4, 110, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_DEL_SUPPLIER, g_hInst, NULL);
        EnableWindow(g_hBtnDelSup, FALSE);

        /* Supplier ListView */
        g_hLVSupplier = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
            0, 36, 700, 200,
            hWnd, (HMENU)(INT_PTR)IDC_LV_SUPPLIERS, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hLVSupplier,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LV_AddCol(g_hLVSupplier, 0, "Name",    180);
        LV_AddCol(g_hLVSupplier, 1, "Website", 150);
        LV_AddCol(g_hLVSupplier, 2, "Email",   160);
        LV_AddCol(g_hLVSupplier, 3, "Phone",   110);

        /* Link toolbar row */
        g_hBtnAddLink = CreateWindowEx(0, "BUTTON", "Add Compound Link",
            WS_CHILD | WS_VISIBLE,
            4, 242, 128, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_ADD_LINK, g_hInst, NULL);
        EnableWindow(g_hBtnAddLink, FALSE);

        g_hBtnEditLink = CreateWindowEx(0, "BUTTON", "Edit Link",
            WS_CHILD | WS_VISIBLE,
            140, 242, 80, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_EDIT_LINK, g_hInst, NULL);
        EnableWindow(g_hBtnEditLink, FALSE);

        g_hBtnRemoveLink = CreateWindowEx(0, "BUTTON", "Remove Link",
            WS_CHILD | WS_VISIBLE,
            228, 242, 90, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_REMOVE_LINK, g_hInst, NULL);
        EnableWindow(g_hBtnRemoveLink, FALSE);

        /* Compound links ListView */
        g_hLVLinks = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
            0, 272, 700, 200,
            hWnd, (HMENU)(INT_PTR)IDC_LV_COMP_LINKS, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hLVLinks,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LV_AddCol(g_hLVLinks, 0, "Compound",    180);
        LV_AddCol(g_hLVLinks, 1, "Price/g",      70);
        LV_AddCol(g_hLVLinks, 2, "Min Order",    80);
        LV_AddCol(g_hLVLinks, 3, "Lead (days)",  80);
        LV_AddCol(g_hLVLinks, 4, "Catalog #",   120);
    }
    return 0;

    case WM_SIZE:
    {
        WORD w = LOWORD(lParam);
        WORD h = HIWORD(lParam);
        if (g_hLVSupplier)
            MoveWindow(g_hLVSupplier, 0, 36, w, 200, TRUE);
        if (g_hLVLinks)
            MoveWindow(g_hLVLinks, 0, 272, w, h > 272 ? h - 272 : 0, TRUE);
    }
    return 0;

    case WM_COMMAND:
    {
        int ctl = LOWORD(wParam);

        if (ctl == IDC_BTN_NEW_SUPPLIER) {
            OpenSupplierDialog(hWnd, 0);
            if (g_dlgSaved) Panel_Suppliers_Refresh();
            return 0;
        }
        if (ctl == IDC_BTN_EDIT_SUPPLIER) {
            if (g_selSupplierId > 0) {
                OpenSupplierDialog(hWnd, g_selSupplierId);
                if (g_dlgSaved) Panel_Suppliers_Refresh();
            }
            return 0;
        }
        if (ctl == IDC_BTN_DEL_SUPPLIER) {
            if (g_selSupplierId > 0) {
                if (MessageBox(hWnd,
                    "Delete this supplier and all its compound links?",
                    "Confirm Delete", MB_YESNO | MB_ICONWARNING) == IDYES) {
                    db_delete_supplier(g_selSupplierId);
                    g_selSupplierId = 0;
                    Panel_Suppliers_Refresh();
                }
            }
            return 0;
        }
        if (ctl == IDC_BTN_ADD_LINK) {
            if (g_selSupplierId > 0) {
                OpenLinkDialog(hWnd, g_selSupplierId, 0);
                if (g_dlgSaved) RefreshLinks(g_selSupplierId);
            }
            return 0;
        }
        if (ctl == IDC_BTN_EDIT_LINK) {
            if (g_selSupplierId > 0 && g_selLinkId > 0) {
                OpenLinkDialog(hWnd, g_selSupplierId, g_selLinkId);
                if (g_dlgSaved) RefreshLinks(g_selSupplierId);
            }
            return 0;
        }
        if (ctl == IDC_BTN_REMOVE_LINK) {
            if (g_selLinkId > 0) {
                if (MessageBox(hWnd,
                    "Remove this compound link?",
                    "Confirm Remove", MB_YESNO | MB_ICONWARNING) == IDYES) {
                    db_remove_compound_supplier(g_selLinkId);
                    g_selLinkId = 0;
                    RefreshLinks(g_selSupplierId);
                }
            }
            return 0;
        }
    }
    return 0;

    case WM_NOTIFY:
    {
        NMHDR *pnm = (NMHDR *)lParam;

        if (pnm->idFrom == IDC_LV_SUPPLIERS &&
            pnm->code == LVN_ITEMCHANGED)
        {
            NMLISTVIEW *pnlv = (NMLISTVIEW *)lParam;
            if (pnlv->uNewState & LVIS_SELECTED) {
                LVITEM lvi;
                ZeroMemory(&lvi, sizeof(lvi));
                lvi.mask   = LVIF_PARAM;
                lvi.iItem  = pnlv->iItem;
                ListView_GetItem(g_hLVSupplier, &lvi);
                g_selSupplierId = (int)lvi.lParam;
                EnableWindow(g_hBtnEditSup, TRUE);
                EnableWindow(g_hBtnDelSup,  TRUE);
                EnableWindow(g_hBtnAddLink, TRUE);
                RefreshLinks(g_selSupplierId);
            }
            return 0;
        }

        if (pnm->idFrom == IDC_LV_COMP_LINKS &&
            pnm->code == LVN_ITEMCHANGED)
        {
            NMLISTVIEW *pnlv = (NMLISTVIEW *)lParam;
            if (pnlv->uNewState & LVIS_SELECTED) {
                LVITEM lvi;
                ZeroMemory(&lvi, sizeof(lvi));
                lvi.mask  = LVIF_PARAM;
                lvi.iItem = pnlv->iItem;
                ListView_GetItem(g_hLVLinks, &lvi);
                g_selLinkId = (int)lvi.lParam;
                EnableWindow(g_hBtnEditLink,   TRUE);
                EnableWindow(g_hBtnRemoveLink, TRUE);
            }
            return 0;
        }
    }
    return CDRF_DODEFAULT;

    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   Panel_Suppliers_Create
   ========================================================================= */
HWND Panel_Suppliers_Create(HWND hParent)
{
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.lpfnWndProc   = SuppliersPanelWndProc;
    wc.hInstance     = g_hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "SuppliersPanelClass";
    RegisterClassEx(&wc);

    g_hPanel = CreateWindowEx(0,
        "SuppliersPanelClass", NULL,
        WS_CHILD,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}

/* =========================================================================
   Panel_Suppliers_Refresh
   ========================================================================= */
void Panel_Suppliers_Refresh(void)
{
    sqlite3      *db   = db_get_handle();
    sqlite3_stmt *stmt = NULL;
    int           row  = 0;

    if (!g_hLVSupplier || !db) return;

    ListView_DeleteAllItems(g_hLVSupplier);
    g_selSupplierId = 0;
    EnableWindow(g_hBtnEditSup, FALSE);
    EnableWindow(g_hBtnDelSup,  FALSE);
    EnableWindow(g_hBtnAddLink, FALSE);
    RefreshLinks(0);

    if (sqlite3_prepare_v2(db,
        "SELECT id, supplier_name, "
        "       COALESCE(website, ''), "
        "       COALESCE(email, ''), "
        "       COALESCE(phone, '') "
        "FROM suppliers ORDER BY supplier_name;",
        -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int         id   = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *web  = (const char *)sqlite3_column_text(stmt, 2);
        const char *eml  = (const char *)sqlite3_column_text(stmt, 3);
        const char *ph   = (const char *)sqlite3_column_text(stmt, 4);
        int actual = LV_InsertRow(g_hLVSupplier, row, name ? name : "", (LPARAM)id);
        LV_SetCell(g_hLVSupplier, actual, 1, web ? web : "");
        LV_SetCell(g_hLVSupplier, actual, 2, eml ? eml : "");
        LV_SetCell(g_hLVSupplier, actual, 3, ph  ? ph  : "");
        row++;
    }
    sqlite3_finalize(stmt);
}
