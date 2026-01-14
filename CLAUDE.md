# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Astrolog** is a comprehensive astrology calculation software (version 7.80) that creates chart displays, calculates planetary positions, performs ephemeris calculations, and provides astrological interpretations. Originally created in 1991, it is a mature C++ codebase with C-style patterns.

- **License**: GNU GPL v2
- **Website**: http://www.astrolog.org/astrolog.htm
- **Author**: Walter D. Pullen

## Build Commands

```bash
make              # Compile and link
make clean        # Remove object files and binary
./astrolog        # Run the program
./astrolog -h     # Display help
```

The Makefile uses gcc/cc with X11 graphics support. Remove `-lX11` from LIBS if not using X windows.

## Architecture

### Program Flow

The main entry point (`main()` in `astrolog.cpp`) follows this sequence:

1. **`InitProgram()`** - Read configuration from `astrolog.as`
2. **`FProcessSwitchFile(DEFAULT_INFOFILE, ...)`** - Load default settings
3. **`NPromptSwitches()`** or **`FProcessSwitches()`** - Parse command line switches
4. **`Action()`** - Execute the requested chart/operation
5. **`Terminate()`** - Cleanup and exit

The program may loop back to `LBegin` if `us.fLoop` or `us.fNoQuit` flags are set.

### Global State Architecture

This codebase uses **heavy global state** defined in `extern.h`:

- `ciCore`, `ciMain` - Core chart info structures (with macros: `MM`, `DD`, `YY`, `TT`, `ZZ`, `SS`, `OO`, `AA`)
- `us`, `is` - User and internal settings structures
- `cp0` - Core calculation parameters
- `ignore`, `ignoreMem` - Object selection arrays
- `grid` - Aspect grid data

The `extern.h` file contains all global variable declarations and function prototypes—essentially the central "wiring" for the entire codebase.

### Key Source File Responsibilities

| File | Purpose |
|------|---------|
| `astrolog.cpp` | Entry point, command line parsing (`FProcessSwitches`, `NPromptSwitches`) |
| `general.cpp` | Math utilities, string functions, I/O, chart calculations (`ComputeEphem`, `CastChart`) |
| `charts*.cpp` | Chart display routines (wheel, grid, aspect, horizon, ephemeris, etc.) |
| `intrpret.cpp` / `interpret.cpp` | Astrological interpretation logic |
| `atlas.cpp` | Geographic location lookup from `atlas.as` database |
| `io.cpp` | File I/O and data export |
| `x*.cpp` | X11 graphics interface (Unix only) |
| `swe*.cpp` | Swiss Ephemeris library integration |
| `calc.cpp` | Core astronomical calculations |

### Configuration Files

- `astrolog.as` - User settings (loaded at runtime via `FProcessSwitchFile()`)
- `atlas.as` - Geographic location database (~204k entries)
- `timezone.as` - Time zone rules
- `*.ais` - Interpretation style files (INI-style format for different astrological traditions)
- `earth.bmp` - World map for astrological cartography

### Adding Command Line Switches

New switches require modifications in multiple places:

1. **`extern.h`** - Add switch character to appropriate arrays
2. **`general.cpp`**:
   - `NPromptSwitches()` - Add prompt text
   - `NProcessSwitchesRare()` or `FProcessSwitches()` - Add processing logic
3. **`astrolog.htm`** - Update documentation

## Code Style Conventions

### Hungarian Notation Prefixes

| Prefix | Meaning | Example |
|--------|---------|---------|
| `N` | Returns int | `NParseSz`, `NCompareSz` |
| `F` | Returns bool | `FProcessSwitches`, `FInputData` |
| `Sz` | Returns char* | `SzDate`, `SzClone` |
| `R` | Returns double | `RAngle`, `RFromSz` |
| `V` | Void function | `VAngle` |
| `K` | Color-related | `KvBlend`, `KvHue` |
| `C` | Count/size | `CchSz`, `CwchSz` |

### Variable Naming

- `rg` prefix = array/range (e.g., `rgobjList`, `rgAspConfig`)
- `sz` prefix = string (e.g., `szAppName`, `szObjName`)
- `f` prefix = boolean flag (e.g., `force`, `rgfProg`)
- `r` prefix = real/float (e.g., `rAspAngle`)
- `c` prefix = count (e.g., `cSatellite`)

### File Headers

All source files begin with a comprehensive copyright and license header block. Preserve this format when adding new files.

### Code Characteristics

- No namespaces (all code in global namespace)
- Standalone functions (not class methods)
- C-style arrays and pointers
- `#define` used for constants (not `const`)
- Minimal C++ features (essentially C with .cpp extension)

## Platform Configuration

Key `#define` settings in `astrolog.h`:

- `X11` - X Windows graphics support
- `WIN` - MS Windows platform
- `PC` - Generic PC platform
- `SWITCHES` - Command line parameter support
- `TIME` - Time function support

## Testing

This project has **no automated tests**. Verification is manual—run the binary and test the specific functionality being modified.
