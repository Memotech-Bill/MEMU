/* bool.h   -  Defines bool data type for C programs.

Coding history:

   WJB   23/ 5/07 First draft.
   WJB   13/ 6/07 Do not emulate C++ type. Causes problems
                  with mixed code.

*/

#ifndef  H_BOOL
#define  H_BOOL

typedef enum { False, True } Bool;

#endif
