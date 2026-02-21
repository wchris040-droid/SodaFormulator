#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compound.h"
#include "formulation.h"
#include "tasting.h"
#include "batch.h"
#include "version.h"
#include "database.h"

int main() {
    printf("========================================\n");
    printf("  Soda Formulation Manager v0.1.0\n");
    printf("========================================\n");
    
    // Create Cinnamon Roll formulation
    Formulation* cinroll = create_formulation("CINROLL", "Cinnamon Roll");
    
    add_compound(cinroll, "Cinnamaldehyde", 30.0);
    add_compound(cinroll, "Vanillin", 50.0);
    add_compound(cinroll, "Diacetyl", 2.0);
    add_compound(cinroll, "Eugenol", 5.0);
    add_compound(cinroll, "Ethyl maltol", 15.0);
    
    cinroll->target_ph = 3.2f;
    cinroll->target_brix = 12.0f;
    
    print_formulation(cinroll);
    
    // Create Cherry Cream formulation
    Formulation* cherry = create_formulation("CHERRYCREAM", "Cherry Cream");
    
    add_compound(cherry, "Benzaldehyde", 40.0);
    add_compound(cherry, "Vanillin", 60.0);
    add_compound(cherry, "Diacetyl", 3.0);
    add_compound(cherry, "Delta-decalactone", 5.0);
    add_compound(cherry, "Ethyl cinnamate", 2.0);
    
    cherry->target_ph = 3.3f;
    cherry->target_brix = 13.0f;
    
    print_formulation(cherry);
    
    // Test version incrementing
    printf("\n=== Version Control Demo ===\n");
    Version v = create_version(1, 0, 0);
    printf("Initial: ");
    print_version(v);
    printf("\n");
    
    v = increment_version(v, INCREMENT_MINOR);
    printf("After minor update: ");
    print_version(v);
    printf("\n");
    
    v = increment_version(v, INCREMENT_MAJOR);
    printf("After major update: ");
    print_version(v);
    printf("\n\n");
    
    // Database demo
    printf("=== Database Demo ===\n");

    if (db_open("formulations.db") == 0) {

        // Compound library demo
        printf("\n=== Compound Library Demo ===\n");
        db_seed_compound_library();
        db_list_compounds();

        {
            CompoundInfo info;
            printf("\n--- Cinnamaldehyde info card ---\n");
            if (db_get_compound_by_name("Cinnamaldehyde", &info) == 0)
                compound_print(&info);

            printf("\nValidating CINROLL v1.0.0 against library limits:\n");
            {
                int v = db_validate_formulation(cinroll);
                if (v == 0) printf("  All compounds within limits.\n");
            }
        }

        // Save initial versions (cinroll and cherry are both v1.0.0)
        db_save_formulation(cinroll);
        db_save_formulation(cherry);

        // Patch increment cinroll: minor tweak to pH (<10% change)
        cinroll->version = increment_version(cinroll->version, INCREMENT_PATCH);
        cinroll->target_ph = 3.3f;
        db_save_formulation(cinroll);

        // Minor increment cinroll: add Sotolon compound
        cinroll->version = increment_version(cinroll->version, INCREMENT_MINOR);
        add_compound(cinroll, "Sotolon", 0.3f);
        db_save_formulation(cinroll);

        // List all flavors at their latest version
        db_list_formulations();

        // Show all versions of CINROLL
        db_get_version_history("CINROLL");

        // Load CINROLL v1.0.0 from DB into a stack-allocated struct
        {
            Formulation loaded;
            printf("\n--- Loading CINROLL v1.0.0 from database ---\n");
            if (db_load_version("CINROLL", 1, 0, 0, &loaded) == 0)
                print_formulation(&loaded);

            printf("--- Loading latest CINROLL ---\n");
            if (db_load_latest("CINROLL", &loaded) == 0)
                print_formulation(&loaded);
        }

        // Tasting session demo
        printf("\n=== Tasting Session Demo ===\n");
        {
            TastingSession ts;

            // Session 1: CINROLL v1.0.0 — evaluator A
            tasting_create(&ts, 0, "NFide");
            ts.overall_score   = 7.5f;
            ts.aroma_score     = 8.0f;
            ts.flavor_score    = 7.0f;
            ts.mouthfeel_score = 7.5f;
            ts.finish_score    = 7.0f;
            ts.sweetness_score = 6.5f;
            strncpy(ts.notes,
                "Cinnamon presence strong on entry, vanillin rounds out mid-palate. "
                "Finish is slightly astringent. Reduce cinnamaldehyde 5 ppm next patch.",
                MAX_TASTING_NOTES - 1);
            db_save_tasting("CINROLL", 1, 0, 0, &ts);
            tasting_print(&ts);

            // Session 2: CINROLL v1.0.0 — evaluator B
            tasting_create(&ts, 0, "Panelist_02");
            ts.overall_score   = 8.0f;
            ts.aroma_score     = 8.5f;
            ts.flavor_score    = 7.5f;
            ts.mouthfeel_score = 8.0f;
            ts.finish_score    = 7.5f;
            ts.sweetness_score = 7.0f;
            strncpy(ts.notes,
                "Excellent bakery note. Diacetyl butter detectable but not over-threshold. "
                "Very balanced for a prototype.",
                MAX_TASTING_NOTES - 1);
            db_save_tasting("CINROLL", 1, 0, 0, &ts);

            // Session 3: CINROLL v1.1.0 — evaluator A (after sotolon addition)
            tasting_create(&ts, 0, "NFide");
            ts.overall_score   = 8.5f;
            ts.aroma_score     = 9.0f;
            ts.flavor_score    = 8.0f;
            ts.mouthfeel_score = 8.0f;
            ts.finish_score    = 8.5f;
            ts.sweetness_score = 7.0f;
            strncpy(ts.notes,
                "Sotolon addition lifts caramel depth without overwhelming. "
                "Finish is significantly improved. Recommend proceeding to batch scale.",
                MAX_TASTING_NOTES - 1);
            db_save_tasting("CINROLL", 1, 1, 0, &ts);

            // Session 4: CHERRYCREAM v1.0.0 — evaluator A
            tasting_create(&ts, 0, "NFide");
            ts.overall_score   = 8.0f;
            ts.aroma_score     = 8.5f;
            ts.flavor_score    = 8.0f;
            ts.mouthfeel_score = 7.5f;
            ts.finish_score    = 7.5f;
            ts.sweetness_score = 8.0f;
            strncpy(ts.notes,
                "Benzaldehyde cherry note clean and true. Delta-decalactone adds good "
                "creaminess. Ethyl cinnamate spice element is subtle but effective.",
                MAX_TASTING_NOTES - 1);
            db_save_tasting("CHERRYCREAM", 1, 0, 0, &ts);
        }

        // List all sessions + averages for CINROLL
        db_list_tastings_for_flavor("CINROLL");
        db_get_avg_scores("CINROLL");

        // Phase 4: Batch Scaling, Cost Analysis, Inventory
        printf("\n=== Phase 4: Batch Scaling Demo ===\n");

        db_seed_inventory();

        {
            // Load CINROLL v1.1.0 (latest) to batch against live formulation
            Formulation latest;
            BatchRun    br;

            memset(&br, 0, sizeof(br));

            if (db_load_latest("CINROLL", &latest) == 0) {
                // Calculate weights for 20 L batch
                batch_calculate(&br, &latest, 20.0f);

                // Fill cost data from compound library
                db_cost_batch(&br);

                // Check inventory before committing
                printf("\nInventory check for 20 L CINROLL batch:\n");
                db_check_inventory(&br);

                // Print weighing manifest
                batch_print_manifest(&br);

                // Save the batch (auto-generates batch number)
                db_save_batch("CINROLL",
                              latest.version.major,
                              latest.version.minor,
                              latest.version.patch,
                              &br);

                // Print production label
                batch_print_label(&br, &latest);

                // Deduct from inventory
                db_deduct_inventory(&br);
            }

            // Second batch: CHERRYCREAM v1.0.0, 10 L
            memset(&br, 0, sizeof(br));
            if (db_load_latest("CHERRYCREAM", &latest) == 0) {
                batch_calculate(&br, &latest, 10.0f);
                db_cost_batch(&br);
                db_save_batch("CHERRYCREAM",
                              latest.version.major,
                              latest.version.minor,
                              latest.version.patch,
                              &br);
                db_deduct_inventory(&br);
            }
        }

        // Show batch history and final inventory
        db_list_batches("CINROLL");
        db_list_inventory();

        db_close();
    }

    // Cleanup
    free_formulation(cinroll);
    free_formulation(cherry);
    
    printf("Press Enter to exit...");
    getchar();
    
    return 0;
}
