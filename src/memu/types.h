/*

types.h - Common types

*/

#ifndef TYPES_H
#define	TYPES_H

#ifndef BYTE_TYPE
#define	BYTE_TYPE
typedef unsigned char byte;
#endif

#ifndef SBYTE_TYPE
#define	SBYTE_TYPE
typedef signed char  sbyte;
#endif

#ifndef WORD_TYPE
#define	WORD_TYPE
typedef unsigned short word;
#endif

#ifndef SWORD_TYPE
#define	SWORD_TYPE
typedef signed short sword;
#endif

typedef int BOOLEAN;
#ifndef TRUE
#define	TRUE 1
#define	FALSE 0
#endif

#endif
