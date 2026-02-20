# Soda Formulation Manager - Project Context

## Business Overview

**Owner:** Noneya (developing startup soda business in California)

**Business Model:**
- Innovative flavors with exclusive limited edition releases
- Focus on unique, artisanal craft soda market
- Starting locally in Fresno, California
- Using licensed commercial kitchen for production
- Working with individual chemical compounds for molecular-level flavor creation

**Target Market:**
- Craft beverage enthusiasts
- Limited edition collectors
- Customers seeking innovative flavor experiences

## Current Project Status

**Initial Flavors (Limited Edition):**
1. **Cinnamon Roll** (CINROLL)
   - Cinnamon spice layer (cinnamaldehyde, eugenol, coumarin)
   - Baked dough notes (diacetyl, furaneol, maltol)
   - Brown sugar/caramel (sotolon, cyclotene)
   - Optional cream cheese frosting layer (delta-decalactone, butyric acid)

2. **Cherry Cream** (CHERRYCREAM)
   - Cherry character (benzaldehyde, ethyl cinnamate, benzyl acetate)
   - Cream component (vanillin, diacetyl, delta-decalactone, gamma-undecalactone)

## Technical Approach

### Formulation Method
- Creating flavors from **individual aroma compounds** (not flavor extracts)
- Working at **parts-per-million (ppm) precision**
- Professional flavorist approach
- Molecular-level flavor design

### Key Chemical Compounds Being Used

**Cinnamon Roll Flavor:**
- Cinnamaldehyde: 20-50 ppm (dominant cinnamon)
- Eugenol: 2-10 ppm (clove-like warmth)
- Vanillin: 20-80 ppm (sweet vanilla base)
- Diacetyl: 0.5-3 ppm (buttery richness - VERY potent)
- Ethyl maltol: 5-30 ppm (cotton candy sweetness)
- Sotolon: 0.01-0.1 ppm (maple/caramel - EXTREMELY potent)
- Furaneol: 0.1-1 ppm (caramelized sweetness)

**Cherry Cream Flavor:**
- Benzaldehyde: 20-100 ppm (maraschino cherry)
- Vanillin: 30-100 ppm (vanilla cream)
- Diacetyl: 1-5 ppm (butter cream)
- Delta-decalactone: 1-10 ppm (creamy, peachy)
- Ethyl cinnamate: 1-5 ppm (fruity complexity)
- Benzyl acetate: 2-10 ppm (sweet fruity depth)

### Critical Safety Notes
- Many compounds have **HARD REGULATORY LIMITS** (FEMA GRAS, FDA)
- Some compounds are **EXTREMELY POTENT** (work at ppb/ppm levels)
- **Dangerous concentration examples:**
  - Diacetyl >5 ppm: Overwhelming, artificial
  - Sotolon >0.5 ppm: Burnt, unpleasant
  - Butyric acid >1 ppm: Vomit character
- Software MUST validate against safe limits

### Target Parameters
- **pH:** 3.0-3.5 (typical soda range)
- **Brix:** 12-13 (sugar content)
- **Carbonation:** ~4 volumes CO2

## Software Requirements

### Current Implementation (v0.1.0)
- ✅ Semantic versioning (MAJOR.MINOR.PATCH)
- ✅ Formulation structures
- ✅ Compound tracking with ppm concentrations
- ✅ Version incrementing
- ✅ Display/printing functions

### Version Control Rules
**MAJOR (X.0.0):**
- Fundamental reformulation
- Different compound profile
- Major flavor direction change
- Example: 1.5.2 → 2.0.0 (added cream cheese layer)

**MINOR (0.X.0):**
- Concentration adjustments >10%
- Adding/removing minor compounds
- Example: 1.2.3 → 1.3.0 (increased cinnamaldehyde 30→40 ppm)

**PATCH (0.0.X):**
- Small tweaks <10%
- pH/Brix adjustments
- Process changes
- Example: 1.3.0 → 1.3.1 (pH 3.2→3.3)

### Planned Features (Priority Order)

**Phase 1: Database Integration**
- SQLite for formulation persistence
- Save/load formulations
- Version history tracking
- Compound library database

**Phase 2: Compound Database**
- Individual compound properties:
  - CAS number, molecular weight
  - FEMA GRAS number, max use levels
  - Solubility data
  - Odor/taste descriptors
  - Detection thresholds
  - Safety limits
  - Storage requirements
- Validation against regulatory limits
- Solubility calculations
- Supplier tracking

**Phase 3: Tasting Session Tracking**
- Structured sensory evaluation
- Multiple tasters
- Scoring attributes (sweetness, sourness, etc.)
- Free-form notes
- Link tasting sessions to formulation versions
- Track what changes improved ratings

**Phase 4: Advanced Features**
- Batch scaling calculations
- Cost analysis per batch
- Ingredient inventory management
- Label generation (ingredient lists)
- Dilution calculations for concentrates
- pH prediction modeling

### Data Structures Needed

**Extended Compound Properties:**
```c
typedef struct {
    char cas_number[16];
    char compound_name[128];
    int fema_number;
    float max_use_ppm;
    float recommended_min_ppm;
    float recommended_max_ppm;
    float water_solubility;      // mg/L
    float molecular_weight;
    float ph_stable_min;
    float ph_stable_max;
    char odor_profile[256];
    bool requires_solubilizer;
    char storage_temp[16];       // "RT", "refrigerate", "freezer"
    bool requires_inert_atmosphere;
} AromaChemical;
```

**Tasting Sessions:**
```c
typedef struct {
    char session_id[32];
    char experiment_id[32];
    char tasting_date[16];
    char taster_name[64];
    
    // Structured scores (0-10)
    int sweetness;
    int sourness;
    int bitterness;
    int aroma_intensity;
    int overall_rating;
    
    // Free-form notes
    char flavor_notes[512];
    char aroma_notes[512];
    
    bool would_purchase;
} TastingSession;
```

**Supplier Tracking:**
```c
typedef struct {
    char supplier_name[128];
    float min_order_qty;
    float price_per_unit;
    int lead_time_days;
} SupplierInfo;
```

## Chemistry Knowledge Base

### Solubility Issues
- Many aroma compounds are **oil-soluble, not water-soluble**
- May require solubilizers:
  - Polysorbate 80
  - Propylene glycol
  - Ethanol (small amounts)
- Software should calculate if compounds will stay in solution

### Flavor Interactions
- **Synergies:** Some compounds amplify each other
  - Vanillin + Ethyl maltol (very synergistic)
- **Masking:** High concentrations can mask delicate notes
  - Too much diacetyl masks fruit esters
- Software should eventually model these interactions

### Stability Considerations
- **pH sensitive:** Some compounds degrade in acidic conditions
- **Oxidation:** Citral, limonene oxidize over time
- **Light sensitive:** Many aromatics need UV protection
- **Temperature:** Some require refrigeration

### Sourcing
- **Key Suppliers:**
  - Sigma-Aldrich/MilliporeSigma (high purity, expensive)
  - Advanced Biotech (food-grade focus)
  - Vigon International (flavor chemicals)
  - Flavor & Fragrance Specialties
  - Jedwards International (bulk)
- Minimum orders typically 10-25g per compound
- Lead times: 2-4 weeks
- Storage: Amber bottles, cool/dark conditions

## Development Environment

**Platform:** Windows (Surface Book 3)
- Intel Core i7-1065G7
- 32GB RAM
- Visual Studio Community 2026

**Language:** C (not C++)
- Compiled with MSVC
- `/TC` flag (Compile as C Code)

**Database:** SQLite (planned)
- Single-file database
- No server required
- Good for desktop application

## Design Principles

1. **Precision:** Chemical concentrations must be exact (ppm level)
2. **Safety:** Always validate against regulatory limits
3. **Traceability:** Full version history and audit trail
4. **Flexibility:** Easy to add new compounds and formulations
5. **Professional:** This is real flavor chemistry, not hobby level

## Current File Structure
```
SodaFormulator/
├── main.c              - Demo program
├── version.h/c         - Version control system
├── formulation.h/c     - Formulation management
├── SodaFormulator.sln  - Visual Studio solution
└── SodaFormulator.vcxproj - Project file
```

## Next Steps

1. Add compound modification functions
2. Implement SQLite integration
3. Create compound database with safety limits
4. Add validation functions
5. Implement tasting session tracking
6. Add batch scaling calculations
7. Build cost analysis features

## Important Notes

- **This is professional work:** Creating flavors at molecular level
- **Regulatory compliance critical:** FDA, FEMA GRAS limits must be respected
- **Quality matters:** These formulations will be used in actual products
- **Precision required:** Working at parts-per-million (ppm) level
- **Safety first:** Some compounds toxic/dangerous at wrong concentrations

## Questions to Consider

- How to handle formulation branching? (testing multiple variants from same base)
- Best way to track which changes improved taste ratings?
- How to model compound interactions/synergies?
- Integration with commercial kitchen production schedules?
- Label generation for FDA compliance?
- Inventory management - when to reorder compounds?

---

**Last Updated:** February 19, 2026
**Project Status:** Foundation complete, ready for database integration
**Owner:** Noneya (California soda startup)
