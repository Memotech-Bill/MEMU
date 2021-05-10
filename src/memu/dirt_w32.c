/*

dirt.c - Directory Traversal - Win32 version

*/

/*...sincludes:0:*/
#define	WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <stdlib.h>

#define	DIRT_C
#include "dirt.h"

/*...vdirt\46\h:0:*/
/*...e*/

typedef struct
	{
	HANDLE hff;
	WIN32_FIND_DATA fd;
	BOOL empty;
	BOOL first;
	} DIRT;

/*...sdirt_open:0:*/
DIRT *dirt_open(const char *dirname, int *rc)
	{
	DIRT *dirt;
	char pattern[MAX_PATH+2+1];
	int len = (int) strlen(dirname);
	*rc = DIRTE_OK;
	if ( len == 0 || len > MAX_PATH )
		{
		*rc = DIRTE_NOT_FOUND;
		return NULL;
		}
	if ( (dirt = (DIRT *) malloc(sizeof(DIRT))) == NULL )
		{
		*rc = DIRTE_NO_MEMORY;
		return NULL;
		}
	memcpy(pattern, dirname, len);
	if ( pattern[len-1] == '/' || pattern[len-1] == '\\' )
		strcpy(pattern+len-1, "\\*");
	else if ( len == 2 && isalpha(pattern[0]) && pattern[1] == ':' )
		strcpy(pattern+len, "*");
	else
		strcpy(pattern+len, "\\*");
	if ( (dirt->hff = FindFirstFile(pattern, &(dirt->fd))) == (HANDLE) -1 )
		{
		switch ( GetLastError() )
			{
			case ERROR_FILE_NOT_FOUND:
				dirt->empty = TRUE;
				return dirt;
			case ERROR_PATH_NOT_FOUND:
				*rc = DIRTE_NOT_FOUND;
				break;
			case ERROR_DIRECTORY:
				*rc = DIRTE_NOT_DIRECTORY;
				break;
			case ERROR_ACCESS_DENIED:
				*rc = DIRTE_NO_ACCESS;
				break;
			case ERROR_NOT_ENOUGH_MEMORY:
				*rc = DIRTE_NO_MEMORY;
				break;
			default:
				*rc = DIRTE_GEN_ERROR;
				break;
			}
		free(dirt);
		return NULL;
		}
	dirt->empty = FALSE;
	dirt->first = TRUE;
	return dirt;
	}
/*...e*/
/*...sdirt_next:0:*/
const char *dirt_next(DIRT *dirt)
	{
	if ( dirt->empty )
		return NULL;
	else if ( dirt->first )
		dirt->first = FALSE;
	else if ( ! FindNextFile(dirt->hff, &(dirt->fd)) )
		return NULL;
	return dirt->fd.cFileName;
	}
/*...e*/
/*...sdirt_close:0:*/
void dirt_close(DIRT *dirt)
	{
	if ( dirt->hff != (HANDLE) -1 )
		FindClose(dirt->hff);
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
		};
	return errlist[rc];
	}
/*...e*/
