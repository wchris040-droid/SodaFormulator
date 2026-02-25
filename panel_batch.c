#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ui.h"
#include "database.h"
#include "formulation.h"
#include "batch.h"
#include "sqlite3.h"

/* =========================================================================
   File-scope state
   ========================================================================= */
static HWND g_hPanel       = NULL;
static HWND g_hListView    = NULL;
static HWND g_hFilterCombo = NULL;
static HWND g_hBtnNew;

/* Dialog state */
static BOOL    g_dlgDone;
static BOOL    g_dlgSaved;
static HWND    g_hBatchDlg  = NULL;

/* FDA Label dialog state */
static HWND    g_hBtnFDALabel  = NULL;
static BOOL    g_labelDlgDone  = FALSE;

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
   Populate flavor combo (reused from tasting — same logic)
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

static void DlgPopulateVersionCombo(HWND hVerCombo, const char* flavor)
{
    sqlite3*      db   = db_get_handle();
    sqlite3_stmt* stmt = NULL;

    if (!db || !hVerCombo || !flavor || !flavor[0]) return;

    SendMessage(hVerCombo, CB_RESETCONTENT, 0, 0);

    if (sqlite3_prepare_v2(db,
            "SELECT ver_major, ver_minor, ver_patch FROM formulations "
            "WHERE flavor_code=? ORDER BY ver_major DESC, ver_minor DESC, ver_patch DESC;",
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
   Build auto batch number string (same logic as db_save_batch)
   ========================================================================= */
static void BuildBatchNumber(const char* flavor, char* out, int outLen)
{
    sqlite3*      db   = db_get_handle();
    sqlite3_stmt* stmt = NULL;
    int           seq  = 1;

    if (db && sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM batch_runs br "
            "JOIN formulations f ON f.id = br.formulation_id "
            "WHERE f.flavor_code = ?;",
            -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, flavor, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            seq = sqlite3_column_int(stmt, 0) + 1;
        sqlite3_finalize(stmt);
    }

    snprintf(out, outLen, "%s-2026-%03d", flavor, seq);
}

/* =========================================================================
   Batch dialog WndProc
   ========================================================================= */
static LRESULT CALLBACK BatchDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        HWND hFlvCombo, hVerCombo;
        int  y  = 10;
        int  lx = 10, lw = 90, ex = 105, ew = 160, rh = 22;

        /* Flavor */
        CreateWindowEx(0, "STATIC", "Flavor:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        hFlvCombo = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            ex, y, ew, 200, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_FLV_COMBO, g_hInst, NULL);
        PopulateFlavorCombo(hFlvCombo);
        SendMessage(hFlvCombo, CB_DELETESTRING, 0, 0); /* remove (All) */
        SendMessage(hFlvCombo, CB_SETCURSEL, 0, 0);
        y += 32;

        /* Version */
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
                char batchno[MAX_BATCH_NUMBER];
                SendMessage(hFlvCombo, CB_GETLBTEXT, flvSel, (LPARAM)flvBuf);
                DlgPopulateVersionCombo(hVerCombo, flvBuf);
                /* Pre-fill batch number */
                BuildBatchNumber(flvBuf, batchno, sizeof(batchno));
                SetDlgItemText(hWnd, IDC_DLG_BATCHNO, batchno);
            }
        }
        y += 32;

        /* Volume */
        CreateWindowEx(0, "STATIC", "Volume (L):",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "10.0",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            ex, y, 80, rh, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_VOL, g_hInst, NULL);
        y += 32;

        /* Batch Number */
        CreateWindowEx(0, "STATIC", "Batch No:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            ex, y, ew, rh, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_BATCHNO, g_hInst, NULL);
        y += 36;

        /* [Check Inventory] button */
        CreateWindowEx(0, "BUTTON", "Check Inventory",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx, y, 130, 26, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_CHECK, g_hInst, NULL);
        y += 36;

        /* Status / cost labels */
        CreateWindowEx(0, "STATIC", "Inventory:",
            WS_CHILD | WS_VISIBLE,
            lx, y, 80, 18, hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(0, "STATIC", "(click Check Inventory to verify)",
            WS_CHILD | WS_VISIBLE,
            lx + 85, y, 280, 18, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_STATUS_LBL, g_hInst, NULL);
        y += 26;

        CreateWindowEx(0, "STATIC", "Est. Cost:",
            WS_CHILD | WS_VISIBLE,
            lx, y, 80, 18, hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(0, "STATIC", "--",
            WS_CHILD | WS_VISIBLE,
            lx + 85, y, 200, 18, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_COST_LBL, g_hInst, NULL);
        y += 36;

        /* Save / Cancel */
        CreateWindowEx(0, "BUTTON", "Save && Deduct Inventory",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx, y, 200, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_SAVE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx + 210, y, 80, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_CANCEL, g_hInst, NULL);
    }
    return 0;

    case WM_COMMAND:
    {
        int id     = LOWORD(wParam);
        int notify = HIWORD(wParam);

        if ((id == IDC_DLG_FLV_COMBO && notify == CBN_SELCHANGE) ||
            (id == IDC_DLG_VER_COMBO && notify == CBN_SELCHANGE))
        {
            HWND hFlv = GetDlgItem(hWnd, IDC_DLG_FLV_COMBO);
            HWND hVer = GetDlgItem(hWnd, IDC_DLG_VER_COMBO);
            char flvBuf[MAX_FLAVOR_CODE];
            int  flvSel = (int)SendMessage(hFlv, CB_GETCURSEL, 0, 0);

            if (flvSel >= 0) {
                SendMessage(hFlv, CB_GETLBTEXT, flvSel, (LPARAM)flvBuf);
                if (id == IDC_DLG_FLV_COMBO)
                    DlgPopulateVersionCombo(hVer, flvBuf);
                {
                    char batchno[MAX_BATCH_NUMBER];
                    BuildBatchNumber(flvBuf, batchno, sizeof(batchno));
                    SetDlgItemText(hWnd, IDC_DLG_BATCHNO, batchno);
                }
            }
            break;
        }

        if (id == IDC_BTN_CHECK) {
            HWND hFlv = GetDlgItem(hWnd, IDC_DLG_FLV_COMBO);
            HWND hVer = GetDlgItem(hWnd, IDC_DLG_VER_COMBO);
            char flvBuf[MAX_FLAVOR_CODE];
            char verBuf[16];
            char volStr[16];
            int  flvSel, verSel;
            int  major, minor, patch;
            float vol;
            BatchRun    br;
            char        statusBuf[128];
            char        costBuf[64];

            flvSel = (int)SendMessage(hFlv, CB_GETCURSEL, 0, 0);
            verSel = (int)SendMessage(hVer, CB_GETCURSEL, 0, 0);
            if (flvSel < 0 || verSel < 0) break;

            SendMessage(hFlv, CB_GETLBTEXT, flvSel, (LPARAM)flvBuf);
            SendMessage(hVer, CB_GETLBTEXT, verSel, (LPARAM)verBuf);
            if (sscanf(verBuf, "%d.%d.%d", &major, &minor, &patch) != 3) break;

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_VOL), volStr, sizeof(volStr));
            vol = (float)atof(volStr);
            if (vol <= 0.0f) { MessageBox(hWnd, "Enter a positive volume.", "Input", MB_OK); break; }

            ZeroMemory(&br, sizeof(br));
            {
                FormBase       bases[MAX_FORM_BASES];
                FormIngredient ings[MAX_FORM_INGREDIENTS];
                int            bc = 0, ic = 0;
                db_load_formulation_extras(flvBuf, major, minor, patch,
                                           bases, &bc, ings, &ic);
                batch_calculate_from_ingredients(&br, bases, bc, ings, ic, vol);
            }

            strcpy(statusBuf, "Ingredients calculated.");

            if (br.cost_total >= 0.0f)
                sprintf(costBuf, "$%.4f", br.cost_total);
            else
                strcpy(costBuf, "(missing cost data)");

            SetWindowText(GetDlgItem(hWnd, IDC_DLG_STATUS_LBL), statusBuf);
            SetWindowText(GetDlgItem(hWnd, IDC_DLG_COST_LBL),   costBuf);
            break;
        }

        if (id == IDC_BTN_CANCEL) {
            g_dlgDone = TRUE;
            DestroyWindow(hWnd);
            break;
        }

        if (id == IDC_BTN_SAVE) {
            HWND hFlv = GetDlgItem(hWnd, IDC_DLG_FLV_COMBO);
            HWND hVer = GetDlgItem(hWnd, IDC_DLG_VER_COMBO);
            char flvBuf[MAX_FLAVOR_CODE];
            char verBuf[16];
            char volStr[16];
            char batchnoStr[MAX_BATCH_NUMBER];
            int  flvSel, verSel;
            int  major, minor, patch;
            float vol;
            BatchRun    br;

            flvSel = (int)SendMessage(hFlv, CB_GETCURSEL, 0, 0);
            verSel = (int)SendMessage(hVer, CB_GETCURSEL, 0, 0);
            if (flvSel < 0 || verSel < 0) {
                MessageBox(hWnd, "Select a flavor and version.", "Input", MB_OK);
                break;
            }

            SendMessage(hFlv, CB_GETLBTEXT, flvSel, (LPARAM)flvBuf);
            SendMessage(hVer, CB_GETLBTEXT, verSel, (LPARAM)verBuf);
            if (sscanf(verBuf, "%d.%d.%d", &major, &minor, &patch) != 3) {
                MessageBox(hWnd, "Invalid version.", "Input", MB_OK);
                break;
            }

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_VOL), volStr, sizeof(volStr));
            vol = (float)atof(volStr);
            if (vol <= 0.0f) { MessageBox(hWnd, "Enter a positive volume.", "Input", MB_OK); break; }

            GetWindowText(GetDlgItem(hWnd, IDC_DLG_BATCHNO), batchnoStr, sizeof(batchnoStr));

            ZeroMemory(&br, sizeof(br));
            {
                FormBase       bases[MAX_FORM_BASES];
                FormIngredient ings[MAX_FORM_INGREDIENTS];
                int            bc = 0, ic = 0;
                db_load_formulation_extras(flvBuf, major, minor, patch,
                                           bases, &bc, ings, &ic);
                batch_calculate_from_ingredients(&br, bases, bc, ings, ic, vol);
            }

            if (batchnoStr[0])
                strncpy(br.batch_number, batchnoStr, MAX_BATCH_NUMBER - 1);

            if (db_save_batch(flvBuf, major, minor, patch, &br) != 0) {
                MessageBox(hWnd, "Failed to save batch.", "Error", MB_ICONERROR);
                break;
            }

            g_dlgSaved = TRUE;
            g_dlgDone  = TRUE;
            DestroyWindow(hWnd);
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
   FDA Label Dialog WndProc + opener
   ========================================================================= */

/* Stored batch number for the label dialog (set by OpenFDALabelDialog) */
static char g_labelBatchNo[MAX_BATCH_NUMBER];

static LRESULT CALLBACK FDALabelDlgWndProc(HWND hWnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        int lx = 10, lw = 90, ex = 105, ew = 400, rh = 22;
        int y = 10;
        char saved[256];

        /* Company Name */
        CreateWindowEx(0, "STATIC", "Company:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            ex, y, ew, rh, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_CO_NAME, g_hInst, NULL);
        if (db_get_setting("company_name", saved, sizeof(saved)) && saved[0])
            SetDlgItemText(hWnd, IDC_DLG_CO_NAME, saved);
        y += 32;

        /* Company Address */
        CreateWindowEx(0, "STATIC", "Address:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            ex, y, ew, rh, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_CO_ADDR, g_hInst, NULL);
        if (db_get_setting("company_address", saved, sizeof(saved)) && saved[0])
            SetDlgItemText(hWnd, IDC_DLG_CO_ADDR, saved);
        y += 32;

        /* Container oz + Servings on same row */
        CreateWindowEx(0, "STATIC", "Cont. oz:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        {
            const char *def_oz = "12.0";
            CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", def_oz,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                ex, y, 55, rh, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_CONT_OZ, g_hInst, NULL);
            if (db_get_setting("label_cont_oz", saved, sizeof(saved)) && saved[0])
                SetDlgItemText(hWnd, IDC_DLG_CONT_OZ, saved);
        }
        CreateWindowEx(0, "STATIC", "Servings:",
            WS_CHILD | WS_VISIBLE,
            ex + 65, y + 3, 65, 18, hWnd, NULL, g_hInst, NULL);
        {
            const char *def_sv = "1";
            CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", def_sv,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                ex + 135, y, 40, rh, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_SERVINGS, g_hInst, NULL);
            if (db_get_setting("label_servings", saved, sizeof(saved)) && saved[0])
                SetDlgItemText(hWnd, IDC_DLG_SERVINGS, saved);
        }
        y += 32;

        /* Sweetener + Acid on same row */
        CreateWindowEx(0, "STATIC", "Sweetener:",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            lx, y + 3, lw, 18, hWnd, NULL, g_hInst, NULL);
        {
            const char *def_sw = "Cane Sugar";
            CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", def_sw,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                ex, y, 140, rh, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_SWEETENER, g_hInst, NULL);
            if (db_get_setting("label_sweetener", saved, sizeof(saved)) && saved[0])
                SetDlgItemText(hWnd, IDC_DLG_SWEETENER, saved);
        }
        CreateWindowEx(0, "STATIC", "Acid:",
            WS_CHILD | WS_VISIBLE,
            ex + 150, y + 3, 40, 18, hWnd, NULL, g_hInst, NULL);
        {
            const char *def_ac = "Citric Acid";
            CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", def_ac,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                ex + 195, y, 120, rh, hWnd,
                (HMENU)(INT_PTR)IDC_DLG_ACID, g_hInst, NULL);
            if (db_get_setting("label_acid", saved, sizeof(saved)) && saved[0])
                SetDlgItemText(hWnd, IDC_DLG_ACID, saved);
        }
        y += 32;

        /* Separator */
        CreateWindowEx(0, "STATIC", "",
            WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            lx, y, 670, 2, hWnd, NULL, g_hInst, NULL);
        y += 8;

        /* Preview edit */
        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE |
            ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL |
            ES_AUTOVSCROLL,
            lx, y, 670, 280, hWnd,
            (HMENU)(INT_PTR)IDC_DLG_LABEL_PREVIEW, g_hInst, NULL);
        y += 288;

        /* Buttons */
        CreateWindowEx(0, "BUTTON", "Generate Preview",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx, y, 120, 26, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_GENERATE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Save to File...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx + 130, y, 110, 26, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_SAVE_FILE, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Save Settings",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx + 250, y, 110, 26, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_SAVE_SETTINGS, g_hInst, NULL);
        CreateWindowEx(0, "BUTTON", "Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx + 370, y, 80, 26, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_CANCEL, g_hInst, NULL);
    }
    return 0;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == IDC_BTN_GENERATE)
        {
            LabelConfig     cfg;
            char            ozStr[16], svStr[16];
            char            flavor_name[64], flavor_code[32], ver_str[16];
            double          brix = 0.0;
            sqlite3        *db   = db_get_handle();
            sqlite3_stmt   *stmt = NULL;
            static char     lbuf[8192];

            /* Read fields */
            GetDlgItemText(hWnd, IDC_DLG_CO_NAME,   cfg.company_name,    sizeof(cfg.company_name));
            GetDlgItemText(hWnd, IDC_DLG_CO_ADDR,   cfg.company_address, sizeof(cfg.company_address));
            GetDlgItemText(hWnd, IDC_DLG_CONT_OZ,   ozStr,               sizeof(ozStr));
            GetDlgItemText(hWnd, IDC_DLG_SERVINGS,  svStr,               sizeof(svStr));
            GetDlgItemText(hWnd, IDC_DLG_SWEETENER, cfg.sweetener_name,  sizeof(cfg.sweetener_name));
            GetDlgItemText(hWnd, IDC_DLG_ACID,      cfg.acid_name,       sizeof(cfg.acid_name));

            cfg.container_oz          = (float)atof(ozStr);
            cfg.servings_per_container = atoi(svStr);
            if (cfg.container_oz <= 0.0f) cfg.container_oz = 12.0f;
            if (cfg.servings_per_container <= 0) cfg.servings_per_container = 1;

            /* Query formulation info from batch number */
            flavor_name[0] = '\0';
            flavor_code[0] = '\0';
            ver_str[0]     = '\0';

            if (db && sqlite3_prepare_v2(db,
                    "SELECT f.flavor_name, f.flavor_code, "
                    "       f.ver_major, f.ver_minor, f.ver_patch, f.target_brix "
                    "FROM batch_runs br "
                    "JOIN formulations f ON f.id = br.formulation_id "
                    "WHERE br.batch_number = ?;",
                    -1, &stmt, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(stmt, 1, g_labelBatchNo, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char *fn = (const char*)sqlite3_column_text(stmt, 0);
                    const char *fc = (const char*)sqlite3_column_text(stmt, 1);
                    int maj = sqlite3_column_int(stmt, 2);
                    int min = sqlite3_column_int(stmt, 3);
                    int pat = sqlite3_column_int(stmt, 4);
                    brix = sqlite3_column_double(stmt, 5);
                    if (fn) strncpy(flavor_name, fn, sizeof(flavor_name) - 1);
                    if (fc) strncpy(flavor_code, fc, sizeof(flavor_code) - 1);
                    snprintf(ver_str, sizeof(ver_str), "%d.%d.%d", maj, min, pat);
                }
                sqlite3_finalize(stmt);
            }

            if (!flavor_name[0]) {
                MessageBox(hWnd, "Could not find formulation for this batch.",
                           "Error", MB_ICONERROR);
                break;
            }

            batch_generate_fda_label(
                g_labelBatchNo,
                flavor_name,
                flavor_code,
                ver_str,
                (float)brix,
                &cfg,
                lbuf,
                sizeof(lbuf));

            SetDlgItemText(hWnd, IDC_DLG_LABEL_PREVIEW, lbuf);
            break;
        }

        if (id == IDC_BTN_SAVE_FILE)
        {
            HWND    hPrev = GetDlgItem(hWnd, IDC_DLG_LABEL_PREVIEW);
            int     len   = GetWindowTextLength(hPrev);
            OPENFILENAME ofn;
            char    fname[MAX_PATH];
            char   *buf;
            FILE   *fp;

            if (len == 0) {
                MessageBox(hWnd, "Generate preview first.", "Info", MB_OK);
                break;
            }

            ZeroMemory(&ofn, sizeof(ofn));
            fname[0] = '\0';
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hWnd;
            ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files\0*.*\0";
            ofn.lpstrFile   = fname;
            ofn.nMaxFile    = sizeof(fname);
            ofn.Flags       = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = "txt";

            if (!GetSaveFileName(&ofn)) break;

            buf = (char*)malloc((size_t)(len + 2));
            if (!buf) { MessageBox(hWnd, "Out of memory.", "Error", MB_ICONERROR); break; }
            GetWindowText(hPrev, buf, len + 1);

            fp = fopen(ofn.lpstrFile, "w");
            if (!fp) {
                free(buf);
                MessageBox(hWnd, "Could not open file for writing.", "Error", MB_ICONERROR);
                break;
            }
            fputs(buf, fp);
            fclose(fp);
            free(buf);
            MessageBox(hWnd, "Label saved.", "Saved", MB_OK);
            break;
        }

        if (id == IDC_BTN_SAVE_SETTINGS)
        {
            char co_name[128], co_addr[256];
            char oz_str[16],   sv_str[16];
            char sweetener[64], acid[64];

            GetDlgItemText(hWnd, IDC_DLG_CO_NAME,   co_name,   sizeof(co_name));
            GetDlgItemText(hWnd, IDC_DLG_CO_ADDR,   co_addr,   sizeof(co_addr));
            GetDlgItemText(hWnd, IDC_DLG_CONT_OZ,   oz_str,    sizeof(oz_str));
            GetDlgItemText(hWnd, IDC_DLG_SERVINGS,  sv_str,    sizeof(sv_str));
            GetDlgItemText(hWnd, IDC_DLG_SWEETENER, sweetener, sizeof(sweetener));
            GetDlgItemText(hWnd, IDC_DLG_ACID,      acid,      sizeof(acid));

            db_set_setting("company_name",    co_name);
            db_set_setting("company_address", co_addr);
            db_set_setting("label_cont_oz",   oz_str);
            db_set_setting("label_servings",  sv_str);
            db_set_setting("label_sweetener", sweetener);
            db_set_setting("label_acid",      acid);

            MessageBox(hWnd, "Settings saved.", "Saved", MB_OK);
            break;
        }

        if (id == IDC_BTN_CANCEL) {
            g_labelDlgDone = TRUE;
            DestroyWindow(hWnd);
        }
    }
    return 0;

    case WM_CLOSE:
        g_labelDlgDone = TRUE;
        DestroyWindow(hWnd);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void OpenFDALabelDialog(HWND hParent, const char *batchno)
{
    static BOOL s_registered = FALSE;
    RECT pr;
    int  dw = 700, dh = 530;
    int  cx, cy;
    MSG  msg;

    strncpy(g_labelBatchNo, batchno, MAX_BATCH_NUMBER - 1);
    g_labelBatchNo[MAX_BATCH_NUMBER - 1] = '\0';

    if (!s_registered) {
        WNDCLASSEX wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = FDALabelDlgWndProc;
        wc.hInstance     = g_hInst;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "FDALabelDlg";
        RegisterClassEx(&wc);
        s_registered = TRUE;
    }

    GetWindowRect(hParent, &pr);
    cx = pr.left + (pr.right  - pr.left - dw) / 2;
    cy = pr.top  + (pr.bottom - pr.top  - dh) / 2;

    g_labelDlgDone = FALSE;

    CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "FDALabelDlg",
        "Nutrition Label",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        cx, cy, dw, dh,
        hParent, NULL, g_hInst, NULL);

    EnableWindow(hParent, FALSE);

    while (!g_labelDlgDone) {
        if (!GetMessage(&msg, NULL, 0, 0)) { PostQuitMessage((int)msg.wParam); break; }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
}

/* =========================================================================
   OpenBatchDialog
   ========================================================================= */
static void OpenBatchDialog(HWND hParent)
{
    RECT pr;
    int  dw = 460, dh = 360;
    int  cx, cy;
    MSG  msg;

    GetWindowRect(hParent, &pr);
    cx = pr.left + (pr.right  - pr.left - dw) / 2;
    cy = pr.top  + (pr.bottom - pr.top  - dh) / 2;

    g_dlgDone  = FALSE;
    g_dlgSaved = FALSE;

    g_hBatchDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "BatchDlgClass",
        "New Batch Run",
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
    if (IsWindow(g_hBatchDlg)) DestroyWindow(g_hBatchDlg);
    g_hBatchDlg = NULL;
    SetForegroundWindow(hParent);
}

/* =========================================================================
   Panel WndProc
   ========================================================================= */
static LRESULT CALLBACK BatchPanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        int bx = 4, by = 4;

        CreateWindowEx(0, "STATIC", "Flavor:",
            WS_CHILD | WS_VISIBLE,
            bx, by + 4, 50, 18, hWnd, NULL, g_hInst, NULL);
        g_hFilterCombo = CreateWindowEx(0, "COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            bx + 55, by, 160, 200, hWnd,
            (HMENU)(INT_PTR)IDC_FILTER_COMBO, g_hInst, NULL);

        g_hBtnNew = CreateWindowEx(0, "BUTTON", "New Batch",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            bx + 222, by, 90, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_NEW_BATCH, g_hInst, NULL);

        g_hBtnFDALabel = CreateWindowEx(0, "BUTTON", "Nutrition Label",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
            bx + 318, by, 110, 28, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_FDA_LABEL, g_hInst, NULL);

        g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE |
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 36, 100, 100, hWnd,
            (HMENU)(INT_PTR)IDC_LISTVIEW, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LV_AddCol(g_hListView, 0, "Batch No",   170);
        LV_AddCol(g_hListView, 1, "Flavor",      80);
        LV_AddCol(g_hListView, 2, "Version",     70);
        LV_AddCol(g_hListView, 3, "Volume (L)",  90);
        LV_AddCol(g_hListView, 4, "Cost (USD)",  90);
        LV_AddCol(g_hListView, 5, "Date",        160);
    }
    return 0;

    case WM_SIZE:
    {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        MoveWindow(g_hListView, 0, 36, w, h - 36, TRUE);
    }
    return 0;

    case WM_NOTIFY:
    {
        NMHDR *pnm = (NMHDR*)lParam;
        if (pnm->idFrom == IDC_LISTVIEW && pnm->code == LVN_ITEMCHANGED) {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            EnableWindow(g_hBtnFDALabel, sel >= 0);
        }
    }
    return 0;

    case WM_COMMAND:
    {
        int id     = LOWORD(wParam);
        int notify = HIWORD(wParam);

        if (id == IDC_FILTER_COMBO && notify == CBN_SELCHANGE) {
            Panel_Batch_Refresh();
            break;
        }
        if (id == IDC_BTN_NEW_BATCH) {
            OpenBatchDialog(hWnd);
            if (g_dlgSaved) Panel_Batch_Refresh();
            break;
        }
        if (id == IDC_BTN_FDA_LABEL) {
            int  sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            char batchno[MAX_BATCH_NUMBER];
            if (sel < 0) break;
            ListView_GetItemText(g_hListView, sel, 0, batchno, sizeof(batchno));
            OpenFDALabelDialog(hWnd, batchno);
        }
    }
    return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   Panel_Batch_Create
   ========================================================================= */
HWND Panel_Batch_Create(HWND hParent)
{
    WNDCLASSEX wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = BatchPanelWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "BatchPanel";
    RegisterClassEx(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = BatchDlgWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "BatchDlgClass";
    RegisterClassEx(&wc);

    g_hPanel = CreateWindowEx(0,
        "BatchPanel", NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}

/* =========================================================================
   Panel_Batch_Refresh
   ========================================================================= */
void Panel_Batch_Refresh(void)
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
            "SELECT br.batch_number, f.flavor_code, "
            "       f.ver_major, f.ver_minor, f.ver_patch, "
            "       br.volume_liters, br.cost_total, br.batched_at "
            "FROM batch_runs br "
            "JOIN formulations f ON f.id = br.formulation_id "
            "WHERE f.flavor_code = '%s' "
            "ORDER BY br.batched_at ASC;", filter);
    } else {
        sprintf(sql,
            "SELECT br.batch_number, f.flavor_code, "
            "       f.ver_major, f.ver_minor, f.ver_patch, "
            "       br.volume_liters, br.cost_total, br.batched_at "
            "FROM batch_runs br "
            "JOIN formulations f ON f.id = br.formulation_id "
            "ORDER BY br.batched_at ASC;");
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* bn    = (const char*)sqlite3_column_text(stmt, 0);
        const char* code  = (const char*)sqlite3_column_text(stmt, 1);

        LV_InsertRow(g_hListView, row, bn ? bn : "");
        LV_SetCell(g_hListView, row, 1, code ? code : "");

        sprintf(buf, "%d.%d.%d",
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4));
        LV_SetCell(g_hListView, row, 2, buf);

        sprintf(buf, "%.2f", sqlite3_column_double(stmt, 5));
        LV_SetCell(g_hListView, row, 3, buf);

        if (sqlite3_column_type(stmt, 6) == SQLITE_NULL) {
            strcpy(buf, "--");
        } else {
            sprintf(buf, "$%.4f", sqlite3_column_double(stmt, 6));
        }
        LV_SetCell(g_hListView, row, 4, buf);

        {
            const char* d = (const char*)sqlite3_column_text(stmt, 7);
            LV_SetCell(g_hListView, row, 5, d ? d : "");
        }

        row++;
    }

    sqlite3_finalize(stmt);
}
