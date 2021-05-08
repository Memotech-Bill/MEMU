/*

dirt.c - Directory Traversal - UNIX version

*/

/*...sincludes:0:*/
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>

#define	DIRT_C
#include "dirt.h"

/*...vdirt\46\h:0:*/
/*...e*/

typedef struct
	{
	DIR *dir;
	} DIRT;

/*...sdirt_open:0:*/
DIRT *dirt_open(const char *dirname, int *rc)
	{
	DIRT *dirt;
	*rc = DIRTE_OK;
	if ( (dirt = (DIRT *) malloc(sizeof(DIRT))) == NULL )
		{
		*rc = DIRTE_NO_MEMORY;
		return NULL;
		}
	if ( (dirt->dir = opendir(dirname)) == NULL )
		{
		switch ( errno )
			{
			case ENOMEM:	*rc = DIRTE_NO_MEMORY;		break;
			case ENOENT:	*rc = DIRTE_NOT_FOUND;		break;
			case ENOTDIR:	*rc = DIRTE_NOT_DIRECTORY;	break;
			case EACCES:	*rc = DIRTE_NO_ACCESS;		break;
			case EMFILE:
			case ENFILE:	*rc = DIRTE_NO_RESOURCE;	break;
			default:	*rc = DIRTE_GEN_ERROR;		break;
			}
		free(dirt);
		return NULL;
		}
	return dirt;
	}
/*...e*/
/*...sdirt_next:0:*/
const char *dirt_next(DIRT *dirt)
	{
	struct dirent *de;
	if ( (de = readdir(dirt->dir)) == NULL )
		return NULL;
	return de->d_name;
	}
/*...e*/
/*...sdirt_close:0:*/
void dirt_close(DIRT *dirt)
	{
	closedir(dirt->dir);
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
