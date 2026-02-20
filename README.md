# Soda Formulator - Visual Studio 2026 Project

## What's Included

This is a complete Visual Studio 2026 C project for managing soda formulations with version control.

**Files:**
- `SodaFormulator.sln` - Visual Studio Solution file (double-click this to open)
- `SodaFormulator.vcxproj` - Project configuration
- `main.c` - Main program with demo code
- `version.c/h` - Version control system
- `formulation.c/h` - Formulation management system

## How to Open and Run

1. **Extract this folder** to your desired location (e.g., Documents\Projects\)

2. **Double-click `SodaFormulator.sln`**
   - This will open the project in Visual Studio 2026

3. **Build the project:**
   - Press `Ctrl+Shift+B` or go to Build → Build Solution

4. **Run the program:**
   - Press `Ctrl+F5` or go to Debug → Start Without Debugging

## What It Does

The program demonstrates:
- Creating cinnamon roll and cherry cream soda formulations
- Tracking chemical compounds with concentrations in ppm
- Managing pH and Brix targets
- Version control (1.0.0 → 1.1.0 → 2.0.0)

## Expected Output

You should see:
```
========================================
  Soda Formulation Manager v0.1.0
========================================

=== Cinnamon Roll (CINROLL) ===
Version: 1.0.0
Target pH: 3.2
Target Brix: 12.0

Compounds (5):
  - Cinnamaldehyde    : 30.0 ppm
  - Vanillin          : 50.0 ppm
  - Diacetyl          : 2.0 ppm
  - Eugenol           : 5.0 ppm
  - Ethyl maltol      : 15.0 ppm

=== Cherry Cream (CHERRYCREAM) ===
...
```

## Project Configuration

The project is already configured to:
- Compile as C code (not C++)
- Use Debug and Release configurations
- Support x86 and x64 platforms
- Include all necessary compiler flags

## Next Steps

Once this works, we can add:
1. SQLite database integration for saving formulations
2. Compound database with safety limits
3. Tasting session tracking
4. Batch calculations
5. Cost analysis

## Troubleshooting

**If build fails:**
- Make sure Visual Studio 2026 is installed with "Desktop development with C++" workload
- Check that all 5 source files are present in the folder

**If you see C++ errors:**
- The project is already configured to compile as C
- Check Project Properties → C/C++ → Advanced → Compile As = "Compile as C Code (/TC)"

## Questions?

Let me know if you need help with:
- Adding new features
- Database integration
- Understanding any part of the code
- Modifying the formulations
