/*

dirt.h - Interface to Directory Traversal - UNIX version

*/

#ifndef DIRT_H
#define	DIRT_H

#define	DIRTE_OK            0
#define	DIRTE_GEN_ERROR     1
#define	DIRTE_NO_MEMORY     2
#define	DIRTE_NOT_FOUND     3
#define	DIRTE_NOT_DIRECTORY 4
#define	DIRTE_NO_ACCESS     5
#define	DIRTE_NO_RESOURCE   6

#ifndef DIRT_C

typedef void DIRT;

extern DIRT *dirt_open(const char *dirname, int *rc);
extern const char *dirt_next(DIRT *dirt);
extern void dirt_close(DIRT *dirt);

extern const char *dirt_error(int rc);

#endif

#endif
