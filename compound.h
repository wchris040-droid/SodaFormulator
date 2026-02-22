#ifndef COMPOUND_H
#define COMPOUND_H

typedef struct {
    int   id;                    /* compound_library.id; 0 if not persisted */
    char  compound_name[128];
    char  cas_number[16];
    int   fema_number;
    float max_use_ppm;           /* 0.0 = no established limit */
    float rec_min_ppm;
    float rec_max_ppm;
    float molecular_weight;
    float water_solubility;      /* mg/L at ~20C; 0 = fully miscible/unknown */
    float ph_stable_min;
    float ph_stable_max;
    char  odor_profile[256];
    char  storage_temp[16];      /* "RT", "2-8C", "-20C" */
    int   requires_solubilizer;  /* 0/1 */
    int   requires_inert_atm;    /* 0/1 */
    float cost_per_gram;         /* USD/g; 0.0 = not set */
    char  flavor_descriptors[256]; /* retronasal/taste keywords */
    float odor_threshold_ppm;      /* detection threshold in water (ppm) */
    char  applications[128];       /* pipe-separated: "beverages|baked goods" */
} CompoundInfo;

/* Check ppm against max_use_ppm.
   Returns 0 = within limits, 1 = exceeds max, 2 = no limit data (max_use_ppm == 0) */
int  compound_check_limit(const CompoundInfo* c, float ppm);

/* Print a formatted compound info card to stdout */
void compound_print(const CompoundInfo* c);

#endif
