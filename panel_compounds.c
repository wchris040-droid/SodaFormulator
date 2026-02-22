#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ui.h"
#include "database.h"
#include "compound.h"
#include "sqlite3.h"

/* =========================================================================
   Panel-internal constants
   ========================================================================= */
#define DETAIL_W        260
#define TOP_BAR_H        36
#define IDC_SEARCH_EDIT 2010
#define IDC_SOLUB_COMBO 2011
#define IDC_BTN_POPOUT  2012

/* =========================================================================
   File-scope state
   ========================================================================= */
static HWND g_hPanel      = NULL;
static HWND g_hListView   = NULL;
static HWND g_hBtnEditCost;

static HWND g_hDetailPane = NULL;
static HWND g_hBtnPopOut  = NULL;
static HWND g_hSearchEdit = NULL;
static HWND g_hSolubCombo = NULL;
static HWND g_hAppCombo   = NULL;

static CompoundInfo g_selectedCompound;
static BOOL         g_hasSelection = FALSE;

static HFONT g_hFontBold   = NULL;
static HFONT g_hFontNormal = NULL;
static HWND  g_hPopOuts[8];
static int   g_nPopOuts = 0;

/* Dialog state */
static BOOL g_dlgDone;
static BOOL g_dlgSaved;
static char g_dlgCompoundName[128];
static HWND g_hCostDlg = NULL;

/* Forward declarations */
static void OpenPopOutWindow(const CompoundInfo* c);
static void DrawCompoundDetail(HDC hdc, const RECT* prc, const CompoundInfo* c);
static LRESULT CALLBACK DetailPaneWndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK PopOutWndProc(HWND, UINT, WPARAM, LPARAM);

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
   DrawCompoundDetail — shared helper for detail pane and pop-out windows.
   Background must already be filled before calling.
   ========================================================================= */
static void DrawCompoundDetail(HDC hdc, const RECT* prc, const CompoundInfo* c)
{
    int  y = 8;
    char buf[512];
    HFONT hOldFont;

    SetBkMode(hdc, TRANSPARENT);

    /* Compound name — bold, blue */
    hOldFont = NULL;
    if (g_hFontBold) hOldFont = (HFONT)SelectObject(hdc, g_hFontBold);
    SetTextColor(hdc, RGB(0, 70, 160));
    {
        RECT tr;
        tr.left = 8; tr.top = y; tr.right = prc->right - 4; tr.bottom = y + 1000;
        DrawText(hdc, c->compound_name, -1, &tr,
                 DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        DrawText(hdc, c->compound_name, -1, &tr,
                 DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        y += (tr.bottom - tr.top) + 4;
    }

    /* CAS | FEMA */
    if (g_hFontNormal) SelectObject(hdc, g_hFontNormal);
    SetTextColor(hdc, RGB(80, 80, 80));
    {
        RECT tr;
        if (c->fema_number > 0)
            sprintf(buf, "CAS %s  |  FEMA %d", c->cas_number, c->fema_number);
        else
            sprintf(buf, "CAS %s", c->cas_number);
        tr.left = 8; tr.top = y; tr.right = prc->right - 4; tr.bottom = y + 1000;
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        y += (tr.bottom - tr.top) + 4;
    }

    /* Separator line */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 8, y, NULL);
        LineTo(hdc, prc->right - 8, y);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);
        y += 6;
    }

    SetTextColor(hdc, RGB(30, 30, 30));

/* Macro: draw a labelled text field with word-wrap */
#define DRAW_FIELD(label, value) \
    do { \
        RECT tr_; \
        sprintf(buf, "%s%s", (label), (value)); \
        tr_.left = 8; tr_.top = y; tr_.right = prc->right - 4; tr_.bottom = y + 1000; \
        DrawText(hdc, buf, -1, &tr_, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX); \
        DrawText(hdc, buf, -1, &tr_, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX); \
        y += (tr_.bottom - tr_.top) + 3; \
    } while(0)

    /* Applications */
    if (c->applications[0]) {
        SetTextColor(hdc, RGB(0, 110, 100));
        DRAW_FIELD("Apps: ", c->applications);
        SetTextColor(hdc, RGB(30, 30, 30));
    }

    /* Odor profile */
    DRAW_FIELD("Odor: ", c->odor_profile[0] ? c->odor_profile : "(none)");

    /* Flavor descriptors */
    DRAW_FIELD("Flavor: ", c->flavor_descriptors[0] ? c->flavor_descriptors : "(none)");

    /* Odor detection threshold */
    {
        RECT tr;
        if (c->odor_threshold_ppm > 0.0f)
            sprintf(buf, "Threshold: %.6g ppm", (double)c->odor_threshold_ppm);
        else
            sprintf(buf, "Threshold: (not set)");
        tr.left = 8; tr.top = y; tr.right = prc->right - 4; tr.bottom = y + 1000;
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        y += (tr.bottom - tr.top) + 3;
    }

    /* Max ppm / rec range */
    {
        RECT tr;
        if (c->max_use_ppm > 0.0f)
            sprintf(buf, "Max: %.1f ppm  (rec %.2f-%.2f)",
                    (double)c->max_use_ppm,
                    (double)c->rec_min_ppm, (double)c->rec_max_ppm);
        else
            sprintf(buf, "Max: no limit  (rec %.2f-%.2f)",
                    (double)c->rec_min_ppm, (double)c->rec_max_ppm);
        tr.left = 8; tr.top = y; tr.right = prc->right - 4; tr.bottom = y + 1000;
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        y += (tr.bottom - tr.top) + 3;
    }

    /* pH range */
    {
        RECT tr;
        sprintf(buf, "pH: %.1f - %.1f",
                (double)c->ph_stable_min, (double)c->ph_stable_max);
        tr.left = 8; tr.top = y; tr.right = prc->right - 4; tr.bottom = y + 1000;
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        y += (tr.bottom - tr.top) + 3;
    }

    /* MW / water solubility */
    {
        RECT tr;
        if (c->water_solubility > 0.0f)
            sprintf(buf, "MW: %.2f g/mol  Sol: %.0f mg/L",
                    (double)c->molecular_weight, (double)c->water_solubility);
        else
            sprintf(buf, "MW: %.2f g/mol  Sol: miscible",
                    (double)c->molecular_weight);
        tr.left = 8; tr.top = y; tr.right = prc->right - 4; tr.bottom = y + 1000;
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        y += (tr.bottom - tr.top) + 3;
    }

    /* Storage */
    DRAW_FIELD("Storage: ", c->storage_temp[0] ? c->storage_temp : "RT");

    /* Solubilizer — orange text if required, plus solubilizer options note */
    {
        RECT tr;
        if (c->requires_solubilizer) {
            SetTextColor(hdc, RGB(200, 100, 0));
            sprintf(buf, "Solubilizer: Yes (Polysorbate 80 / PG / EtOH)");
        } else {
            SetTextColor(hdc, RGB(30, 30, 30));
            sprintf(buf, "Solubilizer: No");
        }
        tr.left = 8; tr.top = y; tr.right = prc->right - 4; tr.bottom = y + 1000;
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        y += (tr.bottom - tr.top) + 3;
        SetTextColor(hdc, RGB(30, 30, 30));
    }

    /* Inert atmosphere */
    DRAW_FIELD("Inert atm: ", c->requires_inert_atm ? "Yes" : "No");

    /* Cost */
    {
        RECT tr;
        if (c->cost_per_gram > 0.0f)
            sprintf(buf, "Cost: $%.4f / g", (double)c->cost_per_gram);
        else
            sprintf(buf, "Cost: (not set)");
        tr.left = 8; tr.top = y; tr.right = prc->right - 4; tr.bottom = y + 1000;
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        DrawText(hdc, buf, -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        y += (tr.bottom - tr.top) + 3;
    }

#undef DRAW_FIELD

    if (hOldFont) SelectObject(hdc, hOldFont);
}

/* =========================================================================
   DetailPaneWndProc
   ========================================================================= */
static LRESULT CALLBACK DetailPaneWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
        g_hBtnPopOut = CreateWindowEx(0, "BUTTON", "Pop Out",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            8, 8, 80, 24, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_POPOUT, g_hInst, NULL);
        if (g_hFontNormal)
            SendMessage(g_hBtnPopOut, WM_SETFONT, (WPARAM)g_hFontNormal, FALSE);
        return 0;

    case WM_SIZE:
    {
        int h = HIWORD(lParam);
        if (g_hBtnPopOut)
            MoveWindow(g_hBtnPopOut, 8, h - 32, 80, 24, TRUE);
    }
    return 0;

    case WM_COMMAND:
        /* Forward Pop Out button click up to the panel */
        if (LOWORD(wParam) == IDC_BTN_POPOUT)
            SendMessage(GetParent(hWnd), WM_COMMAND, wParam, lParam);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        RECT rc;
        HDC hdc = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

        if (!g_hasSelection) {
            SetTextColor(hdc, RGB(160, 160, 160));
            SetBkMode(hdc, TRANSPARENT);
            if (g_hFontNormal) SelectObject(hdc, g_hFontNormal);
            DrawText(hdc, "Select a compound\nto see details.",
                     -1, &rc, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
        } else {
            RECT content = rc;
            content.bottom -= 40;  /* leave room for Pop Out button */
            DrawCompoundDetail(hdc, &content, &g_selectedCompound);
        }

        EndPaint(hWnd, &ps);
    }
    return 0;

    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   PopOutWndProc
   ========================================================================= */
static LRESULT CALLBACK PopOutWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
    {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        CompoundInfo* c = (CompoundInfo*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        PAINTSTRUCT ps;
        RECT rc;
        HDC hdc = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        if (c) DrawCompoundDetail(hdc, &rc, c);
        EndPaint(hWnd, &ps);
    }
    return 0;

    case WM_DESTROY:
    {
        CompoundInfo* c = (CompoundInfo*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        int i, n;
        if (c) {
            free(c);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
        }
        /* Remove from g_hPopOuts using copy-last filter.
           Set count to 0 first to prevent re-entrant mutation. */
        n = g_nPopOuts;
        g_nPopOuts = 0;
        for (i = 0; i < n; i++) {
            if (g_hPopOuts[i] != hWnd)
                g_hPopOuts[g_nPopOuts++] = g_hPopOuts[i];
        }
    }
    return 0;

    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/* =========================================================================
   OpenPopOutWindow
   ========================================================================= */
static void OpenPopOutWindow(const CompoundInfo* c)
{
    CompoundInfo* copy;
    char title[160];

    if (g_nPopOuts >= 8) return;

    copy = (CompoundInfo*)malloc(sizeof(CompoundInfo));
    if (!copy) return;
    memcpy(copy, c, sizeof(CompoundInfo));

    sprintf(title, "Compound: %s", c->compound_name);

    g_hPopOuts[g_nPopOuts] = CreateWindowEx(
        WS_EX_TOOLWINDOW,
        "CompoundPopOut",
        title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 500,
        NULL, NULL, g_hInst, copy);

    if (g_hPopOuts[g_nPopOuts])
        g_nPopOuts++;
    else
        free(copy);
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
            char  costStr[32];
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
        /* Search bar */
        CreateWindowEx(0, "STATIC", "Search:",
            WS_CHILD | WS_VISIBLE,
            4, 10, 46, 18, hWnd, NULL, g_hInst, NULL);
        g_hSearchEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            52, 8, 160, 22, hWnd,
            (HMENU)(INT_PTR)IDC_SEARCH_EDIT, g_hInst, NULL);

        /* Solubilizer filter */
        CreateWindowEx(0, "STATIC", "Solub:",
            WS_CHILD | WS_VISIBLE,
            220, 10, 40, 18, hWnd, NULL, g_hInst, NULL);
        g_hSolubCombo = CreateWindowEx(0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            260, 7, 80, 100, hWnd,
            (HMENU)(INT_PTR)IDC_SOLUB_COMBO, g_hInst, NULL);
        SendMessage(g_hSolubCombo, CB_ADDSTRING, 0, (LPARAM)"All");
        SendMessage(g_hSolubCombo, CB_ADDSTRING, 0, (LPARAM)"Yes");
        SendMessage(g_hSolubCombo, CB_ADDSTRING, 0, (LPARAM)"No");
        SendMessage(g_hSolubCombo, CB_SETCURSEL, 0, 0);

        /* Edit Cost button */
        g_hBtnEditCost = CreateWindowEx(0, "BUTTON", "Edit Cost",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            346, 5, 90, 26, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_EDIT_COST, g_hInst, NULL);

        /* App filter */
        CreateWindowEx(0, "STATIC", "App:",
            WS_CHILD | WS_VISIBLE,
            444, 10, 30, 18, hWnd, NULL, g_hInst, NULL);
        g_hAppCombo = CreateWindowEx(0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            476, 7, 120, 200, hWnd,
            (HMENU)(INT_PTR)IDC_APP_COMBO, g_hInst, NULL);
        SendMessage(g_hAppCombo, CB_ADDSTRING, 0, (LPARAM)"All");
        SendMessage(g_hAppCombo, CB_ADDSTRING, 0, (LPARAM)"Beverages");
        SendMessage(g_hAppCombo, CB_ADDSTRING, 0, (LPARAM)"Alcoholic");
        SendMessage(g_hAppCombo, CB_ADDSTRING, 0, (LPARAM)"Baked Goods");
        SendMessage(g_hAppCombo, CB_ADDSTRING, 0, (LPARAM)"Dairy");
        SendMessage(g_hAppCombo, CB_ADDSTRING, 0, (LPARAM)"Meat");
        SendMessage(g_hAppCombo, CB_ADDSTRING, 0, (LPARAM)"Confections");
        SendMessage(g_hAppCombo, CB_ADDSTRING, 0, (LPARAM)"Savory");
        SendMessage(g_hAppCombo, CB_SETCURSEL, 0, 0);

        /* Apply fonts */
        if (g_hFontNormal) {
            SendMessage(g_hSearchEdit,  WM_SETFONT, (WPARAM)g_hFontNormal, FALSE);
            SendMessage(g_hSolubCombo,  WM_SETFONT, (WPARAM)g_hFontNormal, FALSE);
            SendMessage(g_hBtnEditCost, WM_SETFONT, (WPARAM)g_hFontNormal, FALSE);
            SendMessage(g_hAppCombo,    WM_SETFONT, (WPARAM)g_hFontNormal, FALSE);
        }

        /* ListView */
        g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE |
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, TOP_BAR_H, 100, 100, hWnd,
            (HMENU)(INT_PTR)IDC_LISTVIEW, g_hInst, NULL);
        ListView_SetExtendedListViewStyle(g_hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LV_AddCol(g_hListView, 0, "Name",       170);
        LV_AddCol(g_hListView, 1, "FEMA",         60);
        LV_AddCol(g_hListView, 2, "Max ppm",       80);
        LV_AddCol(g_hListView, 3, "Rec Min",       75);
        LV_AddCol(g_hListView, 4, "Rec Max",       75);
        LV_AddCol(g_hListView, 5, "$/g",           70);
        LV_AddCol(g_hListView, 6, "Solubilizer",   85);
        LV_AddCol(g_hListView, 7, "Storage",       70);

        /* Detail pane — full height on the right side */
        g_hDetailPane = CreateWindowEx(WS_EX_CLIENTEDGE, "CompoundDetailPane", NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, DETAIL_W, 100, hWnd, NULL, g_hInst, NULL);
    }
    return 0;

    case WM_SIZE:
    {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        int lv_w = w - DETAIL_W;
        if (lv_w < 0) lv_w = 0;
        if (g_hListView)
            MoveWindow(g_hListView,   0,     TOP_BAR_H, lv_w,    h - TOP_BAR_H, TRUE);
        if (g_hDetailPane)
            MoveWindow(g_hDetailPane, lv_w,  0,          DETAIL_W, h,            TRUE);
    }
    return 0;

    case WM_NOTIFY:
    {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->idFrom == IDC_LISTVIEW && pnm->code == LVN_ITEMCHANGED) {
            NMLISTVIEW* pnlv = (NMLISTVIEW*)lParam;
            if ((pnlv->uNewState & LVIS_SELECTED) &&
                !(pnlv->uOldState & LVIS_SELECTED))
            {
                char name[128];
                ListView_GetItemText(g_hListView, pnlv->iItem, 0, name, sizeof(name));
                ZeroMemory(&g_selectedCompound, sizeof(g_selectedCompound));
                g_hasSelection =
                    (db_get_compound_by_name(name, &g_selectedCompound) == 0);
                if (g_hDetailPane)
                    InvalidateRect(g_hDetailPane, NULL, TRUE);
            }
        }
    }
    return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SEARCH_EDIT:
            if (HIWORD(wParam) == EN_CHANGE) Panel_Compounds_Refresh();
            break;
        case IDC_SOLUB_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE) Panel_Compounds_Refresh();
            break;
        case IDC_APP_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE) Panel_Compounds_Refresh();
            break;
        case IDC_BTN_POPOUT:
            if (g_hasSelection) OpenPopOutWindow(&g_selectedCompound);
            break;
        case IDC_BTN_EDIT_COST:
        {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (sel < 0) {
                MessageBox(hWnd, "Select a compound to edit its cost.", "Edit Cost", MB_OK);
                break;
            }
            ListView_GetItemText(g_hListView, sel, 0,
                                 g_dlgCompoundName, sizeof(g_dlgCompoundName));
            OpenCostDialog(hWnd);
            if (g_dlgSaved) {
                Panel_Compounds_Refresh();
                /* Refresh detail pane if the edited compound is selected */
                if (g_hasSelection &&
                    strcmp(g_selectedCompound.compound_name, g_dlgCompoundName) == 0)
                {
                    db_get_compound_by_name(g_dlgCompoundName, &g_selectedCompound);
                    if (g_hDetailPane) InvalidateRect(g_hDetailPane, NULL, TRUE);
                }
            }
        }
        break;
        }
        return 0;

    case WM_DESTROY:
    {
        int i, n;
        /* Close all pop-outs before freeing fonts */
        n = g_nPopOuts; g_nPopOuts = 0;
        for (i = 0; i < n; i++)
            if (IsWindow(g_hPopOuts[i])) DestroyWindow(g_hPopOuts[i]);
        if (g_hFontBold)   { DeleteObject(g_hFontBold);   g_hFontBold   = NULL; }
        if (g_hFontNormal) { DeleteObject(g_hFontNormal); g_hFontNormal = NULL; }
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

    /* Create fonts before the panel window so WM_CREATE can use them */
    g_hFontNormal = CreateFont(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                               DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    g_hFontBold   = CreateFont(-13, 0, 0, 0, FW_BOLD,   0, 0, 0,
                               DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");

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

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DetailPaneWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CompoundDetailPane";
    RegisterClassEx(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = PopOutWndProc;
    wc.hInstance     = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CompoundPopOut";
    RegisterClassEx(&wc);

    g_hPanel = CreateWindowEx(0,
        "CompoundsPanel", NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 100, 100,
        hParent, NULL, g_hInst, NULL);

    return g_hPanel;
}

/* =========================================================================
   Panel_Compounds_Refresh — single parameterized SQL with search, app, and
   solubilizer filters.
   ========================================================================= */
void Panel_Compounds_Refresh(void)
{
    /* App token map: combobox index → pipe-search token */
    static const char* app_tokens[] = {
        "",            /* 0 = All */
        "beverages",   /* 1 */
        "alcoholic",   /* 2 */
        "baked goods", /* 3 */
        "dairy",       /* 4 */
        "meat",        /* 5 */
        "confections", /* 6 */
        "savory"       /* 7 */
    };

    sqlite3*      db        = db_get_handle();
    sqlite3_stmt* stmt      = NULL;
    int           row       = 0;
    char          buf[64];
    char          search[256];
    char          like_search[260]; /* for name/odor/flavor */
    char          like_app[140];    /* for applications column */
    int           solub_sel;
    int           app_sel;
    int           solub_sentinel;   /* -1=all, 0/1=filter */

    if (!g_hListView || !db) return;

    /* Read search text */
    search[0] = '\0';
    if (g_hSearchEdit) GetWindowText(g_hSearchEdit, search, sizeof(search));
    if (search[0])
        sprintf(like_search, "%%%s%%", search);
    else
        strcpy(like_search, "%");

    /* Read solubilizer selection: 0=All, 1=Yes, 2=No */
    solub_sel = 0;
    if (g_hSolubCombo) solub_sel = (int)SendMessage(g_hSolubCombo, CB_GETCURSEL, 0, 0);
    if (solub_sel < 0) solub_sel = 0;
    solub_sentinel = (solub_sel == 0) ? -1 : (solub_sel == 1 ? 1 : 0);

    /* Read app filter */
    app_sel = 0;
    if (g_hAppCombo) app_sel = (int)SendMessage(g_hAppCombo, CB_GETCURSEL, 0, 0);
    if (app_sel < 0) app_sel = 0;
    if (app_sel == 0 || app_sel >= (int)(sizeof(app_tokens)/sizeof(app_tokens[0])))
        strcpy(like_app, "%");
    else
        sprintf(like_app, "%%%s%%", app_tokens[app_sel]);

    ListView_DeleteAllItems(g_hListView);

    /* Single query handles all filter combinations via sentinels */
    sqlite3_prepare_v2(db,
        "SELECT compound_name, fema_number, max_use_ppm, "
        "       rec_min_ppm, rec_max_ppm, cost_per_gram, "
        "       requires_solubilizer, storage_temp "
        "FROM compound_library "
        "WHERE (compound_name      LIKE ?1"
        "    OR odor_profile       LIKE ?1"
        "    OR flavor_descriptors LIKE ?1)"
        "  AND (applications       LIKE ?2)"
        "  AND (?3 = -1 OR requires_solubilizer = ?3)"
        "ORDER BY compound_name;",
        -1, &stmt, NULL);

    if (!stmt) return;

    sqlite3_bind_text(stmt, 1, like_search,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, like_app,       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, solub_sentinel);

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

        sprintf(buf, "%d",   fema);   LV_SetCell(g_hListView, row, 1, buf);
        sprintf(buf, "%.1f", maxppm); LV_SetCell(g_hListView, row, 2, buf);
        sprintf(buf, "%.2f", recmin); LV_SetCell(g_hListView, row, 3, buf);
        sprintf(buf, "%.2f", recmax); LV_SetCell(g_hListView, row, 4, buf);
        sprintf(buf, "%.4f", cpg);    LV_SetCell(g_hListView, row, 5, buf);
        LV_SetCell(g_hListView, row, 6, solub ? "Yes" : "No");
        LV_SetCell(g_hListView, row, 7, stor ? stor : "");

        row++;
    }

    sqlite3_finalize(stmt);
}
