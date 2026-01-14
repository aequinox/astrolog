# Astrolog 7.80

This is a modified version of Astrolog 7.80 (official release: June 20, 2025).

**Official website**: http://www.astrolog.org/astrolog.htm
**Original source**: http://www.astrolog.org/ftp/ast78src.zip
**Version 7.80 changes**: http://www.astrolog.org/ftp/updat780.htm

## Building

```bash
make              # Compile and link
make clean        # Remove object files and binary
./astrolog        # Run the program
./astrolog -h     # Display help
```

### Requirements
- C compiler (gcc/clang)
- X11 development libraries (optional, for graphics)
- Make

### Platform Notes
- **Unix/Linux**: Use the provided Makefile
- **Windows**: Use Visual Studio project files (`*.vcxproj`, `*.sln`)
- **X11**: Remove `-lX11` from Makefile LIBS if not using X windows

## Custom Features

This version includes the following enhancements beyond the official 7.80 release:

### Expanded Atlas
- `atlas.as` expanded with ~204k additional geographic locations

### Interpretation Styles
- `dane_rudhyar.ais` - Humanistic astrology interpretation style based on Dane Rudhyar's work
- `liz_greene.ais` - Psychological (Jungian) astrology interpretation style based on Liz Greene's work
- `interpret.cpp` - Enhanced interpretation module

## License

GNU General Public License (GPL) v2. See `license.htm` for details.

## Copyright

- Core program: (C) 1991-2025 Walter D. Pullen
- Swiss Ephemeris: (C) 1997-2008 Astrodienst AG
- Atlas data: Geonames.org (CC BY 4.0)
