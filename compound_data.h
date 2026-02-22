#ifndef COMPOUND_DATA_H
#define COMPOUND_DATA_H
/*
 * compound_data.h — Static compound library seeding data.
 * Include ONLY from database.c.
 *
 * Field order:
 *   compound_name, cas_number, fema_number,
 *   max_use_ppm, rec_min_ppm, rec_max_ppm,
 *   molecular_weight, water_solubility (mg/L; 0=miscible),
 *   ph_stable_min, ph_stable_max,
 *   odor_profile, storage_temp,
 *   requires_solubilizer (0/1), requires_inert_atm (0/1),
 *   cost_per_gram (USD/g),
 *   flavor_descriptors, odor_threshold_ppm,
 *   applications (pipe-separated tokens)
 *
 * Application tokens:
 *   beverages | alcoholic | baked goods | dairy | meat | confections | savory
 *
 * Sources: FEMA GRAS, Fenaroli's Handbook, peer-reviewed literature.
 * Entries with sparse public data are excluded — no guessed values.
 */

static const struct {
    const char* compound_name;
    const char* cas_number;
    int         fema_number;
    float       max_use_ppm;
    float       rec_min_ppm;
    float       rec_max_ppm;
    float       molecular_weight;
    float       water_solubility;
    float       ph_stable_min;
    float       ph_stable_max;
    const char* odor_profile;
    const char* storage_temp;
    int         requires_solubilizer;
    int         requires_inert_atm;
    float       cost_per_gram;
    const char* flavor_descriptors;
    float       odor_threshold_ppm;
    const char* applications;
} compounds[] = {

/* =========================================================================
   EXISTING 15 — retained with applications added
   ========================================================================= */
{ "Benzaldehyde",        "100-52-7",    2127, 100.0f,  20.0f,  80.0f, 106.12f, 3000.0f, 2.5f, 5.0f, "almond cherry maraschino",   "RT",   0, 0, 0.05f,  "bitter almond cherry sweet",       0.35f,    "beverages|confections|baked goods" },
{ "Benzyl acetate",      "140-11-4",    2135,  50.0f,   2.0f,  15.0f, 150.17f, 5900.0f, 3.0f, 5.0f, "jasmine fruity sweet",       "RT",   0, 0, 0.08f,  "fruity floral jasmine sweet",      0.15f,    "beverages|confections|baked goods" },
{ "Butyric acid",        "107-92-6",    2221,   1.0f,   0.1f,   0.5f,  88.11f,    0.0f, 2.5f, 5.0f, "butter cheese rancid",       "RT",   0, 0, 0.10f,  "butter cheese rancid sour",        0.24f,    "dairy|baked goods" },
{ "Cinnamaldehyde",      "104-55-2",    2286,  50.0f,  10.0f,  40.0f, 132.16f, 1400.0f, 2.5f, 4.5f, "cinnamon spicy warm",        "RT",   0, 0, 0.15f,  "cinnamon spicy sweet warm",        0.015f,   "beverages|baked goods|confections" },
{ "Cyclotene",           "80-71-7",     2700,  10.0f,   1.0f,   5.0f, 112.13f, 5000.0f, 2.5f, 5.0f, "maple caramel roasted",      "RT",   0, 0, 2.00f,  "maple caramel smoky sweet",        0.1f,     "baked goods|confections" },
{ "Delta-decalactone",   "705-86-2",    2361,  20.0f,   1.0f,  10.0f, 170.25f,  200.0f, 3.0f, 5.5f, "peach cream coconut",        "RT",   1, 0, 1.50f,  "peach coconut creamy sweet",       0.011f,   "dairy|confections|beverages" },
{ "Diacetyl",            "431-03-8",    2370,   5.0f,   0.5f,   3.0f,  86.09f,    0.0f, 2.0f, 5.0f, "butter cream",               "2-8C", 0, 0, 0.20f,  "butter cream dairy",               0.005f,   "dairy|baked goods|beverages" },
{ "Ethyl cinnamate",     "103-36-6",    2430,  10.0f,   1.0f,   5.0f, 176.21f,  500.0f, 3.0f, 5.0f, "fruity cinnamon sweet",      "RT",   1, 0, 1.00f,  "cinnamon fruity sweet balsamic",   1.1f,     "beverages|baked goods|confections" },
{ "Ethyl maltol",        "4940-11-8",   3487, 150.0f,  10.0f,  80.0f, 140.14f,55000.0f, 2.5f, 5.0f, "cotton candy sweet",         "RT",   0, 0, 0.10f,  "sweet cotton candy fruity",        0.086f,   "beverages|confections|baked goods" },
{ "Eugenol",             "97-53-0",     2467,  20.0f,   2.0f,  10.0f, 164.20f, 2400.0f, 3.0f, 7.0f, "clove spicy warm",           "RT",   1, 0, 0.08f,  "clove spicy warm medicinal",       0.006f,   "beverages|baked goods|meat|savory" },
{ "Furaneol",            "3658-77-3",   3174,   5.0f,   0.1f,   1.0f, 128.13f,50000.0f, 2.5f, 4.5f, "caramel strawberry sweet",   "2-8C", 0, 0, 5.00f,  "caramel strawberry sweet fruity",  0.000025f,"beverages|confections|baked goods" },
{ "Gamma-undecalactone", "104-67-6",    3091,  20.0f,   1.0f,  10.0f, 184.28f,  100.0f, 3.0f, 5.5f, "peach creamy fruity",        "RT",   1, 0, 2.00f,  "peach creamy coconut fruity",      0.005f,   "beverages|confections|dairy" },
{ "Maltol",              "118-71-8",    2656, 200.0f,  20.0f, 100.0f, 126.11f, 8000.0f, 2.5f, 5.0f, "sweet caramel cotton candy", "RT",   0, 0, 0.15f,  "sweet caramel jam fruity",         1.7f,     "beverages|confections|baked goods" },
{ "Sotolon",             "28664-35-9",  3634,   0.5f,  0.01f,   0.3f, 128.15f,50000.0f, 2.5f, 5.0f, "maple caramel fenugreek",    "2-8C", 0, 0, 10.00f, "maple caramel fenugreek sweet",    0.0015f,  "beverages|baked goods|confections" },
{ "Vanillin",            "121-33-5",    3107, 150.0f,  20.0f, 100.0f, 152.15f,10000.0f, 2.0f, 5.0f, "vanilla sweet creamy",       "RT",   0, 0, 0.05f,  "vanilla sweet creamy",             0.02f,    "beverages|baked goods|dairy|confections" },

/* =========================================================================
   ALCOHOLS — aromatic
   ========================================================================= */
{ "Benzyl alcohol",      "100-51-6",    2137, 100.0f,   1.0f,  20.0f, 108.14f,40000.0f, 2.5f, 7.0f, "floral rose sweet mild",     "RT",   0, 0, 0.04f,  "sweet floral rose mild",           1.0f,     "beverages|confections|baked goods" },
{ "2-Phenylethanol",     "60-12-8",     2858, 100.0f,   5.0f,  30.0f, 122.17f,20000.0f, 2.5f, 7.0f, "rosy floral honey",          "RT",   0, 0, 0.05f,  "rosy floral honey sweet",          0.75f,    "beverages|confections|baked goods|alcoholic" },
{ "Linalool",            "78-70-6",     2635, 100.0f,   1.0f,  30.0f, 154.25f, 1600.0f, 2.5f, 5.5f, "floral lavender citrus",     "RT",   1, 0, 0.08f,  "floral lavender bergamot sweet",   0.006f,   "beverages|confections|baked goods" },
{ "Geraniol",            "106-24-1",    2507, 100.0f,   1.0f,  20.0f, 154.25f,  500.0f, 2.5f, 5.0f, "floral rose citrus",         "RT",   1, 0, 0.10f,  "rosy floral citrus sweet",         0.004f,   "beverages|confections|baked goods" },
{ "Citronellol",         "106-22-9",    2309, 100.0f,   1.0f,  15.0f, 156.27f,  100.0f, 2.5f, 5.0f, "floral rosy citrus",         "RT",   1, 0, 0.12f,  "rosy floral sweet citrus",         0.04f,    "beverages|confections" },
{ "Nerol",               "106-25-2",    2770,  50.0f,   1.0f,  10.0f, 154.25f,  900.0f, 2.5f, 5.0f, "sweet rose citrus neroli",   "RT",   1, 0, 0.15f,  "sweet rosy citrus floral",         0.3f,     "beverages|confections" },
{ "Menthol",             "89-78-1",     2665, 500.0f,  10.0f, 100.0f, 156.27f,  500.0f, 2.5f, 7.0f, "peppermint cool minty",      "RT",   1, 0, 0.02f,  "cool mint peppermint refreshing",  0.004f,   "beverages|confections|savory" },
{ "Isoamyl alcohol",     "123-51-3",    2057, 200.0f,   5.0f,  50.0f,  88.15f,    0.0f, 2.5f, 7.0f, "fusel winey whiskey",        "RT",   0, 0, 0.03f,  "fusel winey alcoholic fruity",     0.25f,    "alcoholic|beverages" },
{ "Hexanol",             "111-27-3",    2567,  50.0f,   1.0f,  10.0f, 102.18f, 5900.0f, 2.5f, 5.5f, "green herbaceous fresh",     "RT",   0, 0, 0.06f,  "green herbaceous fresh",           0.5f,     "beverages|alcoholic" },
{ "Furfuryl alcohol",    "98-00-0",     2491,  50.0f,   1.0f,  20.0f,  98.10f,    0.0f, 3.0f, 5.0f, "coffee caramel roasted",     "RT",   0, 0, 0.05f,  "coffee caramel roasted sweet",     0.6f,     "baked goods|beverages|confections" },
{ "2-Methylbutanol",     "137-32-6",    2688, 100.0f,   5.0f,  30.0f,  88.15f, 3000.0f, 2.5f, 7.0f, "fusel winey cognac",         "RT",   0, 0, 0.10f,  "winey cognac fusel sweet",         0.05f,    "alcoholic|beverages" },
{ "alpha-Terpineol",     "98-55-5",     3045, 100.0f,   1.0f,  20.0f, 154.25f, 1900.0f, 2.5f, 5.5f, "lilac pine floral",          "RT",   1, 0, 0.08f,  "floral lilac pine sweet",          0.33f,    "beverages|confections" },

/* =========================================================================
   ALCOHOLS — aliphatic / fatty
   ========================================================================= */
{ "cis-3-Hexenol",       "928-96-1",    2563,  50.0f,   0.5f,  10.0f, 100.16f,    0.0f, 2.5f, 5.0f, "green fresh cut grass",      "RT",   0, 0, 0.40f,  "fresh cut grass green herbal",     0.00007f, "beverages|baked goods" },
{ "trans-2-Hexenol",     "928-95-0",    2562,  50.0f,   0.5f,  10.0f, 100.16f,    0.0f, 2.5f, 5.0f, "green herbaceous fruity",    "RT",   0, 0, 0.50f,  "green herbaceous fruity sweet",    0.0035f,  "beverages" },
{ "1-Octanol",           "111-87-5",    2800,  50.0f,   0.5f,  10.0f, 130.23f,  540.0f, 2.5f, 5.5f, "waxy citrus mushroom",       "RT",   1, 0, 0.05f,  "waxy citrus fatty fresh",          0.11f,    "beverages|confections" },
{ "2-Heptanol",          "543-49-7",    2539,  50.0f,   0.5f,  10.0f, 116.20f, 3300.0f, 2.5f, 5.5f, "oily citrus mushroom",       "RT",   0, 0, 0.12f,  "oily citrus musty mild",           0.25f,    "beverages" },
{ "Isobutyl alcohol",    "78-83-1",     2179, 200.0f,   5.0f,  80.0f,  74.12f,    0.0f, 2.5f, 7.0f, "fusel winey sharp",          "RT",   0, 0, 0.02f,  "winey fusel sharp alcoholic",      0.3f,     "alcoholic|beverages" },
{ "1-Nonanol",           "143-08-8",    2784,  30.0f,   0.2f,   5.0f, 144.25f,  140.0f, 2.5f, 5.5f, "waxy floral citrus",         "RT",   1, 0, 0.08f,  "waxy floral citrus fatty",         0.05f,    "beverages|confections" },
{ "1-Decanol",           "112-30-1",    2365,  30.0f,   0.2f,   5.0f, 158.28f,   40.0f, 2.5f, 5.5f, "waxy fatty soapy",           "RT",   1, 0, 0.08f,  "waxy fatty soapy citrus",          0.04f,    "beverages" },
{ "2-Ethyl-1-hexanol",   "104-76-7",    2436,  50.0f,   1.0f,  10.0f, 130.23f,  900.0f, 2.5f, 5.5f, "fresh citrus mild",          "RT",   1, 0, 0.05f,  "fresh citrus mild waxy",           0.27f,    "beverages" },
{ "1-Octen-3-ol",        "3391-86-4",   2805,  10.0f,  0.05f,   2.0f, 128.21f, 5000.0f, 2.5f, 5.5f, "mushroom herbal earthy",     "RT",   0, 0, 0.60f,  "mushroom earthy herbal green",     0.001f,   "savory|beverages" },

/* =========================================================================
   ALDEHYDES — aromatic
   ========================================================================= */
{ "Phenylacetaldehyde",  "122-78-1",    2866,  50.0f,   0.5f,  10.0f, 120.15f,10000.0f, 2.5f, 5.0f, "honey rose floral green",    "RT",   0, 0, 0.40f,  "honey rosy floral sweet green",    0.001f,   "beverages|confections|baked goods" },
{ "p-Anisaldehyde",      "123-11-5",    2670,  50.0f,   2.0f,  20.0f, 136.15f, 1400.0f, 2.5f, 5.0f, "anise sweet hawthorn",       "RT",   0, 0, 0.10f,  "anise sweet floral hawthorn",      0.045f,   "beverages|confections|baked goods" },
{ "4-Methylbenzaldehyde","104-87-0",    2660,  50.0f,   1.0f,  15.0f, 120.15f, 1000.0f, 2.5f, 5.0f, "almond hawthorn sweet",      "RT",   0, 0, 0.15f,  "almond hawthorn cherry sweet",     0.04f,    "savory|beverages|confections" },
{ "Piperonal",           "120-57-0",    2911, 100.0f,   5.0f,  50.0f, 150.13f, 1400.0f, 2.5f, 5.0f, "floral heliotrope cherry",   "RT",   0, 0, 0.15f,  "floral heliotrope cherry sweet",   0.01f,    "beverages|confections|baked goods" },
{ "Citronellal",         "106-23-0",    2307, 100.0f,   2.0f,  20.0f, 154.25f,  100.0f, 2.5f, 5.0f, "citrus lemon rose",          "RT",   1, 0, 0.08f,  "citrus lemon rosy sweet",          0.04f,    "beverages|confections" },
{ "2-Phenylpropanal",    "93-53-8",     2874,  30.0f,   0.5f,   8.0f, 134.18f,  500.0f, 3.0f, 5.0f, "hyacinth green floral",      "RT",   0, 0, 0.60f,  "hyacinth green floral sweet",      0.05f,    "confections|beverages" },
{ "Cinnamyl alcohol",    "104-54-1",    2294,  50.0f,   1.0f,  20.0f, 134.18f, 2000.0f, 3.0f, 5.5f, "hyacinth balsamic floral",   "RT",   0, 0, 0.15f,  "hyacinth balsamic floral sweet",   1.2f,     "confections|baked goods|beverages" },
{ "Amyl cinnamic aldehyde","122-40-7",  2061,  50.0f,   1.0f,  15.0f, 202.29f,   10.0f, 3.0f, 5.0f, "floral jasmine sweet",       "RT",   1, 0, 0.15f,  "jasmine floral sweet honey",       0.0068f,  "confections|beverages" },
{ "Benzaldehyde dimethyl acetal","1125-88-8",2128,100.0f,5.0f,50.0f, 152.19f, 1000.0f, 2.5f, 4.5f, "almond cherry mild",         "RT",   0, 0, 0.10f,  "almond cherry mild sweet",         0.6f,     "beverages|confections|baked goods" },

/* =========================================================================
   ALDEHYDES — aliphatic
   ========================================================================= */
{ "Acetaldehyde",        "75-07-0",     2003, 100.0f,   2.0f,  20.0f,  44.05f,    0.0f, 2.5f, 4.5f, "ethereal pungent fruity",    "-20C", 0, 1, 0.20f,  "ethereal fruity pungent fresh",    0.015f,   "beverages|alcoholic|dairy" },
{ "Hexanal",             "66-25-1",     2557,  50.0f,   0.5f,  10.0f, 100.16f, 1100.0f, 2.5f, 5.0f, "green grassy fresh",         "RT",   0, 0, 0.08f,  "green grassy fresh herbaceous",    0.0045f,  "beverages|baked goods" },
{ "trans-2-Hexenal",     "6728-26-3",   2560,  20.0f,   0.5f,   5.0f,  98.14f, 1200.0f, 2.5f, 5.0f, "green leafy herbaceous",     "2-8C", 0, 0, 0.25f,  "green leafy herbaceous fruity",    0.0017f,  "beverages" },
{ "Octanal",             "124-13-0",    2797,  50.0f,   0.5f,   5.0f, 128.21f,  570.0f, 2.5f, 5.0f, "citrus fatty orange waxy",   "RT",   1, 0, 0.20f,  "citrus fatty orange fresh",        0.0001f,  "beverages|confections" },
{ "Nonanal",             "124-19-6",    2782,  50.0f,   0.2f,   3.0f, 142.24f,  100.0f, 2.5f, 5.0f, "waxy floral citrus fatty",   "RT",   1, 0, 0.15f,  "waxy floral citrus rose",          0.001f,   "beverages|confections" },
{ "Decanal",             "112-31-2",    2362,  50.0f,   0.2f,   3.0f, 156.27f,   15.0f, 2.5f, 5.0f, "waxy citrus fatty orange",   "RT",   1, 0, 0.20f,  "waxy citrus orange sweet",         0.0009f,  "beverages|confections" },
{ "Undecanal",           "112-44-7",    3092,  30.0f,   0.2f,   3.0f, 170.29f,   30.0f, 2.5f, 5.0f, "citrus waxy fatty",          "RT",   1, 0, 0.20f,  "citrus waxy fatty floral",         0.001f,   "beverages|confections" },
{ "Citral",              "5392-40-5",   2303, 100.0f,   5.0f,  50.0f, 152.23f, 3000.0f, 2.5f, 4.5f, "lemon citrus fresh",         "RT",   1, 0, 0.08f,  "lemon citrus fresh bright",        0.03f,    "beverages|confections|baked goods" },
{ "2-Methylpropanal",    "78-84-2",     2690,  30.0f,   1.0f,  10.0f,  72.11f,    0.0f, 2.5f, 5.0f, "malty cocoa chocolate",      "RT",   0, 1, 0.15f,  "malty cocoa chocolate nutty",      0.001f,   "baked goods|beverages" },
{ "3-Methylbutanal",     "590-86-3",    2692,  30.0f,   0.5f,   8.0f,  86.13f, 3000.0f, 2.5f, 5.0f, "malty fermented cocoa",      "RT",   0, 1, 0.12f,  "malty fermented cocoa chocolate",  0.00012f, "baked goods|alcoholic" },
{ "2-Methylbutanal",     "96-17-3",     2691,  30.0f,   0.5f,   8.0f,  86.13f, 3000.0f, 2.5f, 5.0f, "fruity cocoa malty",         "RT",   0, 1, 0.20f,  "fruity cocoa malty green",         0.00016f, "baked goods|beverages" },
{ "Butanal",             "123-72-8",    2219,  30.0f,   0.5f,   8.0f,  72.11f,    0.0f, 2.5f, 5.0f, "cheesy rancid fresh",        "RT",   0, 1, 0.05f,  "cheesy rancid fresh pungent",      0.00086f, "baked goods|dairy" },
{ "trans-2-Octenal",     "2363-89-5",   2802,  20.0f,   0.2f,   5.0f, 126.20f,   80.0f, 2.5f, 5.0f, "citrus fatty green fresh",   "RT",   1, 0, 0.80f,  "citrus fatty green waxy",          0.0003f,  "beverages" },
{ "trans-2-Nonenal",     "18829-56-6",  2790,  10.0f,  0.05f,   2.0f, 140.22f,   30.0f, 2.5f, 5.0f, "cucumber fatty waxy",        "RT",   1, 0, 1.50f,  "cucumber fatty waxy green",        0.0001f,  "beverages" },
{ "trans-2-Decenal",     "3913-81-3",   2366,  20.0f,   0.1f,   3.0f, 154.25f,   20.0f, 2.5f, 5.0f, "citrus fatty waxy orange",   "RT",   1, 0, 1.00f,  "citrus fatty waxy orange",         0.00028f, "beverages|savory" },
{ "2,4-Decadienal",      "25152-84-5",  2400,  10.0f,  0.05f,   1.0f, 152.23f,   20.0f, 2.5f, 5.0f, "fried fatty oily",           "RT",   1, 0, 3.00f,  "fried fatty oily deep-fried",      0.000016f,"savory|meat" },
{ "Methylglyoxal",       "78-98-8",     2750,  10.0f,   0.5f,   5.0f,  72.06f,    0.0f, 3.0f, 5.5f, "caramel burnt sweet",        "RT",   0, 0, 0.20f,  "caramel burnt sweet pungent",      4.0f,     "baked goods|confections" },
{ "2,4-Nonadienal",      "5910-87-2",   2791,  10.0f,   0.1f,   2.0f, 138.21f,   50.0f, 2.5f, 5.0f, "fatty fried green",          "RT",   1, 0, 2.00f,  "fatty fried green oily",           0.0001f,  "savory|meat" },

/* =========================================================================
   ESTERS — simple fruit
   ========================================================================= */
{ "Isoamyl acetate",     "123-92-2",    2055, 200.0f,   5.0f, 100.0f, 130.19f, 2000.0f, 2.5f, 4.5f, "banana fruity sweet",        "RT",   0, 0, 0.03f,  "banana fruity sweet candy",        0.002f,   "beverages|confections" },
{ "Ethyl acetate",       "141-78-6",    2414, 500.0f,  20.0f, 200.0f,  88.11f,    0.0f, 2.5f, 5.0f, "fruity solvent pineapple",   "RT",   0, 0, 0.01f,  "fruity solvent pineapple sweet",   5.0f,     "beverages|alcoholic|baked goods" },
{ "Ethyl butyrate",      "105-54-4",    2427, 100.0f,   5.0f,  50.0f, 116.16f, 5000.0f, 2.5f, 5.0f, "pineapple fruity tropical",  "RT",   0, 0, 0.04f,  "pineapple fruity tropical sweet",  0.001f,   "beverages|confections" },
{ "Ethyl formate",       "109-94-4",    2434, 200.0f,   5.0f,  80.0f,  74.08f,    0.0f, 2.5f, 4.5f, "fruity rum ethereal",        "RT",   0, 0, 0.04f,  "fruity rum ethereal sweet",        10.0f,    "beverages|alcoholic" },
{ "Propyl acetate",      "109-60-4",    2925, 200.0f,  10.0f,  80.0f, 102.13f,18900.0f, 2.5f, 4.5f, "fruity pear sweet",          "RT",   0, 0, 0.03f,  "fruity pear sweet mild",           3.0f,     "beverages" },
{ "Methyl acetate",      "79-20-9",     2676, 200.0f,  10.0f,  80.0f,  74.08f,    0.0f, 2.5f, 4.0f, "fruity acetone mild",        "RT",   0, 0, 0.02f,  "fruity sweet mild acetone",        5.0f,     "beverages" },
{ "Amyl acetate",        "628-63-7",    2081, 200.0f,  10.0f,  80.0f, 130.19f, 5000.0f, 2.5f, 4.5f, "banana pear fruity",         "RT",   0, 0, 0.03f,  "banana pear fruity sweet",         0.002f,   "beverages|confections" },
{ "Isobutyl acetate",    "110-19-0",    2175, 200.0f,  10.0f,  80.0f, 116.16f, 6300.0f, 2.5f, 4.5f, "fruity banana pear",         "RT",   0, 0, 0.03f,  "fruity banana pear sweet",         0.062f,   "beverages|confections" },
{ "Butyl acetate",       "123-86-4",    2174, 200.0f,  10.0f,  80.0f, 116.16f, 6800.0f, 2.5f, 4.5f, "fruity banana pear sweet",   "RT",   0, 0, 0.02f,  "fruity banana pear sweet",         0.066f,   "beverages|confections" },
{ "Hexyl acetate",       "142-92-7",    2565, 100.0f,   5.0f,  40.0f, 144.21f,  650.0f, 2.5f, 5.0f, "fruity herbal pear green",   "RT",   1, 0, 0.05f,  "fruity herbal pear green",         0.002f,   "beverages|confections" },
{ "Isoamyl formate",     "110-45-2",    2068, 100.0f,   5.0f,  40.0f, 116.16f, 2000.0f, 2.5f, 4.5f, "fruity apple banana",        "RT",   0, 0, 0.06f,  "fruity apple banana sweet",        0.01f,    "beverages|confections" },
{ "Ethyl propanoate",    "105-37-3",    2456, 100.0f,   5.0f,  50.0f, 102.13f,19000.0f, 2.5f, 4.5f, "fruity rum tropical",        "RT",   0, 0, 0.05f,  "fruity rum tropical sweet",        0.1f,     "beverages|alcoholic" },
{ "Methyl butyrate",     "623-42-7",    2693, 100.0f,   5.0f,  50.0f, 102.13f, 6000.0f, 2.5f, 4.5f, "fruity apple pineapple",     "RT",   0, 0, 0.05f,  "fruity apple pineapple sweet",     0.005f,   "beverages|confections" },
{ "Isobutyl butyrate",   "539-90-2",    2187, 100.0f,   5.0f,  40.0f, 144.21f,  800.0f, 2.5f, 5.0f, "fruity tropical sweet",      "RT",   1, 0, 0.08f,  "fruity tropical pineapple sweet",  0.015f,   "beverages|confections" },
{ "Butyl butyrate",      "109-21-7",    2188, 100.0f,   5.0f,  40.0f, 144.21f,  800.0f, 2.5f, 5.0f, "pineapple fruity tropical",  "RT",   1, 0, 0.06f,  "pineapple fruity tropical sweet",  0.001f,   "beverages|confections" },
{ "Amyl butyrate",       "540-18-1",    2094, 100.0f,   5.0f,  40.0f, 158.24f,  400.0f, 2.5f, 5.0f, "banana pear apricot",        "RT",   1, 0, 0.08f,  "banana pear apricot fruity",       0.0007f,  "beverages|confections" },
{ "Propyl butyrate",     "105-66-8",    2934, 100.0f,   5.0f,  40.0f, 130.19f, 1600.0f, 2.5f, 5.0f, "fruity tropical sweet",      "RT",   0, 0, 0.06f,  "fruity tropical pineapple sweet",  0.0009f,  "beverages|confections" },
{ "3-Methylbutyl hexanoate","2198-61-0",2070,  50.0f,   2.0f,  20.0f, 186.29f,  200.0f, 2.5f, 5.0f, "fruity apple pear sweet",    "RT",   1, 0, 0.12f,  "fruity apple pear sweet mild",     0.002f,   "beverages|confections" },
{ "Propyl hexanoate",    "626-77-7",    2938,  50.0f,   2.0f,  20.0f, 158.24f,  400.0f, 2.5f, 5.0f, "fruity pear coconut",        "RT",   1, 0, 0.10f,  "fruity pear coconut sweet",        0.0006f,  "beverages|confections" },
{ "Hexyl hexanoate",     "1299-06-1",   2572,  30.0f,   0.5f,   8.0f, 200.32f,   10.0f, 2.5f, 5.0f, "waxy fruity oily",           "RT",   1, 0, 0.20f,  "waxy fruity oily mild",            0.002f,   "beverages" },
{ "Methyl isobutyrate",  "547-63-7",    2694, 100.0f,   5.0f,  50.0f, 102.13f, 3000.0f, 2.5f, 5.0f, "fruity apple mild",          "RT",   0, 0, 0.06f,  "fruity apple sweet mild",          0.5f,     "beverages" },
{ "Methyl valerate",     "624-24-8",    2752,  50.0f,   2.0f,  20.0f, 116.16f, 2000.0f, 2.5f, 5.0f, "fruity apple sweet",         "RT",   0, 0, 0.08f,  "fruity apple sweet mild",          0.05f,    "beverages|confections" },
{ "Ethyl 2-methylpropanoate","97-62-1", 2428, 100.0f,   5.0f,  50.0f, 116.16f, 2400.0f, 2.5f, 5.0f, "fruity sweet rum",           "RT",   0, 0, 0.08f,  "fruity sweet rum apple",           0.01f,    "beverages" },
{ "Methyl hexanoate",    "106-70-7",    2708,  50.0f,   1.0f,  15.0f, 130.19f,  300.0f, 2.5f, 5.0f, "fruity oily pineapple",      "RT",   1, 0, 0.12f,  "fruity oily pineapple sweet",      0.025f,   "beverages|confections" },
{ "Methyl octanoate",    "111-11-5",    2728,  50.0f,   0.5f,  10.0f, 158.24f,  200.0f, 2.5f, 5.0f, "oily fruity waxy",           "RT",   1, 0, 0.12f,  "oily fruity waxy mild",            0.03f,    "beverages|confections" },

/* =========================================================================
   ESTERS — ethyl long chain
   ========================================================================= */
{ "Ethyl hexanoate",     "123-66-0",    2439,  50.0f,   2.0f,  20.0f, 144.21f,  500.0f, 2.5f, 5.0f, "apple fruity green",         "RT",   1, 0, 0.08f,  "apple fruity green sweet",         0.0001f,  "beverages|alcoholic" },
{ "Ethyl isovalerate",   "108-64-5",    2463,  50.0f,   2.0f,  20.0f, 130.19f,  800.0f, 2.5f, 5.0f, "fruity apple sweet",         "RT",   1, 0, 0.15f,  "fruity apple sweet tropical",      0.0001f,  "beverages|confections" },
{ "Ethyl lactate",       "97-64-3",     2440, 100.0f,   5.0f,  50.0f, 118.13f,    0.0f, 2.5f, 6.0f, "milky buttery mild",         "RT",   0, 0, 0.06f,  "milky buttery mild sour",          14.0f,    "alcoholic|beverages|dairy" },
{ "Ethyl octanoate",     "106-32-1",    2449,  50.0f,   1.0f,  15.0f, 172.27f,  300.0f, 2.5f, 5.0f, "fruity waxy wine-like",      "RT",   1, 0, 0.15f,  "fruity waxy wine-like tropical",   0.0002f,  "beverages|alcoholic" },
{ "Ethyl decanoate",     "110-38-3",    2432,  50.0f,   1.0f,  10.0f, 200.32f,   50.0f, 2.5f, 5.0f, "waxy fruity coconut brandy", "RT",   1, 0, 0.20f,  "waxy fruity coconut brandy",       0.002f,   "beverages|alcoholic|confections" },
{ "Ethyl nonanoate",     "123-29-5",    2447,  30.0f,   0.5f,   8.0f, 186.29f,  100.0f, 2.5f, 5.0f, "fruity waxy cognac",         "RT",   1, 0, 0.25f,  "fruity waxy cognac rose",          0.007f,   "beverages|alcoholic" },
{ "Ethyl 2-methylbutyrate","7452-79-1", 2443,  20.0f,   0.5f,   5.0f, 130.19f, 1200.0f, 2.5f, 5.0f, "fruity apple whisky",        "2-8C", 1, 0, 1.50f,  "fruity apple green whisky",        0.00003f, "beverages" },
{ "Methyl 2-methylbutyrate","868-57-5", 2719,  20.0f,   0.2f,   3.0f, 116.16f, 2000.0f, 2.5f, 5.0f, "apple fruity sweet",         "2-8C", 0, 0, 2.00f,  "apple fruity sweet green",         0.000016f,"beverages|confections" },

/* =========================================================================
   ESTERS — aromatic / specialty
   ========================================================================= */
{ "Methyl anthranilate",  "134-20-3",   2682, 100.0f,   5.0f,  50.0f, 151.17f, 1500.0f, 2.5f, 5.0f, "grape floral concord",       "RT",   1, 0, 0.20f,  "grape concord floral sweet",       0.05f,    "beverages|confections" },
{ "Dimethyl anthranilate","5813-53-6",  2382,  50.0f,   2.0f,  20.0f, 165.19f, 1000.0f, 3.0f, 5.0f, "grape concord floral",       "RT",   1, 0, 0.25f,  "grape concord floral sweet",       0.05f,    "beverages|confections" },
{ "Benzyl propionate",   "122-63-4",    2150,  50.0f,   1.0f,  15.0f, 164.20f, 1000.0f, 2.5f, 5.0f, "floral fruity rose sweet",   "RT",   1, 0, 0.20f,  "floral fruity rose sweet",         0.3f,     "confections|beverages|baked goods" },
{ "Methyl cinnamate",    "103-26-4",    2698,  50.0f,   2.0f,  20.0f, 162.19f, 1500.0f, 2.5f, 5.0f, "sweet balsamic fruity",      "RT",   1, 0, 0.15f,  "sweet balsamic fruity cinnamon",   0.25f,    "confections|baked goods|beverages" },
{ "Phenethyl acetate",   "103-45-7",    2857, 100.0f,   2.0f,  20.0f, 164.20f, 5500.0f, 2.5f, 5.0f, "rose floral honey",          "RT",   1, 0, 0.10f,  "rosy floral honey sweet",          0.025f,   "beverages|confections|baked goods" },
{ "Methyl salicylate",   "119-36-8",    2745,  50.0f,   2.0f,  20.0f, 152.15f,  700.0f, 2.5f, 5.0f, "wintergreen medicinal sweet","RT",   1, 0, 0.04f,  "wintergreen sweet medicinal mint", 0.04f,    "beverages|confections" },
{ "Ethyl salicylate",    "118-61-6",    2458,  50.0f,   2.0f,  20.0f, 166.17f, 5600.0f, 2.5f, 5.0f, "wintergreen fruity sweet",   "RT",   1, 0, 0.08f,  "wintergreen fruity sweet mint",    0.008f,   "beverages|confections" },
{ "Allyl isovalerate",   "2835-39-4",   2045,  50.0f,   2.0f,  20.0f, 156.22f,  500.0f, 2.5f, 5.0f, "fruity apple banana sweet",  "RT",   1, 0, 0.20f,  "fruity apple banana sweet",        0.0017f,  "beverages|confections" },
{ "Allyl hexanoate",     "123-68-2",    2032,  50.0f,   2.0f,  20.0f, 156.22f,  500.0f, 2.5f, 5.0f, "fruity pineapple tropical",  "RT",   1, 0, 0.20f,  "fruity pineapple tropical sweet",  0.05f,    "beverages|confections" },
{ "Ethyl phenylacetate", "101-97-3",    2452,  50.0f,   1.0f,  20.0f, 164.20f, 3000.0f, 2.5f, 5.0f, "honey rose sweet floral",    "RT",   1, 0, 0.15f,  "honey rose sweet floral mild",     0.01f,    "confections|beverages" },
{ "Methyl benzoate",     "93-58-3",     2683,  50.0f,   1.0f,  15.0f, 136.15f, 2100.0f, 2.5f, 5.0f, "fruity floral medicinal",    "RT",   0, 0, 0.05f,  "fruity floral medicinal sweet",    0.03f,    "confections|beverages|baked goods" },
{ "Ethyl benzoate",      "93-89-0",     2422,  50.0f,   1.0f,  15.0f, 150.17f,  600.0f, 2.5f, 5.0f, "fruity floral sweet",        "RT",   1, 0, 0.06f,  "fruity floral sweet mild",         0.06f,    "confections|beverages" },
{ "Benzyl formate",      "104-57-4",    2145,  50.0f,   1.0f,  15.0f, 136.15f, 3000.0f, 2.5f, 5.0f, "sweet fruity floral",        "RT",   0, 0, 0.10f,  "sweet fruity floral rose",         0.3f,     "confections|beverages|baked goods" },
{ "Benzyl butyrate",     "103-37-7",    2140,  50.0f,   1.0f,  15.0f, 178.23f, 1000.0f, 3.0f, 5.0f, "fruity floral rose sweet",   "RT",   1, 0, 0.15f,  "fruity floral rose sweet mild",    0.3f,     "confections|beverages" },
{ "Benzyl cinnamate",    "103-41-3",    2142,  30.0f,   0.5f,  10.0f, 238.28f,  100.0f, 3.0f, 5.0f, "sweet balsamic floral",      "RT",   1, 0, 0.15f,  "sweet balsamic floral sweet",      0.3f,     "confections|beverages" },
{ "Cinnamyl acetate",    "103-54-8",    2293,  50.0f,   1.0f,  15.0f, 176.21f, 1000.0f, 3.0f, 5.0f, "floral sweet cinnamon",      "RT",   1, 0, 0.20f,  "floral sweet cinnamon balsamic",   0.7f,     "confections|beverages|baked goods" },
{ "Cinnamyl propionate", "103-56-0",    2300,  30.0f,   0.5f,  10.0f, 190.24f, 1000.0f, 3.0f, 5.5f, "floral sweet balsamic",      "RT",   1, 0, 0.20f,  "floral sweet balsamic cinnamon",   0.3f,     "confections|beverages" },
{ "cis-3-Hexenyl acetate","3681-71-8",  3171,  50.0f,   1.0f,  20.0f, 142.20f, 2500.0f, 2.5f, 5.0f, "fresh green fruity",         "RT",   0, 0, 0.20f,  "fresh green fruity sweet",         0.002f,   "beverages" },
{ "trans-2-Hexenyl acetate","2497-18-9",3352, 50.0f,   1.0f,  15.0f, 142.20f, 2000.0f, 2.5f, 5.0f, "fruity green fresh",         "RT",   0, 0, 0.30f,  "fruity green fresh sweet",         0.003f,   "beverages" },
{ "Ethyl 3-phenylpropanoate","2021-28-5",2455,50.0f,   1.0f,  15.0f, 178.23f, 1000.0f, 3.0f, 5.0f, "rose fruity sweet",          "RT",   1, 0, 0.15f,  "rose fruity sweet balsamic",       0.5f,     "beverages|confections" },
{ "2-Phenylethyl propanoate","122-70-3",2867, 50.0f,   1.0f,  15.0f, 178.23f, 2000.0f, 3.0f, 5.0f, "rose floral sweet",          "RT",   1, 0, 0.20f,  "rose floral sweet honey",          0.2f,     "confections|beverages" },

/* =========================================================================
   ESTERS — terpene
   ========================================================================= */
{ "Geranyl acetate",     "105-87-3",    2509, 100.0f,   2.0f,  20.0f, 196.29f,  100.0f, 3.0f, 5.5f, "rose floral fruity",         "RT",   1, 0, 0.10f,  "rosy floral fruity sweet",         0.09f,    "beverages|confections" },
{ "Linalyl acetate",     "115-95-7",    2636, 100.0f,   2.0f,  20.0f, 196.29f,  100.0f, 3.0f, 5.0f, "bergamot floral lavender",   "RT",   1, 0, 0.06f,  "bergamot floral lavender sweet",   0.003f,   "beverages|confections" },
{ "Citronellyl acetate", "150-84-5",    2310, 100.0f,   2.0f,  20.0f, 198.30f,  100.0f, 3.0f, 5.0f, "rose fruity floral",         "RT",   1, 0, 0.12f,  "rosy fruity floral sweet",         0.007f,   "beverages|confections" },
{ "Neryl acetate",       "141-12-8",    2773,  50.0f,   1.0f,  15.0f, 196.29f,  100.0f, 3.0f, 5.0f, "rose floral bergamot",       "RT",   1, 0, 0.15f,  "rose floral bergamot sweet",       0.3f,     "beverages|confections" },
{ "Geranyl butyrate",    "106-29-6",    2512,  50.0f,   1.0f,  10.0f, 224.34f,   10.0f, 3.0f, 5.0f, "rose fruity sweet",          "RT",   1, 0, 0.20f,  "rosy fruity sweet tropical",       0.02f,    "beverages|confections" },
{ "Linalyl formate",     "115-99-1",    2640,  50.0f,   1.0f,  10.0f, 182.26f,  100.0f, 3.0f, 5.0f, "floral herbaceous sweet",    "RT",   1, 0, 0.15f,  "floral herbaceous sweet rose",     0.6f,     "beverages|confections" },
{ "Geranyl formate",     "105-86-2",    2514,  50.0f,   1.0f,  10.0f, 182.26f,  100.0f, 3.0f, 5.0f, "floral rose citrus",         "RT",   1, 0, 0.15f,  "floral rosy citrus sweet",         0.1f,     "beverages|confections" },
{ "Methyl geranate",     "6776-19-8",   2642,  30.0f,   0.5f,   8.0f, 182.26f,   50.0f, 3.0f, 5.0f, "rosy floral citrus",         "RT",   1, 0, 0.30f,  "rosy floral citrus sweet",         0.1f,     "beverages|confections" },
{ "Terpinyl acetate",    "80-26-2",     3047, 100.0f,   2.0f,  20.0f, 196.29f,  100.0f, 3.0f, 5.0f, "pine herbal citrus",         "RT",   1, 0, 0.10f,  "pine herbal citrus sweet",         0.4f,     "beverages|baked goods" },

/* =========================================================================
   ACIDS
   ========================================================================= */
{ "Acetic acid",         "64-19-7",     2006, 500.0f,  20.0f, 200.0f,  60.05f,    0.0f, 1.5f, 5.0f, "vinegar sour pungent",       "RT",   0, 0, 0.01f,  "vinegar sour sharp pungent",       24.0f,    "beverages|savory|baked goods" },
{ "Propionic acid",      "79-09-4",     2924, 200.0f,   5.0f,  80.0f,  74.08f,    0.0f, 2.0f, 5.0f, "sour cheesy pungent",        "RT",   0, 0, 0.03f,  "sour cheesy pungent rancid",       8.2f,     "baked goods|dairy" },
{ "Hexanoic acid",       "142-62-1",    2559, 100.0f,   1.0f,  20.0f, 116.16f, 1100.0f, 2.5f, 5.0f, "cheesy fatty rancid",        "RT",   0, 0, 0.05f,  "cheesy fatty rancid goat",         0.8f,     "dairy|savory" },
{ "Octanoic acid",       "124-07-2",    2799, 100.0f,   2.0f,  20.0f, 144.21f,  700.0f, 2.5f, 5.0f, "fatty cheesy oily",          "RT",   0, 0, 0.05f,  "fatty cheesy oily rancid",         8.0f,     "dairy|savory" },
{ "Decanoic acid",       "334-48-5",    2364,  50.0f,   1.0f,  10.0f, 172.27f,  150.0f, 2.5f, 5.0f, "fatty soapy waxy",           "RT",   1, 0, 0.06f,  "fatty soapy waxy mild",            10.0f,    "dairy|savory" },
{ "Isovaleric acid",     "503-74-2",    3102,  50.0f,   0.5f,  10.0f, 102.13f,    0.0f, 2.5f, 5.0f, "cheesy sweaty fermented",    "RT",   0, 0, 0.10f,  "cheesy sweaty fermented sour",     0.12f,    "dairy" },
{ "2-Methylbutyric acid","116-53-0",    2695,  50.0f,   0.5f,  10.0f, 102.13f,    0.0f, 2.5f, 5.0f, "fruity cheesy sour",         "RT",   0, 0, 0.25f,  "fruity cheesy sour dairy",         0.15f,    "dairy|baked goods" },
{ "Isobutyric acid",     "79-31-2",     2222,  50.0f,   1.0f,  10.0f,  88.11f,    0.0f, 2.5f, 5.0f, "butter rancid sharp",        "RT",   0, 0, 0.06f,  "butter rancid sharp cheesy",       0.55f,    "dairy|alcoholic" },
{ "Valeric acid",        "109-52-4",    3101,  50.0f,   0.5f,  10.0f, 102.13f,    0.0f, 2.5f, 5.0f, "sweaty cheesy dairy",        "RT",   0, 0, 0.08f,  "sweaty cheesy dairy fermented",    0.29f,    "dairy|savory" },
{ "Dodecanoic acid",     "143-07-7",    2614,  50.0f,   1.0f,  10.0f, 200.32f,   55.0f, 3.0f, 5.0f, "soapy fatty waxy mild",      "RT",   1, 0, 0.03f,  "soapy fatty waxy mild",            10.0f,    "dairy" },
{ "Tetradecanoic acid",  "544-63-8",    2764,  30.0f,   0.5f,   8.0f, 228.37f,   20.0f, 3.0f, 5.0f, "fatty soapy waxy",           "RT",   1, 0, 0.04f,  "fatty soapy waxy mild",            20.0f,    "dairy" },
{ "Hexadecanoic acid",   "57-10-3",     2832,  30.0f,   0.5f,   5.0f, 256.42f,    7.0f, 3.0f, 5.0f, "waxy soapy mild",            "RT",   1, 0, 0.03f,  "waxy soapy fatty mild",            50.0f,    "dairy" },
{ "Phenylacetic acid",   "103-82-2",    2878,  50.0f,   1.0f,  20.0f, 136.15f,16600.0f, 3.0f, 5.5f, "honey rosy sweet",           "RT",   0, 0, 0.05f,  "honey rosy sweet floral",          1.6f,     "savory|confections|baked goods" },
{ "Cinnamic acid",       "621-82-9",    2288,  50.0f,   1.0f,  15.0f, 148.16f,  400.0f, 3.0f, 5.5f, "honey balsamic sweet",       "RT",   0, 0, 0.10f,  "honey balsamic sweet floral",      50.0f,    "confections|baked goods|beverages" },

/* =========================================================================
   LACTONES
   ========================================================================= */
{ "Gamma-caprolactone",  "695-06-7",    2360,  50.0f,   1.0f,  20.0f, 114.14f, 5000.0f, 2.5f, 5.5f, "coconut sweet creamy",       "RT",   0, 0, 0.80f,  "coconut sweet creamy mild",        0.01f,    "confections|baked goods|beverages" },
{ "Gamma-octalactone",   "104-50-7",    2796,  50.0f,   1.0f,  15.0f, 142.20f, 1000.0f, 3.0f, 5.5f, "coconut creamy sweet",       "RT",   1, 0, 1.00f,  "coconut creamy sweet fruity",      0.007f,   "confections|beverages|dairy" },
{ "Gamma-nonalactone",   "104-61-0",    2781,  50.0f,   0.5f,  10.0f, 156.22f,  200.0f, 3.0f, 5.5f, "coconut peach creamy",       "RT",   1, 0, 1.00f,  "coconut peach creamy sweet",       0.024f,   "confections|beverages|dairy" },
{ "Gamma-valerolactone", "108-29-2",    3103,  50.0f,   1.0f,  15.0f, 100.12f,    0.0f, 2.5f, 5.5f, "herbaceous sweet mild",      "RT",   0, 0, 0.50f,  "herbaceous sweet mild coconut",    0.25f,    "confections|baked goods" },
{ "Gamma-butyrolactone", "96-48-0",     3291, 100.0f,   5.0f,  50.0f,  86.09f,    0.0f, 2.5f, 5.0f, "creamy caramel sweet",       "RT",   0, 0, 0.10f,  "creamy caramel sweet mild",        3.0f,     "beverages|confections|baked goods" },
{ "Epsilon-caprolactone","502-44-3",    3396,  30.0f,   1.0f,  10.0f, 114.14f, 5000.0f, 2.5f, 5.5f, "sweet milky creamy",         "RT",   0, 0, 0.50f,  "sweet milky creamy coconut",       0.07f,    "confections|dairy" },
{ "Delta-dodecalactone", "713-95-1",    2401,  20.0f,   0.5f,   8.0f, 198.31f,  100.0f, 3.0f, 5.5f, "peach coconut creamy",       "RT",   1, 0, 2.00f,  "peach coconut creamy fruity",      0.006f,   "confections|beverages|dairy" },
{ "Delta-octalactone",   "698-76-0",    2795,  30.0f,   0.5f,  10.0f, 142.20f,  500.0f, 3.0f, 5.5f, "fatty coconut creamy",       "RT",   1, 0, 1.50f,  "fatty coconut creamy sweet",       0.7f,     "dairy|confections" },
{ "Delta-nonalactone",   "3301-94-8",   2780,  30.0f,   0.2f,   8.0f, 156.22f,  100.0f, 3.0f, 5.5f, "coconut peach fatty",        "RT",   1, 0, 1.50f,  "coconut peach fatty creamy",       0.1f,     "dairy|confections" },
{ "Dihydroactinidiolide","15356-74-8",  3633,  20.0f,   0.1f,   5.0f, 168.23f,  200.0f, 3.0f, 5.5f, "tea tobacco sweet woody",    "RT",   1, 0, 2.00f,  "tea tobacco sweet woody fruity",   0.001f,   "beverages|confections" },
{ "2(5H)-Furanone",      "497-23-4",    3353,  30.0f,   1.0f,  10.0f,  84.07f,    0.0f, 3.0f, 5.5f, "sweet caramel mild",         "RT",   0, 0, 0.50f,  "sweet caramel mild buttery",       0.12f,    "baked goods|confections" },

/* =========================================================================
   TERPENES / PHENYLPROPANOIDS
   ========================================================================= */
{ "Limonene",            "5989-27-5",   2633, 500.0f,   5.0f, 100.0f, 136.23f,   10.0f, 2.5f, 5.0f, "citrus orange lemon fresh",  "RT",   1, 0, 0.03f,  "citrus orange lemon fresh bright", 0.1f,     "beverages|confections|baked goods" },
{ "d-Carvone",           "2244-16-8",   2249, 500.0f,  10.0f, 100.0f, 150.22f,  900.0f, 2.5f, 5.5f, "spearmint caraway fresh",    "RT",   1, 0, 0.15f,  "spearmint caraway fresh minty",    0.049f,   "beverages|confections|savory" },
{ "l-Carvone",           "6485-40-1",   2249, 500.0f,  10.0f, 100.0f, 150.22f,  900.0f, 2.5f, 5.5f, "spearmint fresh cool",       "RT",   1, 0, 0.20f,  "spearmint fresh cool minty",       0.049f,   "beverages|confections" },
{ "Pulegone",            "89-82-7",     2963, 200.0f,   5.0f,  50.0f, 152.23f,  200.0f, 2.5f, 5.0f, "peppermint minty warm",      "RT",   1, 0, 0.50f,  "peppermint minty warm cool",       0.09f,    "beverages|confections" },
{ "Thymol",              "89-83-8",     3066,  50.0f,   1.0f,  10.0f, 150.22f, 1000.0f, 2.5f, 5.0f, "thyme herbal medicinal",     "RT",   1, 0, 0.10f,  "thyme herbal medicinal spicy",     0.05f,    "savory|beverages" },
{ "Carvacrol",           "499-75-2",    2245,  50.0f,   1.0f,  10.0f, 150.22f, 1000.0f, 2.5f, 5.0f, "oregano thyme herbal",       "RT",   1, 0, 0.15f,  "oregano thyme herbal spicy",       0.004f,   "savory|meat" },
{ "Anethole",            "4180-23-8",   2086, 200.0f,   5.0f,  50.0f, 148.20f,  150.0f, 2.5f, 5.0f, "anise licorice sweet",       "RT",   1, 0, 0.05f,  "anise licorice sweet fennel",      0.05f,    "beverages|confections|savory" },
{ "Estragole",           "140-67-0",    2411, 100.0f,   5.0f,  40.0f, 148.20f, 1000.0f, 2.5f, 5.0f, "anise sweet herbaceous",     "RT",   1, 0, 0.10f,  "anise sweet herbaceous tarragon",  0.04f,    "beverages|savory" },
{ "Isoeugenol",          "97-54-1",     2468,  50.0f,   1.0f,  20.0f, 164.20f,  900.0f, 2.5f, 5.5f, "clove spicy carnation",      "RT",   1, 0, 0.10f,  "clove spicy carnation floral",     0.006f,   "beverages|baked goods|confections" },
{ "Methyl eugenol",      "93-15-2",     2476, 100.0f,   2.0f,  30.0f, 178.23f, 2000.0f, 2.5f, 5.0f, "clove sweet spicy",          "RT",   1, 0, 0.10f,  "clove sweet spicy warm",           0.0001f,  "beverages|baked goods" },
{ "Camphor",             "76-22-2",     2230,  20.0f,   0.5f,   5.0f, 152.23f, 1200.0f, 3.0f, 5.0f, "medicinal cool camphoreous", "RT",   1, 0, 0.05f,  "medicinal cool camphoreous minty", 0.11f,    "confections|savory" },
{ "Borneol",             "507-70-0",    2157,  50.0f,   1.0f,  15.0f, 154.25f, 4000.0f, 3.0f, 5.5f, "camphoreous cool herbal",    "RT",   1, 0, 0.08f,  "camphoreous cool herbal pine",     0.25f,    "savory|beverages" },
{ "Fenchone",            "1195-79-5",   2461, 100.0f,   5.0f,  50.0f, 152.23f,  100.0f, 3.0f, 5.0f, "fennel camphoreous herbal",  "RT",   1, 0, 0.15f,  "fennel camphoreous herbal cool",   0.001f,   "beverages|savory" },
{ "1,8-Cineole",         "470-82-6",    2465, 100.0f,   2.0f,  20.0f, 154.25f, 3600.0f, 2.5f, 5.5f, "eucalyptus camphoreous cool","RT",   1, 0, 0.05f,  "eucalyptus cool camphoreous mint", 0.024f,   "beverages|confections|savory" },
{ "Terpinen-4-ol",       "562-74-3",    2291,  50.0f,   1.0f,  15.0f, 154.25f, 2700.0f, 2.5f, 5.5f, "herbal musty pepper earthy", "RT",   1, 0, 0.10f,  "herbal musty pepper earthy spicy", 0.007f,   "savory|beverages" },

/* =========================================================================
   TERPENE HYDROCARBONS
   ========================================================================= */
{ "alpha-Pinene",        "80-56-8",     2902, 200.0f,   5.0f,  50.0f, 136.23f,   10.0f, 2.5f, 5.0f, "pine turpentine fresh",      "RT",   1, 0, 0.02f,  "pine turpentine fresh resinous",   6.0f,     "beverages|savory|baked goods" },
{ "Camphene",            "79-92-5",     2229,  50.0f,   1.0f,  15.0f, 136.23f,   10.0f, 3.0f, 5.0f, "camphoreous herbal pine",    "RT",   1, 0, 0.05f,  "camphoreous herbal pine fresh",    0.8f,     "savory|beverages" },
{ "Myrcene",             "123-35-3",    2762,  50.0f,   1.0f,  15.0f, 136.23f,   10.0f, 2.5f, 5.0f, "herbal citrus green musky",  "RT",   1, 0, 0.05f,  "herbal citrus green musky",        0.01f,    "beverages|baked goods" },
{ "alpha-Terpinene",     "99-86-5",     3558,  50.0f,   1.0f,  15.0f, 136.23f,   10.0f, 2.5f, 5.0f, "citrus herbal fresh",        "RT",   1, 0, 0.05f,  "citrus herbal fresh clean",        0.1f,     "beverages" },
{ "gamma-Terpinene",     "99-85-4",     3559,  50.0f,   1.0f,  15.0f, 136.23f,   10.0f, 2.5f, 5.0f, "citrus herbal turpentine",   "RT",   1, 0, 0.05f,  "citrus herbal turpentine fresh",   0.09f,    "beverages" },
{ "p-Cymene",            "99-87-6",     2356,  50.0f,   1.0f,  15.0f, 134.22f,   50.0f, 2.5f, 5.0f, "citrus fresh herbal",        "RT",   1, 0, 0.03f,  "citrus fresh herbal spicy",        0.013f,   "beverages|savory" },
{ "Sabinene",            "3387-41-5",   3513,  50.0f,   1.0f,  10.0f, 136.23f,   10.0f, 2.5f, 5.0f, "herbal woody spicy",         "RT",   1, 0, 0.15f,  "herbal woody spicy fresh",         0.1f,     "beverages" },

/* =========================================================================
   SESQUITERPENES AND SPECIALTY TERPENES
   ========================================================================= */
{ "alpha-Ionone",        "127-41-3",    2594,  50.0f,   1.0f,  15.0f, 192.30f,  100.0f, 3.0f, 5.0f, "violet floral woody",        "RT",   1, 0, 0.50f,  "violet floral woody orris",        0.000017f,"beverages|confections" },
{ "beta-Ionone",         "14901-07-6",  2595,  50.0f,   0.5f,  10.0f, 192.30f,   50.0f, 3.0f, 5.0f, "woody violet orris sweet",   "RT",   1, 0, 0.60f,  "woody violet orris sweet floral",  0.000007f,"beverages|confections" },
{ "alpha-Damascone",     "43052-87-5",  3659,  20.0f,   0.1f,   3.0f, 192.30f,   50.0f, 3.0f, 5.0f, "rose fruity apple sweet",    "RT",   1, 0, 2.00f,  "rose fruity apple sweet orris",    0.00004f, "beverages|confections" },
{ "Dihydrojasmone",      "1128-08-1",   3168,  30.0f,   0.5f,  10.0f, 166.26f,  200.0f, 3.0f, 5.0f, "floral jasmine fruity",      "RT",   1, 0, 1.50f,  "floral jasmine fruity sweet",      0.01f,    "confections|beverages" },
{ "Nerolidol",           "7212-44-4",   2772,  30.0f,   0.5f,   8.0f, 222.37f,   10.0f, 3.0f, 5.0f, "floral woody green rose",    "RT",   1, 0, 0.80f,  "floral woody green rosy sweet",    0.05f,    "confections|beverages" },
{ "Rose oxide",          "16409-43-1",  3236,  20.0f,   0.1f,   5.0f, 154.25f,  200.0f, 2.5f, 5.0f, "rose lychee floral",         "RT",   1, 0, 2.00f,  "rose lychee floral geranium",      0.000002f,"beverages|confections" },
{ "Dihydromyrcenol",     "18479-58-8",  3420,  50.0f,   1.0f,  15.0f, 156.27f,  100.0f, 2.5f, 5.0f, "citrus fresh clean",         "RT",   1, 0, 0.10f,  "citrus fresh clean sweet",         0.033f,   "beverages" },
{ "Raspberry ketone",    "5471-51-2",   2588,  50.0f,   1.0f,  20.0f, 164.20f, 2000.0f, 2.5f, 5.0f, "raspberry fruity sweet",     "RT",   0, 0, 0.15f,  "raspberry fruity sweet floral",    0.001f,   "beverages|confections" },

/* =========================================================================
   PHENOLS AND RELATED
   ========================================================================= */
{ "Guaiacol",            "90-05-1",     2532,  50.0f,   1.0f,  20.0f, 124.14f,18000.0f, 3.0f, 5.5f, "smoky phenolic spicy",       "RT",   0, 0, 0.10f,  "smoky phenolic spicy medicinal",   0.021f,   "beverages|savory|meat" },
{ "4-Vinylguaiacol",     "7786-61-0",   2238,  50.0f,   1.0f,  15.0f, 150.17f, 2000.0f, 3.0f, 5.5f, "spicy clove smoky phenolic", "RT",   0, 0, 0.30f,  "spicy clove smoky phenolic",       0.003f,   "beverages|baked goods|savory" },
{ "Skatole",             "83-34-1",     3019,   5.0f,  0.05f,   1.0f, 131.17f, 1000.0f, 3.0f, 5.5f, "fecal floral indolic",       "RT",   0, 0, 0.50f,  "fecal floral indolic animalic",    0.0001f,  "confections|beverages" },
{ "Indole",              "120-72-9",    2593,  10.0f,   0.1f,   3.0f, 117.15f, 2000.0f, 3.0f, 5.5f, "floral jasmine indolic",     "RT",   0, 0, 0.15f,  "floral jasmine indolic sweet",     0.0014f,  "confections|beverages" },
{ "p-Anisyl alcohol",    "105-13-5",    2099,  50.0f,   1.0f,  10.0f, 138.17f, 2600.0f, 3.0f, 5.0f, "sweet floral anise",         "RT",   0, 0, 0.15f,  "sweet floral anise mild",          0.1f,     "confections|beverages" },
{ "Phenyl propyl alcohol","122-97-4",   2885,  50.0f,   1.0f,  15.0f, 136.19f, 7000.0f, 3.0f, 5.5f, "rosy floral sweet",          "RT",   0, 0, 0.20f,  "rosy floral sweet honey",          0.3f,     "confections|beverages" },

/* =========================================================================
   HETEROCYCLES — PYRAZINES AND FURANS (baked goods / savory)
   ========================================================================= */
{ "Pyrazine",            "290-37-9",    3268, 100.0f,   2.0f,  30.0f,  80.09f,    0.0f, 3.0f, 6.0f, "roasted nutty green",        "RT",   0, 0, 0.30f,  "roasted nutty green earthy",       0.17f,    "baked goods|savory|meat" },
{ "2-Methylpyrazine",    "109-08-0",    3309, 100.0f,   2.0f,  30.0f,  94.12f,    0.0f, 3.0f, 6.0f, "roasted nutty chocolate",    "RT",   0, 0, 0.30f,  "roasted nutty chocolate earthy",   0.4f,     "baked goods|savory" },
{ "2,3-Dimethylpyrazine","5910-89-4",   3271,  50.0f,   1.0f,  15.0f, 108.14f,    0.0f, 3.0f, 6.0f, "roasted nutty cocoa",        "RT",   0, 0, 0.50f,  "roasted nutty cocoa chocolate",    0.18f,    "baked goods|savory" },
{ "2,5-Dimethylpyrazine","123-32-0",    3272,  50.0f,   1.0f,  15.0f, 108.14f,    0.0f, 3.0f, 6.0f, "roasted cocoa nutty",        "RT",   0, 0, 0.50f,  "roasted cocoa nutty bread",        1.8f,     "baked goods|savory|confections" },
{ "2,6-Dimethylpyrazine","108-50-9",    3273,  50.0f,   1.0f,  15.0f, 108.14f,    0.0f, 3.0f, 6.0f, "roasted nutty caramel",      "RT",   0, 0, 0.50f,  "roasted nutty caramel earthy",     1.6f,     "baked goods|savory" },
{ "Trimethylpyrazine",   "14667-55-1",  3244,  50.0f,   0.5f,  10.0f, 122.17f,    0.0f, 3.0f, 6.0f, "roasted cocoa musty earthy", "RT",   0, 0, 0.60f,  "roasted cocoa musty earthy",       0.4f,     "baked goods|savory|confections" },
{ "Tetramethylpyrazine", "1124-11-4",   3237,  50.0f,   0.5f,  10.0f, 136.19f,    0.0f, 3.0f, 6.0f, "musty cocoa roasted",        "RT",   0, 0, 0.60f,  "musty cocoa roasted chocolate",    0.9f,     "baked goods|confections" },
{ "2-Ethylpyrazine",     "13925-00-3",  3301,  30.0f,   0.5f,   8.0f, 108.14f,    0.0f, 3.0f, 6.0f, "nutty roasted earthy",       "RT",   0, 0, 0.80f,  "nutty roasted earthy bread",       0.4f,     "baked goods|savory" },
{ "2-Acetylpyrazine",    "22047-25-2",  3126,  30.0f,   0.5f,   5.0f, 122.12f,    0.0f, 3.0f, 6.0f, "roasted popcorn nutty",      "RT",   0, 0, 1.50f,  "roasted popcorn nutty bread",      0.4f,     "baked goods|savory|confections" },
{ "2,3-Diethylpyrazine", "15707-24-1",  3269,  30.0f,   0.5f,   8.0f, 136.19f,    0.0f, 3.0f, 6.0f, "roasted nutty earthy",       "RT",   0, 0, 1.00f,  "roasted nutty earthy cocoa",       0.04f,    "baked goods|savory" },
{ "2-Acetylpyridine",    "1122-62-9",   3251,  10.0f,   0.2f,   3.0f, 121.14f,    0.0f, 3.0f, 6.0f, "popcorn corn roasted",       "RT",   0, 0, 0.80f,  "popcorn corn roasted nutty",       0.001f,   "baked goods|savory" },
{ "2-Acetylpyrrole",     "1072-83-9",   3382,  20.0f,   0.5f,   8.0f, 109.13f,25000.0f, 3.0f, 5.5f, "bread roasted nutty",        "RT",   0, 0, 1.00f,  "bread roasted nutty chocolate",    0.007f,   "baked goods|confections" },
{ "Furfural",            "98-01-1",     2489,  50.0f,   2.0f,  20.0f,  96.08f,    0.0f, 3.0f, 5.5f, "bread caramel almond",       "RT",   0, 0, 0.05f,  "bread caramel almond sweet",       3.0f,     "baked goods|beverages" },
{ "5-Methylfurfural",    "620-02-0",    2748,  50.0f,   1.0f,  15.0f, 110.11f,    0.0f, 3.0f, 5.5f, "caramel bread sweet",        "RT",   0, 0, 0.30f,  "caramel bread sweet nutty",        0.45f,    "baked goods|confections" },
{ "2-Acetylfuran",       "1192-62-7",   3163,  30.0f,   1.0f,  10.0f, 110.11f,    0.0f, 3.0f, 5.5f, "sweet balsamic roasted",     "RT",   0, 0, 0.50f,  "sweet balsamic roasted nutty",     0.5f,     "baked goods|confections" },
{ "Furfuryl acetate",    "623-17-6",    2490,  30.0f,   1.0f,  10.0f, 140.14f, 5000.0f, 3.0f, 5.0f, "sweet caramel coffee",       "RT",   0, 0, 0.30f,  "sweet caramel coffee roasted",     0.4f,     "baked goods|beverages" },
{ "Methyl furoate",      "611-13-2",    2703,  30.0f,   1.0f,  10.0f, 126.11f, 5000.0f, 3.0f, 5.5f, "sweet balsamic caramel",     "RT",   0, 0, 0.20f,  "sweet balsamic caramel nutty",     0.8f,     "baked goods|confections" },
{ "Ethyl 2-furoate",     "614-99-3",    2413,  30.0f,   1.0f,  10.0f, 140.14f, 1000.0f, 3.0f, 5.5f, "sweet balsamic mild",        "RT",   0, 0, 0.30f,  "sweet balsamic mild caramel",      0.3f,     "baked goods|beverages" },
{ "Thiophene",           "110-02-1",    3388,   5.0f,  0.05f,   1.0f,  84.14f, 3600.0f, 3.0f, 5.5f, "sulfurous meaty mild",       "RT",   0, 0, 0.15f,  "sulfurous meaty mild onion",       0.001f,   "savory|meat" },

/* =========================================================================
   HETEROCYCLES — KETONES AND BUTANEDIONES
   ========================================================================= */
{ "2,3-Pentanedione",    "600-14-6",    2841,   5.0f,   0.1f,   2.0f, 100.12f,    0.0f, 2.5f, 5.0f, "butter diacetyl creamy",     "2-8C", 0, 0, 1.00f,  "butter diacetyl creamy sweet",     0.02f,    "dairy|baked goods" },
{ "Acetoin",             "513-86-0",    2008, 100.0f,   5.0f,  50.0f,  88.11f,    0.0f, 2.5f, 5.0f, "buttery cream sweet",        "RT",   0, 0, 0.20f,  "buttery cream sweet dairy",        1.0f,     "dairy|baked goods|beverages" },
{ "Acetol",              "116-09-6",    2840,  50.0f,   2.0f,  20.0f,  74.08f,    0.0f, 3.0f, 5.5f, "sweet caramel mild",         "RT",   0, 0, 0.15f,  "sweet caramel mild buttery",       20.0f,    "baked goods|confections" },

/* =========================================================================
   SULFUR COMPOUNDS (meat / savory)
   ========================================================================= */
{ "Furfuryl mercaptan",  "98-02-2",     2493,   1.0f,  0.01f,   0.3f, 114.17f, 1000.0f, 3.0f, 5.5f, "coffee roasted sulfurous",   "-20C", 0, 1, 5.00f,  "coffee roasted sulfurous meaty",   0.000001f,"beverages|savory|meat" },
{ "2-Methyl-3-furanthiol","28588-74-1", 3188,   1.0f,  0.01f,   0.2f, 114.17f, 1000.0f, 3.0f, 5.5f, "meaty roasted sulfurous",    "-20C", 0, 1, 8.00f,  "meaty roasted sulfurous savory",   0.000000007f,"meat|savory" },
{ "Dimethyl sulfide",    "75-18-3",     2381,   5.0f,  0.05f,   1.0f,  62.13f,22000.0f, 3.0f, 5.5f, "cooked corn sweet sulfury",  "-20C", 0, 1, 0.15f,  "cooked corn sweet sulfury mild",   0.00033f, "savory|beverages" },
{ "Dimethyl disulfide",  "624-92-0",    3536,   5.0f,   0.1f,   2.0f,  94.20f, 2500.0f, 3.0f, 5.0f, "cabbage garlic sulfurous",   "RT",   0, 0, 0.20f,  "cabbage garlic sulfurous onion",   0.0001f,  "savory|meat" },
{ "Methional",           "3268-49-3",   2747,   5.0f,  0.05f,   1.0f,  90.14f,10000.0f, 3.0f, 5.0f, "cooked potato savory",       "2-8C", 0, 1, 3.00f,  "cooked potato savory brothy",      0.00002f, "savory|meat" },
{ "Allyl isothiocyanate","57-06-7",     2034,  10.0f,   0.5f,   5.0f,  99.15f, 2000.0f, 2.5f, 5.0f, "mustard pungent sharp",      "RT",   0, 0, 0.15f,  "mustard pungent sharp hot",        0.001f,   "savory|meat" },
{ "2-Isobutylthiazole",  "18640-74-9",  3134,   5.0f,  0.05f,   1.0f, 141.23f, 1000.0f, 3.0f, 5.5f, "tomato green sulfurous",     "RT",   1, 0, 2.00f,  "tomato green sulfurous savory",    0.0003f,  "savory" },
{ "2-Acetylthiazole",    "24295-03-2",  3328,   5.0f,  0.05f,   1.0f, 127.17f, 2000.0f, 3.0f, 5.5f, "nutty bread popcorn meaty",  "RT",   0, 0, 2.50f,  "nutty bread popcorn meaty roasted",0.001f,   "baked goods|savory|meat" },
{ "Diallyl sulfide",     "592-88-1",    2044,   5.0f,  0.05f,   1.0f, 114.21f,  200.0f, 3.0f, 5.5f, "garlic onion sharp",         "RT",   1, 0, 0.20f,  "garlic onion sharp pungent",       0.012f,   "savory|meat" },
{ "Diallyl disulfide",   "2179-57-9",   2046,   5.0f,   0.1f,   2.0f, 146.27f,  100.0f, 3.0f, 5.5f, "garlic pungent savory",      "RT",   1, 0, 0.20f,  "garlic pungent savory allium",     0.00009f, "savory|meat" },
{ "Dipropyl disulfide",  "629-19-6",    2940,   5.0f,  0.05f,   1.0f, 150.30f,   50.0f, 3.0f, 5.5f, "garlic onion cooked",        "RT",   1, 0, 0.30f,  "garlic onion cooked savory",       0.001f,   "savory|meat" },
{ "Ethyl 3-methylthiopropionate","13327-56-5",3343,5.0f,0.05f, 1.0f, 148.22f,  500.0f, 3.0f, 5.5f, "savory potato brothy",       "RT",   0, 0, 2.00f,  "savory potato brothy meaty",       0.05f,    "savory|meat" },

/* =========================================================================
   KETONES — simple
   ========================================================================= */
{ "Acetone",             "67-64-1",     3326, 200.0f,  10.0f, 100.0f,  58.08f,    0.0f, 2.5f, 5.0f, "solvent fruity mild",        "RT",   0, 0, 0.01f,  "solvent fruity mild sweet",        5.0f,     "beverages|baked goods" },
{ "2-Butanone",          "78-93-3",     2170, 100.0f,   5.0f,  50.0f,  72.11f,    0.0f, 2.5f, 5.0f, "buttery sweet mild",         "RT",   0, 0, 0.02f,  "buttery sweet mild creamy",        0.5f,     "beverages|dairy" },
{ "2-Heptanone",         "110-43-0",    2544,  50.0f,   1.0f,  15.0f, 114.19f, 4300.0f, 2.5f, 5.0f, "blue cheese fruity waxy",    "RT",   0, 0, 0.10f,  "blue cheese fruity waxy dairy",    0.15f,    "dairy|savory" },
{ "2-Nonanone",          "821-55-6",    2785,  50.0f,   0.5f,  10.0f, 142.24f, 1100.0f, 2.5f, 5.0f, "cheese fruity waxy",         "RT",   1, 0, 0.15f,  "cheese fruity waxy dairy mild",    0.025f,   "dairy|savory" },
{ "2-Undecanone",        "112-12-9",    3093,  30.0f,   0.2f,   5.0f, 170.29f,  100.0f, 2.5f, 5.0f, "waxy floral citrus cheesy",  "RT",   1, 0, 0.30f,  "waxy floral citrus cheesy",        0.007f,   "dairy|confections" },
{ "2-Pentanone",         "107-87-9",    2842, 100.0f,   5.0f,  50.0f,  86.13f,    0.0f, 2.5f, 5.0f, "fruity acetone ethereal",    "RT",   0, 0, 0.05f,  "fruity acetone ethereal mild",     0.035f,   "beverages|dairy" },
{ "Acetophenone",        "98-86-2",     2009,  50.0f,   1.0f,  15.0f, 120.15f, 5500.0f, 2.5f, 5.5f, "sweet orange floral almond", "RT",   0, 0, 0.05f,  "sweet orange floral almond mild",  0.065f,   "beverages|confections" },
{ "6-Methyl-5-hepten-2-one","110-93-0", 2707,  50.0f,   1.0f,  10.0f, 126.20f, 1500.0f, 2.5f, 5.0f, "mushroom citrus earthy",     "RT",   0, 0, 0.15f,  "mushroom citrus earthy green",     0.05f,    "savory|beverages" },
{ "Benzophenone",        "119-61-9",    2134,  20.0f,   0.5f,   8.0f, 182.22f,  100.0f, 3.0f, 5.0f, "geranium floral rose",       "RT",   1, 0, 0.05f,  "geranium floral rosy sweet",       0.02f,    "confections|beverages" },
{ "1-Octen-3-one",       "4312-99-6",   3515,   2.0f,  0.01f,   0.3f, 126.20f,  300.0f, 2.5f, 5.0f, "mushroom metallic earthy",   "RT",   1, 1, 8.00f,  "mushroom metallic earthy savory",  0.00001f, "savory|meat" },

/* =========================================================================
   MISCELLANEOUS ALCOHOLIC COMPOUNDS
   ========================================================================= */
{ "Propan-1-ol",         "71-23-8",     2928, 500.0f,  20.0f, 200.0f,  60.10f,    0.0f, 2.5f, 7.0f, "winey spirituous sharp",     "RT",   0, 0, 0.01f,  "winey spirituous sharp fusel",     8.0f,     "alcoholic|beverages" },
{ "Butan-1-ol",          "71-36-3",     2178, 200.0f,   5.0f,  80.0f,  74.12f,    0.0f, 2.5f, 7.0f, "fusel winey sharp",          "RT",   0, 0, 0.02f,  "fusel winey sharp alcoholic",      0.5f,     "alcoholic|beverages" },
{ "Acetal",              "105-57-7",    2002, 200.0f,  10.0f, 100.0f, 118.17f,50000.0f, 2.5f, 4.5f, "ethereal fruity mild",       "RT",   0, 0, 0.05f,  "ethereal fruity mild solvent",     1.5f,     "alcoholic|beverages" },
{ "Ethyl vanillin",      "121-32-4",    2464, 100.0f,  10.0f,  60.0f, 166.17f, 5000.0f, 2.0f, 5.0f, "vanilla sweet creamy strong","RT",   0, 0, 0.10f,  "vanilla sweet creamy stronger",    0.005f,   "beverages|baked goods|dairy|confections" },

/* =========================================================================
   SPECIALTY FLORALS AND ADDITIONAL SAVORY / MEAT
   ========================================================================= */
{ "Hydroxycitronellal",  "107-75-5",    2583,  50.0f,   1.0f,  10.0f, 172.27f, 2000.0f, 3.0f, 5.5f, "lily muguet sweet floral",   "RT",   0, 0, 0.20f,  "lily muguet sweet floral clean",   0.1f,     "confections|beverages" },
{ "2,4,5-Trimethylthiazole","13623-11-5",3325, 5.0f,  0.05f,   1.0f, 127.21f,  500.0f, 3.0f, 5.5f, "meaty nutty roasted",        "RT",   0, 0, 2.00f,  "meaty nutty roasted savory",       0.001f,   "meat|savory" },
{ "Benzothiazole",       "95-16-9",     3239,  10.0f,   0.5f,   5.0f, 135.19f,  300.0f, 3.0f, 5.5f, "rubber nutty meaty",         "RT",   0, 0, 0.30f,  "rubber nutty meaty sulfurous",     0.004f,   "savory|meat" },

}; /* end compounds[] */

static const int NCOMPOUNDS = 238;

#endif /* COMPOUND_DATA_H */
