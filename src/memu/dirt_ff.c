/*

dirt.c - Directory Traversal - Circle FatFS system

*/

/*...sincludes:0:*/

#ifdef __circle__
#include <circle/alloc.h>
#else
#include <stdlib.h>
#endif
#include <stdio.h>
#include <fatfs/ff.h>
#define	DIRT_C
#include "dirt.h"
#define NULL    0

/*...vdirt\46\h:0:*/
/*...e*/

typedef struct
	{
	DIR dir;
	} DIRT;

/*...sdirt_open:0:*/
DIRT *dirt_open(const char *dirname, int *rc)
	{
    printf ("dirt_open (%s)\n", dirname);
	DIRT *dirt;
	*rc = DIRTE_OK;
	if ( ( dirt = (DIRT *) malloc (sizeof (DIRT)) ) == NULL )
		{
        printf ("No memory\n");
		*rc = DIRTE_NO_MEMORY;
		return NULL;
		}
    FRESULT fr = f_opendir (&(dirt->dir), dirname);
	if ( fr != FR_OK )
		{
		switch ( fr )
			{
			case FR_NOT_ENOUGH_CORE:
                printf ("No memory\n");
                *rc = DIRTE_NO_MEMORY;
                break;
			case FR_NO_PATH:
                printf ("No path\n");
                *rc = DIRTE_NOT_FOUND;
                break;
			case FR_INVALID_NAME:
            case FR_INVALID_OBJECT:
                printf ("Invalid name\n");
                *rc = DIRTE_NOT_DIRECTORY;
                break;
			case FR_DENIED:
                printf ("Denied\n");
	            *rc = DIRTE_NO_ACCESS;
                break;
			default:
                printf ("General error %d\n", fr);
                *rc = DIRTE_GEN_ERROR;
                break;
			}
		free (dirt);
		return NULL;
		}
    printf ("Directory opened\n");
	return dirt;
	}
/*...e*/
/*...sdirt_next:0:*/
const char *dirt_next (DIRT *dirt)
	{
	static FILINFO fi;
    printf ("dirt_next\n");
    FRESULT fr = f_readdir (&(dirt->dir), &fi);
	if ( ( fr != FR_OK ) || ( fi.fname[0] == '\0' ) )
        {
        printf ("Not found\n");
		return NULL;
        }
    printf ("Found: %s\n", fi.fname);
	return fi.fname;
	}
/*...e*/
/*...sdirt_close:0:*/
void dirt_close (DIRT *dirt)
	{
    printf ("dirt_close\n");
	f_closedir (&(dirt->dir));
	free(dirt);
	}
/*...e*/

/*...sdirt_error:0:*/
const char *dirt_error(int rc)
	{
	const char *errlist[] =
		{
		NULL,
		"general error",
		"out of memory",
		"not found",
		"not a directory",
		"access denied",
		"out of resource",
		};
	return errlist[rc];
	}
/*...e*/
