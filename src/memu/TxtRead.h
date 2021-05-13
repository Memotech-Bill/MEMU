/* txtread.h - Function prototypes for txtread.c

Coding history:

   WJB   14/ 4/03 Added sSpace field to structure.
   WJB   12/10/06 Added TxrEol macro.
   WJB   16/10/06 Added sQuote to TXR structure. Also added
                  "Query" functions.

*/

#ifndef H_TXTREAD
#define H_TXTREAD

#include <stdio.h>

#define  MAX_SPACE   2     /* Maximum number of whitespace characters */
#define  MAX_TERM    10    /* Maximum number of token terminators */
#define  MAX_QUOTE   2     /* Maximum number of quote characters */

/* File data structures */

#include "cbool.h"
typedef enum {txrmNormal, txrmCsv, txrmToken } TxrMode;

typedef struct s_txr
{
   FILE     *pfil;      /* File stream for CSV file */
   TxrMode  txrm;       /* Mode for file processing */
   char     sSpace[MAX_SPACE+1]; /* List of whitespace characters */
   char     sTerm[MAX_TERM+1];   /* List of token terminating characters */
   char     sQuote[MAX_QUOTE+1]; /* List of quote characters */
   int      iChTerm;    /* Actual terminating character */
   Bool     bEOL;       /* End of line flag */
   Bool     bEOF;       /* End of file flag */
}  TXR;

typedef struct s_txr_buf
   {
   int      nLen;       /* Length of buffer */
   int      nChr;       /* Number of characters stored */
   char     *ps;        /* Pointer to buffer.*/
   } TxrBuff;

/* Function prototype for character accumulator */

typedef void (*TxrChrAcc) (void *pv, int iChr);

/* Function macros */

#define TxrEof(ptxr)    (ptxr->bEOF)      /* Test for end of file */
#define TxrEol(ptxr)    (ptxr->bEOL)      /* Test for end of line */

/* Function prototypes */

#ifdef __cplusplus
extern "C"
{
#endif

void TxrDefaultMode (TxrMode txrm);
int TxrGetText (TXR *, int, char *);
void TxrPutChr (void *, int iChr);
int TxrDoText (TXR *, TxrChrAcc tca, void *pvData);
Bool TxrSkipField (TXR *, int);
Bool TxrNextLine (TXR *, int);
TXR *TxrOpen (const char *);
void TxrClose (TXR *);
int TxrGetInt (TXR *);
double TxrGetReal (TXR *);
int TxrGetTerm (TXR *ptxr);
void TxrSetMode (TXR *ptxr, TxrMode txrm);
void TxrSetTerm (TXR *ptxr, const char *psTerm);
void TxrSetQuote (TXR *ptxr, const char *psQuote);
void TxrRewind (TXR *ptxr);
TxrMode TxrQueryMode (TXR *ptxr);
const char * TxrQueryTerm (TXR *ptxr);
const char * TxrQueryQuote (TXR *ptxr);

#ifdef __cplusplus
}
#endif

#endif

