#ifndef UI_H
#define UI_H

#include <windows.h>

extern HINSTANCE g_hInst;

/* Nav panel indices */
#define NAV_FORMULATIONS  0
#define NAV_COMPOUNDS     1
#define NAV_TASTING       2
#define NAV_BATCH         3
#define NAV_INVENTORY     4

/* Main layout constants */
#define NAV_WIDTH         160

/* Top-level control IDs */
#define IDC_NAV_LIST      100
#define IDC_STATUS_BAR    101

/* Button IDs (shared across panels) */
#define IDC_BTN_NEW           1001
#define IDC_BTN_EDIT          1002
#define IDC_BTN_DELETE        1003
#define IDC_BTN_VALIDATE      1004
#define IDC_BTN_HISTORY       1005
#define IDC_BTN_SAVE          1006
#define IDC_BTN_CANCEL        1007
#define IDC_BTN_ADD_CPD       1008
#define IDC_BTN_REMOVE_CPD    1009
#define IDC_BTN_EDIT_COST     1010
#define IDC_BTN_NEW_SESSION   1011
#define IDC_BTN_NEW_BATCH     1012
#define IDC_BTN_UPDATE_STOCK  1013
#define IDC_BTN_UPDATE_COST   1014
#define IDC_BTN_CHECK         1015

/* Panel child control IDs */
#define IDC_LISTVIEW          2000
#define IDC_FILTER_COMBO      2001

/* Dialog control IDs */
#define IDC_DLG_CODE          3001
#define IDC_DLG_NAME          3002
#define IDC_DLG_PH            3003
#define IDC_DLG_BRIX          3004
#define IDC_DLG_CPD_LIST      3005
#define IDC_DLG_CPD_COMBO     3006
#define IDC_DLG_CPD_PPM       3007
#define IDC_DLG_TASTER        3008
#define IDC_DLG_OVERALL       3009
#define IDC_DLG_AROMA         3010
#define IDC_DLG_FLAVOR        3011
#define IDC_DLG_MOUTHFEEL     3012
#define IDC_DLG_FINISH        3013
#define IDC_DLG_SWEETNESS     3014
#define IDC_DLG_NOTES         3015
#define IDC_DLG_VOL           3016
#define IDC_DLG_BATCHNO       3017
#define IDC_DLG_COST_FIELD    3018
#define IDC_DLG_STOCK_FIELD   3019
#define IDC_DLG_REORDER       3020
#define IDC_DLG_VER_COMBO     3021
#define IDC_DLG_FLV_COMBO     3022
#define IDC_DLG_STATUS_LBL    3023
#define IDC_DLG_COST_LBL      3024

/* Panel create / refresh exports */
HWND Panel_Formulations_Create(HWND hParent);
void Panel_Formulations_Refresh(void);

HWND Panel_Compounds_Create(HWND hParent);
void Panel_Compounds_Refresh(void);

HWND Panel_Tasting_Create(HWND hParent);
void Panel_Tasting_Refresh(void);

HWND Panel_Batch_Create(HWND hParent);
void Panel_Batch_Refresh(void);

HWND Panel_Inventory_Create(HWND hParent);
void Panel_Inventory_Refresh(void);

#endif /* UI_H */
