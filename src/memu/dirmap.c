//  dirmap.c - Support for multiple roots for path names

#ifdef MAP_PATH

#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "dirmap.h"

static const char *rootdir[pmapCount] = {NULL};
static char *psNewMap = NULL;
static int nNewLen = 0;

void PMapRootDir (PMapMode pmap, const char *psDir, BOOLEAN bCopy)
    {
    if ( rootdir[pmap] != NULL ) free ((void *)rootdir[pmap]);
    if ( bCopy ) rootdir[pmap] = estrdup (psDir);
    else rootdir[pmap] = psDir;
    }

static PMapMode PMapClass (const char *psPath)
    {
    if ( psPath == NULL )
        {
        return pmapNone;
        }
    if ( psPath[0] != '~' )
        {
        return pmapNone;
        }
    if ( ( psPath[1] == '/' ) || ( psPath[1] == '\\' ) || ( psPath[1] == '\0' ))
        {
        return pmapHome;
        }
    if ( ( psPath[2] == '/' ) || ( psPath[2] == '\\' ) || ( psPath[2] == '\0' ))
        {
        switch (psPath[1])
            {
            case 'C':
            case 'c':
                return pmapCfg;
            case 'E':
            case 'e':
                return pmapExe;
            case 'H':
            case 'h':
                return pmapHome;
            case 'W':
            case 'w':
                return pmapWork;
            default:
                return pmapNone;
            }
        }
    return pmapNone;
    }

const char *PMapMap (PMapMode pmap, const char *psPath)
    {
    if ( pmap == pmapNone ) return psPath;
    int nLen = strlen (rootdir[pmap]) + strlen (psPath);
    if ( psNewMap == NULL )
        {
        psNewMap = (char *) emalloc (nLen);
        nNewLen = nLen;
        }
    else if ( nNewLen < nLen )
        {
        free (psNewMap);
        psNewMap = (char *) emalloc (nLen);
        nNewLen = nLen;
        }
    strcpy (psNewMap, rootdir[pmap]);
    strcat (psNewMap, &psPath[pmap == pmapHome ? 1 : 2]);
    return psNewMap;
    }

const char *PMapPath (const char *psPath)
    {
    return PMapMap (PMapClass (psPath), psPath);
    }

#endif

#if 0
typedef struct s_mapping
    {
    struct s_mapping    *next;
    const char *        *psOrig;
    const char *        *psMapped;
    } PMapping;

static PMapping first[pmapCount] = {NULL};
static PMapping second[pmapCount] = {NULL};

static const char *PMapFind (PMapMode pmap, const char *psPath)
    {
    PMapping *map = first[pmap];
    while ( map != NULL )
        {
        if ( map->psOrig == psPath ) return map->psMapped;
        pmap = pmap->next;
        }
    map = second[pmap];
    while ( map != NULL )
        {
        if ( map->psOrig == psPath ) return map->psMapped;
        pmap = pmap->next;
        }
    return NULL;
    }

static const char *PMapSearch (PMapMode pmap, const char *psPath)
    {
    PMapping *map = first[pmap];
    while ( map != NULL )
        {
        if ( strcmp (map->psOrig, psPath) == 0 ) return map->psMapped;
        pmap = pmap->next;
        }
    return NULL;
    }

static PMapping *PMapAlloc (PMapping **base)
    {
    PMapping *map = (PMapping *) emalloc (sizeof (PMapping));
    map->next = *base;
    *base = map;
    }

const char *PMapPath (const char *psPath, BOOLEAN bKeep)
    {
    PMapMode pmap = PMapClass (psPath);
    if ( pmap == pmapNone )
        {
        return psPath;
        }
    const char *psMapped = PMapFind (pmap, psPath);
    if ( psMapped != NULL ) return psMapped;
    psMapped = PMapSearch (pmap, psPath);
    if ( psMapped != NULL )
        {
        if ( bKeep )
            {
            PMapping *map = PMapAlloc (&second[pmap]);
            map->psOrig = psPath;
            map->psMapped = psMapped;
            }
        return psMapped;
        }
    psMapped = PMapMap (pmap, psPath);
    if ( bKeep )
        {
        PMapping *map = PMapAlloc (&first[pmap]);
        map->psOrig = psPath;
        map->psMapped = psMapped;
        psNewMap = NULL;
        }
    return psMapped;
    }
#endif
