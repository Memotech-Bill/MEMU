//  dirmap.h - Support for multiple roots for path names

#ifndef H_DIRMAP
#define H_DIRMAP

#ifdef MAP_PATH
#include "types.h"

typedef enum {pmapNone = -1, pmapWork, pmapCfg, pmapExe, pmapHome, pmapCount} PMapMode;

void PMapRootDir (PMapMode pmap, const char *psDir, BOOLEAN bCopy);
const char *PMapPath (const char *psPath);
const char *PMapMapped (const char *psPath);

#else
#define PMapRootDir(x, y, z)
#define PMapPath(x)     x
#define PMapMapped(x)   x

#endif
#endif
