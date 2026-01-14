/*
** Astrolog (Version 7.80) File: interpret.cpp
**
** Custom Interpretation Style Management
**
** IMPORTANT NOTICE: Astrolog and all chart display routines and anything
** not enumerated below used in this program are Copyright (C) 1991-2025 by
** Walter D. Pullen (Astara@msn.com, http://www.astrolog.org/astrolog.htm).
** Permission is granted to freely use, modify, and distribute these
** routines provided these credits and notices remain unmodified with any
** altered or distributed versions of the program.
*/

#include "astrolog.h"
#include <cstring>

#define COMBO_ALLOC 64  // Initial allocation for combo array


/*
******************************************************************************
** Interpretation File Parsing
******************************************************************************
*/

// Parse a line, removing comments and trimming whitespace
// Returns fTrue if line has content, fFalse if empty or comment only
static flag ParseLine(char *sz, char **pchKey, char **pchValue)
{
  char *pch;

  // Remove comments (everything after # or ;)
  for (pch = sz; *pch; pch++) {
    if (*pch == '#' || *pch == ';') {
      *pch = chNull;
      break;
    }
  }

  // Trim leading whitespace
  *pchKey = sz;
  while (**pchKey == ' ' || **pchKey == '\t' || **pchKey == '\r' || **pchKey == '\n')
    (*pchKey)++;

  // Check for empty line or section header
  if (**pchKey == chNull || **pchKey == '[')
    return fFalse;

  // Find separator (:) manually
  for (*pchValue = *pchKey; **pchValue; (*pchValue)++) {
    if (**pchValue == ':')
      break;
  }
  if (**pchValue != ':')
    return fFalse;

  **pchValue = chNull;
  (*pchValue)++;

  // Trim trailing whitespace from key
  pch = *pchValue - 2;
  while (pch >= *pchKey && (*pch == ' ' || *pch == '\t' || *pch == '\r'))
    *pch-- = chNull;

  // Trim leading whitespace from value
  while (**pchValue == ' ' || **pchValue == '\t' || **pchValue == '\r')
    (*pchValue)++;

  // Check for empty value
  if (**pchValue == chNull)
    return fFalse;

  // Trim trailing whitespace and continuation char from value
  pch = *pchValue + CchSz(*pchValue) - 1;
  while (pch >= *pchValue && (*pch == ' ' || *pch == '\t' || *pch == '\r' || *pch == '\n' ||
                          *pch == '\\'))
    *pch-- = chNull;

  return fTrue;
}


// Parse combo key into components (e.g., "Sun+Aries+1" -> 0, 1, 1)
// Returns fTrue if valid, fFalse otherwise
static flag FParseComboKey(CONST char *szKey, int *piObj, int *piSign, int *piHouse)
{
  char szBuf[cchSzMax];
  CONST char *pchSrc;
  char *pch;
  char *sz;  // Working pointer for current position
  int i;

  // Copy key to local buffer
  for (pch = szBuf, pchSrc = szKey; *pchSrc && (pch - szBuf) < cchSzMax - 1; )
    *pch++ = *pchSrc++;
  *pch = chNull;

  // Parse planet (number or name) - find first '+'
  for (pch = szBuf; *pch && *pch != '+'; pch++)
    ;
  if (*pch != '+')
    return fFalse;
  *pch = chNull;

  // Check if numeric or named
  if (szBuf[0] >= '0' && szBuf[0] <= '9') {
    *piObj = NFromSz(szBuf);
  } else {
    // Map planet names to numbers
    for (i = 0; i < objMax; i++) {
      if (FEqSzI(szBuf, szObjName[i])) {
        *piObj = i;
        break;
      }
    }
    if (i >= objMax)
      return fFalse;
  }

  if (!FValidObj(*piObj) && *piObj != -1)  // -1 is wildcard
    return fFalse;

  // Parse sign - find second '+'
  sz = pch + 1;  // sz now points to after first '+'
  for (pch = sz; *pch && *pch != '+'; pch++)
    ;
  if (*pch != '+')
    return fFalse;
  *pch = chNull;

  if (sz[0] == '*') {
    *piSign = -1;  // Wildcard
  } else if (sz[0] >= '0' && sz[0] <= '9') {
    *piSign = NFromSz(sz);
    if (!FValidSign(*piSign))
      return fFalse;
  } else {
    // Map sign names to numbers
    for (i = 1; i <= cSign; i++) {
      if (FEqSzI(sz, szSignName[i])) {
        *piSign = i;
        break;
      }
    }
    if (i > cSign)
      return fFalse;
  }

  // Parse house (the rest after the second '+')
  sz = pch + 1;  // sz now points to after second '+'
  if (sz[0] == '*') {
    *piHouse = -1;  // Wildcard
  } else {
    *piHouse = NFromSz(sz);
    if (!FBetween(*piHouse, 1, 12))  // Houses are 1-12
      return fFalse;
  }

  return fTrue;
}


// Parse aspect key into components (e.g., "conjunction" -> 1)
static int NParseAspectKey(CONST char *szKey)
{
  int i;

  // Try numeric
  if (szKey[0] >= '0' && szKey[0] <= '9') {
    i = NFromSz(szKey);
    if (i >= 0 && i <= cAspect)
      return i;
  }

  // Try aspect names
  for (i = 1; i <= cAspect; i++) {
    if (FEqSzI(szKey, szAspectName[i]))
      return i;
  }

  return -1;
}


// Parse aspect combination key (e.g., "0+4+1" -> Sun+Venus+Conjunct)
// Returns fTrue if valid, fFalse otherwise
static flag FParseAspectComboKey(CONST char *szKey, int *piObj1, int *piObj2, int *piAsp)
{
  char szBuf[cchSzMax];
  CONST char *pchSrc;
  char *pch;
  char *sz;
  int i;

  // Copy key to local buffer
  for (pch = szBuf, pchSrc = szKey; *pchSrc && (pch - szBuf) < cchSzMax - 1; )
    *pch++ = *pchSrc++;
  *pch = chNull;

  // Parse first planet (number or name) - find first '+'
  for (pch = szBuf; *pch && *pch != '+'; pch++)
    ;
  if (*pch != '+')
    return fFalse;
  *pch = chNull;

  // Check if numeric or named
  if (szBuf[0] >= '0' && szBuf[0] <= '9') {
    *piObj1 = NFromSz(szBuf);
  } else {
    // Map planet names to numbers
    for (i = 0; i < objMax; i++) {
      if (FEqSzI(szBuf, szObjName[i])) {
        *piObj1 = i;
        break;
      }
    }
    if (i >= objMax)
      return fFalse;
  }

  if (!FValidObj(*piObj1) && *piObj1 != -1)  // -1 is wildcard
    return fFalse;

  // Parse second planet - find second '+'
  sz = pch + 1;
  for (pch = sz; *pch && *pch != '+'; pch++)
    ;
  if (*pch != '+')
    return fFalse;
  *pch = chNull;

  if (sz[0] == '*') {
    *piObj2 = -1;  // Wildcard
  } else if (sz[0] >= '0' && sz[0] <= '9') {
    *piObj2 = NFromSz(sz);
  } else {
    for (i = 0; i < objMax; i++) {
      if (FEqSzI(sz, szObjName[i])) {
        *piObj2 = i;
        break;
      }
    }
    if (i >= objMax)
      return fFalse;
  }

  if (!FValidObj(*piObj2) && *piObj2 != -1)
    return fFalse;

  // Parse aspect
  sz = pch + 1;
  i = NParseAspectKey(sz);
  if (i < 0)
    return fFalse;
  *piAsp = i;

  return fTrue;
}


// Free an interpretation style structure
void FreeInterpretationStyle(InterpretationStyle *style)
{
  int i;

  if (style == NULL)
    return;

  if (style->filename != NULL)
    DeallocateP(style->filename);
  if (style->name != NULL)
    DeallocateP(style->name);
  if (style->author != NULL)
    DeallocateP(style->author);
  if (style->version != NULL)
    DeallocateP(style->version);

  // Free arrays
  for (i = 0; i < objMax; i++)
    if (style->planetMeaning[i] != NULL)
      DeallocateP(style->planetMeaning[i]);

  for (i = 0; i <= cSign; i++) {
    if (style->signDesc[i] != NULL)
      DeallocateP(style->signDesc[i]);
    if (style->signDesire[i] != NULL)
      DeallocateP(style->signDesire[i]);
    if (style->houseArea[i] != NULL)
      DeallocateP(style->houseArea[i]);
  }

  for (i = 0; i <= cAspect; i++) {
    if (style->aspectInteract[i] != NULL)
      DeallocateP(style->aspectInteract[i]);
    if (style->aspectTherefore[i] != NULL)
      DeallocateP(style->aspectTherefore[i]);
  }

  // Free aspect combos
  if (style->aspectCombos != NULL) {
    for (i = 0; i < style->aspectComboCount; i++) {
      if (style->aspectCombos[i].key != NULL)
        DeallocateP(style->aspectCombos[i].key);
      if (style->aspectCombos[i].value != NULL)
        DeallocateP(style->aspectCombos[i].value);
    }
    DeallocateP(style->aspectCombos);
  }

  // Free combos
  if (style->combos != NULL) {
    for (i = 0; i < style->comboCount; i++) {
      if (style->combos[i].key != NULL)
        DeallocateP(style->combos[i].key);
      if (style->combos[i].value != NULL)
        DeallocateP(style->combos[i].value);
    }
    DeallocateP(style->combos);
  }

  if (style->defaultLocation != NULL)
    DeallocateP(style->defaultLocation);
  if (style->defaultAspect != NULL)
    DeallocateP(style->defaultAspect);

  DeallocateP(style);
}


// Load an interpretation style from file
flag FLoadInterpretationStyle(CONST char *szFile)
{
  InterpretationStyle *style = NULL;
  FILE *file = NULL;
  char szLine[cchSzLine], *pchKey, *pchValue, sz[cchSzMax];
  int iSection = 0, i, obj, sign, house;
  flag fRet = fFalse;
  flag fContinuation = fFalse;
  char *szContinuation = NULL;
  char *szContinuationKey = NULL;  // Save the key for continuation lines

  file = FileOpen((char *)szFile, 0, NULL);
  if (file == NULL) {
    sprintf(szLine, "Could not open interpretation file: %s", szFile);
    PrintError(szLine);
    return fFalse;
  }

  // Allocate style structure
  style = (InterpretationStyle *)PAllocateCore(sizeof(InterpretationStyle));
  if (style == NULL)
    goto LDone;
  ClearB((pbyte)style, sizeof(InterpretationStyle));
  style->filename = SzClone((char *)szFile);

  // Initialize arrays to NULL
  for (i = 0; i < objMax; i++)
    style->planetMeaning[i] = NULL;
  for (i = 0; i <= cSign; i++) {
    style->signDesc[i] = NULL;
    style->signDesire[i] = NULL;
    style->houseArea[i] = NULL;
  }
  for (i = 0; i <= cAspect; i++) {
    style->aspectInteract[i] = NULL;
    style->aspectTherefore[i] = NULL;
  }

  // Parse file line by line
  while (!feof(file)) {
    if (fgets(szLine, cchSzLine, file) == NULL)
      break;

    // Handle continuation from previous line
    if (fContinuation && szContinuation != NULL) {
      i = CchSz(szLine);
      // Trim leading whitespace on continuation line
      pchKey = szLine;
      while (*pchKey == ' ' || *pchKey == '\t')
        pchKey++;

      // Check if this continuation line also ends with continuation character
      i = CchSz(pchKey) - 1;
      while (i >= 0 && (pchKey[i] == '\n' || pchKey[i] == '\r'))
        i--;
      while (i >= 0 && (pchKey[i] == ' ' || pchKey[i] == '\t'))
        i--;
      fContinuation = (i >= 0 && pchKey[i] == '\\');
      if (fContinuation)
        pchKey[i] = chNull;

      // Trim trailing space from accumulated value
      i = CchSz(szContinuation) - 1;
      while (i >= 0 && (szContinuation[i] == ' ' || szContinuation[i] == '\t'))
        szContinuation[i--] = chNull;

      // Append to continuation buffer with a single space separator
      i = CchSz(szContinuation);
      char *szNewBuf = (char *)PAllocateCore(i + CchSz(pchKey) + 2);
      if (szNewBuf == NULL)
        goto LDone;
      sprintf(szNewBuf, "%s %s", szContinuation, pchKey);
      szContinuation = szNewBuf;

      if (!fContinuation) {
        // Continuation complete - process the accumulated value
        pchValue = szContinuation;
        pchKey = szContinuationKey;
        goto LProcessValue;
      }
      continue;
    }

    // Remove newline and carriage return
    i = CchSz(szLine) - 1;
    while (i >= 0 && (szLine[i] == '\n' || szLine[i] == '\r'))
      szLine[i--] = chNull;

    // Check for continuation character in the raw line
    i = CchSz(szLine) - 1;
    while (i >= 0 && (szLine[i] == ' ' || szLine[i] == '\t'))
      i--;
    fContinuation = (i >= 0 && szLine[i] == '\\');
    if (fContinuation) {
      szLine[i] = chNull;  // Remove backslash
    }

    // Trim leading whitespace to check for section header
    pchKey = szLine;
    while (*pchKey == ' ' || *pchKey == '\t' || *pchKey == '\r' || *pchKey == '\n')
      pchKey++;

    // Check for section header BEFORE calling ParseLine
    if (pchKey[0] == '[') {
      if (FEqSzI(pchKey, "[metadata]"))
        iSection = 1;
      else if (FEqSzI(pchKey, "[planet_meanings]"))
        iSection = 2;
      else if (FEqSzI(pchKey, "[sign_descriptions]"))
        iSection = 3;
      else if (FEqSzI(pchKey, "[house_areas]"))
        iSection = 4;
      else if (FEqSzI(pchKey, "[combinations]"))
        iSection = 5;
      else if (FEqSzI(pchKey, "[aspects]"))
        iSection = 6;
      else if (FEqSzI(pchKey, "[aspect_combinations]"))
        iSection = 8;
      else if (FEqSzI(pchKey, "[templates]"))
        iSection = 7;
      else
        iSection = 0;  // Unknown section
      continue;
    }

    // Skip empty lines and non-section content
    if (!ParseLine(szLine, &pchKey, &pchValue))
      continue;

    // Handle continuation - if line ended with backslash, save key and value for next iteration
    if (fContinuation) {
      // Save the key and value (without the backslash which was already removed)
      szContinuation = SzClone(pchValue);
      szContinuationKey = SzClone(pchKey);
      continue;
    }

LProcessValue:
    // Process based on current section
    switch (iSection) {
    case 1:  // [metadata]
      if (FEqSzI(pchKey, "name"))
        style->name = SzClone(pchValue);
      else if (FEqSzI(pchKey, "author"))
        style->author = SzClone(pchValue);
      else if (FEqSzI(pchKey, "version"))
        style->version = SzClone(pchValue);
      else if (FEqSzI(pchKey, "description"))
        ;  // Skip for now
      break;

    case 2:  // [planet_meanings]
      obj = NParseSz(pchKey, pmObject);
      if (FValidObj(obj))
        style->planetMeaning[obj] = SzClone(pchValue);
      break;

    case 3:  // [sign_descriptions]
      // This handles both adjectives and desires - simplified for now
      sign = NParseSz(pchKey, pmSign);
      if (FValidSign(sign)) {
        // Store as signDesc for now
        style->signDesc[sign] = SzClone(pchValue);
        // Also use as desire for simplicity
        style->signDesire[sign] = SzClone(pchValue);
      }
      break;

    case 4:  // [house_areas]
      house = NFromSz(pchKey);
      if (FBetween(house, 1, 12))  // Houses are 1-12
        style->houseArea[house] = SzClone(pchValue);
      break;

    case 5:  // [combinations]
      if (FParseComboKey(pchKey, &obj, &sign, &house)) {
        // Expand combo array if needed
        if (style->comboCount >= style->comboAlloc) {
          InterpretationCombo *newCombos;
          style->comboAlloc += COMBO_ALLOC;
          newCombos = (InterpretationCombo *)PAllocateCore(
            sizeof(InterpretationCombo) * style->comboAlloc);
          if (newCombos == NULL)
            goto LDone;
          // Copy old combos to new array
          for (i = 0; i < style->comboCount; i++) {
            newCombos[i] = style->combos[i];
          }
          style->combos = newCombos;
        }
        style->combos[style->comboCount].key = SzClone(pchKey);
        style->combos[style->comboCount].value = SzClone(pchValue);
        style->comboCount++;
      }
      break;

    case 6:  // [aspects]
      i = NParseAspectKey(pchKey);
      if (i >= 0 && i <= cAspect)
        style->aspectInteract[i] = SzClone(pchValue);
      break;

    case 8:  // [aspect_combinations]
    {
      int obj1, obj2, asp;
      if (FParseAspectComboKey(pchKey, &obj1, &obj2, &asp)) {
        // Expand aspect combo array if needed
        if (style->aspectComboCount >= style->aspectComboAlloc) {
          InterpretationCombo *newCombos;
          style->aspectComboAlloc += COMBO_ALLOC;
          newCombos = (InterpretationCombo *)PAllocateCore(
            sizeof(InterpretationCombo) * style->aspectComboAlloc);
          if (newCombos == NULL)
            goto LDone;
          // Copy old combos to new array
          for (i = 0; i < style->aspectComboCount; i++) {
            newCombos[i] = style->aspectCombos[i];
          }
          style->aspectCombos = newCombos;
        }
        // Add new aspect combo
        style->aspectCombos[style->aspectComboCount].key = SzClone(pchKey);
        style->aspectCombos[style->aspectComboCount].value = SzClone(pchValue);
        style->aspectComboCount++;
      }
      break;
    }

    case 7:  // [templates]
      if (FEqSzI(pchKey, "default_location"))
        style->defaultLocation = SzClone(pchValue);
      else if (FEqSzI(pchKey, "default_aspect"))
        style->defaultAspect = SzClone(pchValue);
      break;
    }

    // Free continuation buffer after processing
    if (szContinuation != NULL) {
      DeallocateP(szContinuation);
      szContinuation = NULL;
    }
  }

  style->fLoaded = fTrue;
  im.style[im.styleCount] = style;
  fRet = fTrue;

LDone:
  if (file != NULL)
    fclose(file);
  if (szContinuation != NULL)
    DeallocateP(szContinuation);
  if (!fRet && style != NULL)
    FreeInterpretationStyle(style);
  return fRet;
}


/*
******************************************************************************
** Interpretation Lookup
******************************************************************************
*/

// Find combo interpretation with fallback logic
CONST char *SzGetComboInterpretation(int obj, int sign, int house)
{
  InterpretationStyle *style;
  char szKey[cchSzMax];
  int i;

  // Use custom style if loaded
  if (im.currentStyle < 0 || im.currentStyle >= im.styleCount)
    return NULL;
  style = im.style[im.currentStyle];
  if (style == NULL || !style->fLoaded)
    return NULL;

  // Try exact match first: "0+1+1" (Sun+Aries+1st)
  sprintf(szKey, "%d+%d+%d", obj, sign, house);
  for (i = 0; i < style->comboCount; i++) {
    if (style->combos[i].key != NULL && FEqSz(style->combos[i].key, szKey))
      return style->combos[i].value;
  }

  // Try planet+sign wildcard: "0+1+*" (Sun+Aries+*)
  sprintf(szKey, "%d+%d+*", obj, sign);
  for (i = 0; i < style->comboCount; i++) {
    if (style->combos[i].key != NULL && FEqSz(style->combos[i].key, szKey))
      return style->combos[i].value;
  }

  // Try planet+house wildcard: "0+*+1" (Sun+*+1st)
  sprintf(szKey, "%d+*+%d", obj, house);
  for (i = 0; i < style->comboCount; i++) {
    if (style->combos[i].key != NULL && FEqSz(style->combos[i].key, szKey))
      return style->combos[i].value;
  }

  // Try sign wildcard: "*+1+*" (Aries+*)
  sprintf(szKey, "*+%d+*", sign);
  for (i = 0; i < style->comboCount; i++) {
    if (style->combos[i].key != NULL && FEqSz(style->combos[i].key, szKey))
      return style->combos[i].value;
  }

  // Use default template if available
  if (style->defaultLocation != NULL)
    return style->defaultLocation;

  return NULL;
}


// Get aspect interpretation
CONST char *SzGetAspectInterpretation(int asp, int nOrb)
{
  InterpretationStyle *style;

  if (im.currentStyle < 0 || im.currentStyle >= im.styleCount)
    return NULL;
  style = im.style[im.currentStyle];
  if (style == NULL || !style->fLoaded)
    return NULL;

  if (asp < 0 || asp > cAspect)
    return NULL;

  return style->aspectInteract[asp];
}


// Get aspect combination interpretation (e.g., Sun conjunct Venus)
// Returns NULL if not found
CONST char *SzGetAspectComboInterpretation(int obj1, int obj2, int asp)
{
  InterpretationStyle *style;
  char szKey[cchSzMax];
  int i;

  // Use custom style if loaded
  if (im.currentStyle < 0 || im.currentStyle >= im.styleCount)
    return NULL;
  style = im.style[im.currentStyle];
  if (style == NULL || !style->fLoaded)
    return NULL;

  // Try exact match first: "0+4+1" (Sun+Venus+Conjunct)
  sprintf(szKey, "%d+%d+%d", obj1, obj2, asp);
  for (i = 0; i < style->aspectComboCount; i++) {
    if (style->aspectCombos[i].key != NULL && FEqSz(style->aspectCombos[i].key, szKey))
      return style->aspectCombos[i].value;
  }

  // Try planet1+planet2 wildcard: "0+4+*" (Sun+Venus+any aspect)
  sprintf(szKey, "%d+%d+*", obj1, obj2);
  for (i = 0; i < style->aspectComboCount; i++) {
    if (style->aspectCombos[i].key != NULL && FEqSz(style->aspectCombos[i].key, szKey))
      return style->aspectCombos[i].value;
  }

  // Try aspect wildcard: "*+*+1" (any planet+any planet+Conjunct)
  sprintf(szKey, "*+*+%d", asp);
  for (i = 0; i < style->aspectComboCount; i++) {
    if (style->aspectCombos[i].key != NULL && FEqSz(style->aspectCombos[i].key, szKey))
      return style->aspectCombos[i].value;
  }

  return NULL;
}


/*
******************************************************************************
** Folder-Based Interpretation Style Management
******************************************************************************
*/

// Initialize the interpretation folder system
// Sets up base path and scans for available styles
flag FInitInterpretationFolders(void)
{
  char *szHome;
#ifdef WIN
  // Windows: use USERPROFILE environment variable
  szHome = getenv("USERPROFILE");
  if (szHome == NULL)
    szHome = "C:\\";
  sprintf(ifm.basePath, "%s\\.astrolog\\interpretations", szHome);
#else
  // Unix/Linux/Mac: use HOME environment variable
  szHome = getenv("HOME");
  if (szHome == NULL)
    szHome = "/tmp";
  sprintf(ifm.basePath, "%s/.astrolog/interpretations", szHome);
#endif

  // Initialize folder array
  ifm.folderCount = 0;
  ifm.activeFolder = -1;

  // Scan for available styles
  return ScanInterpretationFolders() >= 0;
}


// Scan for available style folders in the interpretations directory
// Returns the number of style folders found, or -1 on error
int ScanInterpretationFolders(void)
{
  char szStylesPath[cchStylePath];
  DIR *dir;
  struct dirent *entry;
  struct stat statbuf;
  char szConfPath[cchStylePath];

  // Build path to styles directory
  sprintf(szStylesPath, "%s/styles", ifm.basePath);

  // Try to open the styles directory
  dir = opendir(szStylesPath);
  if (dir == NULL) {
    // Directory doesn't exist yet - not an error, just no styles
    return 0;
  }

  // Scan each subdirectory
  while ((entry = readdir(dir)) != NULL && ifm.folderCount < cMaxStyleFolder) {
    struct stat statEntry;

    // Skip . and ..
    if (entry->d_name[0] == '.')
      continue;

    // Build full path to entry
    sprintf(szConfPath, "%s/%s", szStylesPath, entry->d_name);

    // Skip symlinks (like "active")
    if (lstat(szConfPath, &statEntry) == 0 && S_ISLNK(statEntry.st_mode))
      continue;

    // Build full path to potential style folder
    sprintf(szConfPath, "%s/%s/style.conf", szStylesPath, entry->d_name);

    // Check if style.conf exists
    if (stat(szConfPath, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
      // This is a valid style folder - load its config
      InterpretationFolder *folder = &ifm.folder[ifm.folderCount];
      folder->name = SzClone(entry->d_name);
      sprintf(folder->path, "%s/%s", szStylesPath, entry->d_name);
      folder->fLoaded = fFalse;
      folder->fActive = fFalse;

      // Load style.conf metadata
      if (FLoadStyleConfig(folder, szConfPath)) {
        folder->fLoaded = fTrue;
        ifm.folderCount++;
      } else {
        // Failed to load config - clean up
        if (folder->name != NULL) {
          free(folder->name);
          folder->name = NULL;
        }
      }
    }
  }

  closedir(dir);

  // Check for "active" symlink or file
  char szActivePath[cchStylePath];
  sprintf(szActivePath, "%s/styles/active", ifm.basePath);
  char szActiveTarget[cchStylePath];
  int activeFound = fFalse;

#ifdef WIN
  // Windows: check for active.txt file
  sprintf(szActivePath, "%s/styles/active.txt", ifm.basePath);
  FILE *file = fopen(szActivePath, "r");
  if (file != NULL) {
    if (fgets(szActiveTarget, cchStylePath, file) != NULL) {
      // Remove newline
      char *nl = strchr(szActiveTarget, '\n');
      if (nl != NULL) *nl = '\0';
      nl = strchr(szActiveTarget, '\r');
      if (nl != NULL) *nl = '\0';
      activeFound = fTrue;
    }
    fclose(file);
  }
#else
  // Unix: read symlink
  ssize_t len = readlink(szActivePath, szActiveTarget, cchStylePath - 1);
  if (len > 0) {
    szActiveTarget[len] = '\0';
    // Get just the folder name from the symlink target
    char *slash = strrchr(szActiveTarget, '/');
    if (slash != NULL)
      strcpy(szActiveTarget, slash + 1);
    activeFound = fTrue;
  }
#endif

  if (activeFound) {
    // Find the folder with this name
    for (int i = 0; i < ifm.folderCount; i++) {
      if (FEqSzI(ifm.folder[i].name, szActiveTarget)) {
        ifm.activeFolder = i;
        ifm.folder[i].fActive = fTrue;
        sprintf(ifm.activePath, "%s/styles/%s", ifm.basePath, szActiveTarget);
        break;
      }
    }
  }

  return ifm.folderCount;
}


// Parse style.conf file into InterpretationFolder structure
flag FLoadStyleConfig(InterpretationFolder *folder, CONST char *szPath)
{
  FILE *file;
  char szLine[cchSzMax];
  char *pchKey, *pchValue;
  flag inMetadata = fFalse;

  file = fopen(szPath, "r");
  if (file == NULL)
    return fFalse;

  // Initialize fields to NULL
  folder->displayName = NULL;
  folder->author = NULL;
  folder->version = NULL;
  folder->description = NULL;

  while (fgets(szLine, cchSzMax, file) != NULL) {
    if (!ParseLine(szLine, &pchKey, &pchValue))
      continue;

    // Check for section headers
    if (szLine[0] == '[') {
      inMetadata = (FEqSzI(szLine, "[metadata]"));
      continue;
    }

    if (inMetadata) {
      if (FEqSzI(pchKey, "name")) {
        folder->displayName = SzClone(pchValue);
      } else if (FEqSzI(pchKey, "author")) {
        folder->author = SzClone(pchValue);
      } else if (FEqSzI(pchKey, "version")) {
        folder->version = SzClone(pchValue);
      } else if (FEqSzI(pchKey, "description")) {
        folder->description = SzClone(pchValue);
      }
    }
  }

  fclose(file);

  // Validate that we got the essential fields
  if (folder->displayName == NULL)
    folder->displayName = SzClone(folder->name);  // Use folder name as fallback

  return fTrue;
}


// Set active style by name and load all .ais files from that folder
// Returns fTrue on success, fFalse if style not found
flag FSetActiveStyle(CONST char *szName)
{
  int i, j;
  char szPath[cchStylePath];
  InterpretationStyle *style = NULL;

  // First ensure we've scanned for folders
  if (ifm.folderCount == 0 && !FInitInterpretationFolders()) {
    return fFalse;
  }

  // Search for style by name
  for (i = 0; i < ifm.folderCount; i++) {
    if (FEqSzI(ifm.folder[i].name, szName) ||
        FEqSzI(ifm.folder[i].displayName, szName)) {
      // Found it - set as active
      ifm.activeFolder = i;
      ifm.folder[i].fActive = fTrue;
      sprintf(ifm.activePath, "%s", ifm.folder[i].path);

      // Load all .ais files from this folder into an InterpretationStyle
      // First, allocate a new style slot
      if (im.styleCount >= cMaxStyle) {
        PrintError("Maximum interpretation styles loaded.");
        return fFalse;
      }

      // Allocate style structure
      style = (InterpretationStyle *)PAllocateCore(sizeof(InterpretationStyle));
      if (style == NULL)
        return fFalse;
      ClearB((pbyte)style, sizeof(InterpretationStyle));
      style->filename = SzClone(ifm.folder[i].displayName);

      // Initialize arrays to NULL
      for (j = 0; j < objMax; j++)
        style->planetMeaning[j] = NULL;
      for (j = 0; j <= cSign; j++) {
        style->signDesc[j] = NULL;
        style->signDesire[j] = NULL;
        style->houseArea[j] = NULL;
      }
      for (j = 0; j <= cAspect; j++) {
        style->aspectInteract[j] = NULL;
        style->aspectTherefore[j] = NULL;
      }

      // Load each planet's .ais file from signs/ directory
      struct stat statBuf;
      for (j = 0; j <= cPlanet; j++) {
        if (j <= 0 || j > cPlanet)
          continue;  // Skip invalid indices

        sprintf(szPath, "%s/signs/%s.ais", ifm.activePath, szObjName[j]);
        if (stat(szPath, &statBuf) == 0 && S_ISREG(statBuf.st_mode)) {
          // Load this planet's file and merge into style
          FLoadAISFileIntoStyle(style, szPath);
        }
      }

      style->fLoaded = fTrue;

      // Add to style array
      j = im.styleCount;
      im.style[j] = style;
      im.stylePath[j] = SzClone(ifm.folder[i].name);
      im.styleCount++;
      im.currentStyle = j;

      return fTrue;
    }
  }

  return fFalse;  // Style not found
}


// Get path to a specific .ais file in the active style
// szType: "signs", "aspects", "midpoints"
// iIndex: Planet number
// Returns NULL if not found or no active style
CONST char *SzGetAISPath(CONST char *szType, int iIndex)
{
  static char szPath[cchStylePath];

  if (ifm.activeFolder < 0 || ifm.activeFolder >= ifm.folderCount)
    return NULL;

  sprintf(szPath, "%s/%s/%s.ais", ifm.activePath, szType, szObjName[iIndex]);
  return szPath;
}


// List all available interpretation styles
void PrintInterpretationStyles(void)
{
  int i;

  PrintSz("Available Interpretation Styles:\n");

  if (ifm.folderCount == 0) {
    PrintSz("  No style folders found in ");
    PrintSz(ifm.basePath);
    PrintSz("/styles/\n");
    return;
  }

  for (i = 0; i < ifm.folderCount; i++) {
    InterpretationFolder *folder = &ifm.folder[i];

    // Mark active style with *
    if (folder->fActive)
      PrintSz(" * ");
    else
      PrintSz("   ");

    // Print style name
    PrintSz(folder->name);

    // Print display name if different
    if (folder->displayName != NULL && !FEqSz(folder->name, folder->displayName)) {
      PrintSz(" (");
      PrintSz(folder->displayName);
      PrintSz(")");
    }

    // Print author and version if available
    if (folder->author != NULL || folder->version != NULL) {
      PrintSz(" - ");
      if (folder->author != NULL) {
        PrintSz(folder->author);
        if (folder->version != NULL)
          PrintSz(" ");
      }
      if (folder->version != NULL) {
        PrintSz("v");
        PrintSz(folder->version);
      }
    }

    PrintSz("\n");

    // Print description
    if (folder->description != NULL) {
      PrintSz("     ");
      PrintSz(folder->description);
      PrintSz("\n");
    }
  }

  PrintSz("\n");
  PrintSz("Use -I <name> to select a style, -Id for default interpretations.\n");
}


// Load a specific .ais file from active style folder
// szType: "signs", "aspects", "midpoints"
// iIndex: Planet number
// Returns fTrue on success, fFalse on failure
flag FLoadAISFile(CONST char *szType, int iIndex)
{
  CONST char *szPath;

  // Must have an active style folder
  if (ifm.activeFolder < 0)
    return fFalse;

  // Get path to .ais file
  szPath = SzGetAISPath(szType, iIndex);
  if (szPath == NULL)
    return fFalse;

  // Load using existing function
  return FLoadInterpretationStyle(szPath);
}


// Load a .ais file and merge it into an existing InterpretationStyle structure
// This is used when loading folder-based styles with multiple .ais files
flag FLoadAISFileIntoStyle(InterpretationStyle *style, CONST char *szFile)
{
  FILE *file;
  char szLine[cchSzLine], *pchKey, *pchValue;
  int iSection = 0;
  int obj, sign, house, asp, i;
  flag fRet = fFalse;
  flag fContinuation = fFalse;
  char *szContinuation = NULL;
  char *szContinuationKey = NULL;

  if (style == NULL)
    return fFalse;

  file = fopen(szFile, "r");
  if (file == NULL)
    return fFalse;  // Silent fail - file might not exist

  // Parse file line by line
  while (!feof(file)) {
    if (fgets(szLine, cchSzLine, file) == NULL)
      break;

    // Skip comments and empty lines
    if (szLine[0] == '#' || szLine[0] == '\n' || szLine[0] == '\r')
      continue;

    // Check for section headers
    if (szLine[0] == '[') {
      char szSection[50];
      sscanf(szLine, "[%49[^]]", szSection);
      if (FEqSzI(szSection, "metadata")) {
        iSection = 1;
      } else if (FEqSzI(szSection, "planet_meanings")) {
        iSection = 2;
      } else if (FEqSzI(szSection, "sign_descriptions")) {
        iSection = 3;
      } else if (FEqSzI(szSection, "house_areas")) {
        iSection = 4;
      } else if (FEqSzI(szSection, "combinations")) {
        iSection = 5;
      } else if (FEqSzI(szSection, "aspect_combinations")) {
        iSection = 6;
      } else {
        iSection = 0;
      }
      continue;
    }

    // Handle continuation lines (BEFORE checking for colon)
    if (fContinuation && szContinuation != NULL && szContinuationKey != NULL) {
      // This is a continuation line - it doesn't have a colon
      // Trim the line and append to continuation buffer
      char *szContLine = szLine;
      while (*szContLine == ' ' || *szContLine == '\t')
        szContLine++;
      char *end = szContLine + CchSz(szContLine) - 1;
      while (end > szContLine && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        *end-- = '\0';

      // Check if this continuation line also continues
      if (CchSz(szContLine) > 0 && szContLine[CchSz(szContLine)-1] == '\\') {
        // Remove backslash and continue
        szContLine[CchSz(szContLine)-1] = '\0';
        // Append to buffer
        char *szNewBuf = (char *)PAllocateCore(CchSz(szContinuation) + CchSz(szContLine) + 2);
        if (szNewBuf != NULL) {
          sprintf(szNewBuf, "%s %s", szContinuation, szContLine);
          szContinuation = szNewBuf;
        }
        continue;
      } else {
        // Continuation complete - process the accumulated value
        char *szNewBuf = (char *)PAllocateCore(CchSz(szContinuation) + CchSz(szContLine) + 2);
        if (szNewBuf != NULL) {
          sprintf(szNewBuf, "%s %s", szContinuation, szContLine);
          szContinuation = szNewBuf;
        }
        pchValue = szContinuation;
        pchKey = szContinuationKey;
        fContinuation = fFalse;
        // Fall through to process below
      }
    } else {
      // Parse key:value pairs
      pchValue = strchr(szLine, ':');
      if (pchValue == NULL)
        continue;
      *pchValue = '\0';
      pchKey = szLine;
      pchValue++;

      // Trim whitespace
      while (*pchKey == ' ' || *pchKey == '\t')
        pchKey++;
      char *end = pchKey + CchSz(pchKey) - 1;
      while (end > pchKey && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        *end-- = '\0';

      while (*pchValue == ' ' || *pchValue == '\t')
        pchValue++;
      end = pchValue + CchSz(pchValue) - 1;
      while (end > pchValue && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        *end-- = '\0';

      // Check if this value starts a continuation
      if (CchSz(pchValue) > 0 && pchValue[CchSz(pchValue)-1] == '\\') {
        // Start continuation
        fContinuation = fTrue;
        pchValue[CchSz(pchValue)-1] = '\0';
        szContinuation = SzClone(pchValue);
        szContinuationKey = SzClone(pchKey);
        continue;
      }
    }

    // Store based on section
    switch (iSection) {
      case 2:  // planet_meanings
        obj = NFromSz(pchKey);
        if (obj >= 1 && obj < objMax)
          style->planetMeaning[obj] = SzClone(pchValue);
        break;

      case 3:  // sign_descriptions
        sign = NFromSz(pchKey);
        if (sign >= 1 && sign <= cSign)
          style->signDesc[sign] = SzClone(pchValue);
        break;

      case 4:  // house_areas
        house = NFromSz(pchKey);
        if (house >= 1 && house <= cSign)
          style->houseArea[house] = SzClone(pchValue);
        break;

      case 5:  // combinations (planet+sign+house)
        // Parse "obj+sign+house" key
        obj = sign = house = 0;
        sscanf(pchKey, "%d+%d+%d", &obj, &sign, &house);
        if (obj > 0 && sign > 0 && house > 0) {
          // Grow array if needed (max 2000 combinations)
          if (style->comboCount >= style->comboAlloc) {
            int newAlloc = style->comboAlloc + 500;
            if (newAlloc > 2000) newAlloc = 2000;
            if (style->comboCount >= newAlloc)
              break;  // Already at max
            InterpretationCombo *newCombos = (InterpretationCombo *)PAllocateCore(
              newAlloc * sizeof(InterpretationCombo));
            if (newCombos == NULL)
              break;
            // Copy existing data
            for (i = 0; i < style->comboCount; i++) {
              newCombos[i].key = style->combos[i].key;
              newCombos[i].value = style->combos[i].value;
            }
            style->combos = newCombos;
            style->comboAlloc = newAlloc;
          }
          style->combos[style->comboCount].key = SzClone(pchKey);
          style->combos[style->comboCount].value = SzClone(pchValue);
          style->comboCount++;
        }
        break;

      case 6:  // aspect_combinations
        // Parse "obj1+obj2+aspect" key
        obj = sign = asp = 0;
        sscanf(pchKey, "%d+%d+%d", &obj, &sign, &asp);
        if (obj > 0 && sign > 0) {
          // Grow array if needed (max 1000 aspect combinations)
          if (style->aspectComboCount >= style->aspectComboAlloc) {
            int newAlloc = style->aspectComboAlloc + 250;
            if (newAlloc > 1000) newAlloc = 1000;
            if (style->aspectComboCount >= newAlloc)
              break;  // Already at max
            InterpretationCombo *newCombos = (InterpretationCombo *)PAllocateCore(
              newAlloc * sizeof(InterpretationCombo));
            if (newCombos == NULL)
              break;
            // Copy existing data
            for (i = 0; i < style->aspectComboCount; i++) {
              newCombos[i].key = style->aspectCombos[i].key;
              newCombos[i].value = style->aspectCombos[i].value;
            }
            style->aspectCombos = newCombos;
            style->aspectComboAlloc = newAlloc;
          }
          style->aspectCombos[style->aspectComboCount].key = SzClone(pchKey);
          style->aspectCombos[style->aspectComboCount].value = SzClone(pchValue);
          style->aspectComboCount++;
        }
        break;
    }
  }

  fclose(file);
  return fTrue;
}


// Install a style package from tar.gz file
// This is a stub implementation - full extraction would be needed
flag FInstallStylePackage(CONST char *szPackage)
{
  PrintSz("Style package installation not yet implemented.\n");
  PrintSz("Package: ");
  PrintSz(szPackage);
  PrintSz("\n");
  PrintSz("To manually install:\n");
  PrintSz("  1. Extract the package to ~/.astrolog/interpretations/styles/\n");
  PrintSz("  2. Restart Astrolog or run -I list to refresh\n");
  return fFalse;
}


// Migrate existing .ais files to new folder structure
// This is a stub implementation - full migration logic would be needed
flag FMigrateOldAISFiles(void)
{
  PrintSz("Migration not yet implemented.\n");
  PrintSz("Existing .ais files will continue to work with -Is <file>.\n");
  return fFalse;
}
