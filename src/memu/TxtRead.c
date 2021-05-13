/* TxtRead.c   -  Text input routines.

Based on routines for reading CSV files, but providing similar input
formating to VB input.

*/

#include "TxtRead.h"
#include <stdlib.h>
#include <string.h>

#define  TXR_FLEN    25          /* Maximum length of numeric fields */

static char sField[TXR_FLEN+1];  /* Field buffer for numeric input */
static TxrMode txrmDefault = txrmNormal;  /* Default mode for new files */

/*** TxrDefaultMode **************************************************

Sets the default mode for new files opened with Txr library.

Inputs:

   txrm  =  Required default file mode.

Coding history:

   WJB   21/ 3/03 First draft.

*/

void TxrDefaultMode (TxrMode txrm)
   {
   txrmDefault =  txrm;
   }

/*** TxrGetText *************************************************************

Reads next field from Text input file. Fields are separated by white space
and/or a single comma. Text enclosed in double quotes is read as a single
field. This routine will not read beyond the end of the current line.

Inputs:

   ptxr     =  Text input file.
   nMax     =  Maximum length of text (including null terminator).

Outputs:

   psText   =  The input text field.

Function return:

   Actual length of field.

Coding history:

   WJB   22/ 9/98 First draft.
   WJB   23/ 9/98 Allow the text pointer to be null, in which case the field
                  is skipped.
   WJB   28/ 9/98 Allow for quoted text, containing commas.
   WJB    1/ 9/00 Converted to read white space delimited fields.
   WJB    4/ 9/00 Ensure string is terminated if at end of line.
   WJB   21/ 3/03 Implement CSV mode, where only a comma or new line
                  acts as a field separator. Leading and trailing
                  space is still deleted (unless within quotes),
                  but intermediate spaces are retained.
   WJB    3/ 4/03 Rewritten to use callback function so that C++
                  class can generate strings of unlimited length.
   WJB   14/ 4/03 Bug fix on terminating string.

*/

int TxrGetText (TXR *ptxr, int nMax, char *psText)
   {
   TxrBuff  tb;
   int      nLen, nEnd;

   /* Construct text buffer pointer */

   tb.nLen  =  nMax - 1;
   tb.nChr  =  0;
   tb.ps    =  psText;

   /* Obtain the text */

   nLen  =  TxrDoText (ptxr, TxrPutChr, (void *) &tb);

   /* Terminate string */

   if ( psText != NULL )
      {
      nEnd  =  nLen;
      if ( nEnd >= nMax ) nEnd = nMax - 1;
      psText[nEnd] = '\0';
      }

   return   nLen;
   }

/*** TxrPutChr *******************************************************

Accumulate characters in a C style, fixed length buffer.

Inputs:

   ptb   =  Pointer to character buffer definition.
   iChr  =  Character to end to the buffer.

Coding history:

   WJB    3/ 4/03 First draft.

*/

void TxrPutChr (void *pv, int iChr)
   {
   TxrBuff *ptb	  =	 (TxrBuff *) pv;
   if ( ( ptb->ps != NULL ) && ( ptb->nChr < ptb->nLen ) )
      {
      ptb->ps[ptb->nChr]   =  iChr;
      ++ptb->nChr;
      }
   }

/*** TxrDoText *******************************************************

Reads next field from Text input file. Fields are separated by white space
and/or a single comma. Text enclosed in double quotes is read as a single
field. This routine will not read beyond the end of the current line.

This routine uses a callback function to accumulate the characters.

Inputs:

   ptxr     =  Pointer to TXR structure defining file to read.
   tca      =  Callback function to accumulate characters.
   pvData   =  Data to pass to callback function.

Function return:

   Final length of string. Note the callback function may have been
   passed more characters than this, including trailing spaces.

Coding history:

   WJB    3/ 4/03 Based upon the original coding of TxrGetText.
   WJB   14/ 4/03 Restrict valid whitespace characters.
   WJB    9/ 5/03 Bug fix on reading quoted string.
   WJB   16/10/06 Variable quote characters, defined by structure.

*/

int TxrDoText (TXR *ptxr, TxrChrAcc tca, void *pvData)
{
   int   iCh;
   int   iChQuo;
   int   nLen  =  0;
   int   nTxt  =  0;
   Bool  bDone = ptxr->bEOL;
   Bool  bQuote = False;

   /* Skip leading white space */

   if ( ! bDone )
      {
      iCh = ptxr->sSpace[0];
      while ( strchr (ptxr->sSpace, iCh) != NULL )
         {
         iCh = getc (ptxr->pfil);
         }
      }

   /* Read the next field */

   while ( ! bDone )
      {
      /* Process character */

      if ( iCh == EOF )
         {
         /* End of file */

         ptxr->iChTerm  =  iCh;
         ptxr->bEOL     =  True;
         ptxr->bEOF     =  True;
         bDone          =  True;
         }
      else if ( iCh == '\n' )
         {
         /* End of line */

         ptxr->iChTerm  =  iCh;
         ptxr->bEOL     =  True;
         bDone          =  True;
         }
      else if ( bQuote )
         {
         /* Quoted text */

         if ( iCh == iChQuo )
            {
            bQuote   =  False;
            iChQuo   =  0;
            }
         else
            {
            tca (pvData, iCh);
            ++nLen;
            nTxt = nLen;
            }
         }
      else if ( strchr (ptxr->sQuote, iCh) != NULL )
         {
         /* Quoted string */

         bQuote = True;
         iChQuo =  iCh;
         }
      else if ( strchr (ptxr->sTerm, iCh) != NULL )
         {
         ptxr->iChTerm  =  iCh;
         bDone          =  True;
         }
      else
         {
         /* Field data */

         tca (pvData, iCh);
         ++nLen;
         if ( strchr (ptxr->sSpace, iCh) == NULL ) nTxt = nLen;
         }

      /* Get next character */

      if ( ! bDone ) iCh = getc (ptxr->pfil);
      }

   /* Return length excluding trailing spaces */

   return nTxt;
}

/*** TxrSkipField ***********************************************************

Skips a specified number of fields in one line of a Text file.

Inputs:

   ptxr     =  Text input file.
   nSkip    =  Number of fields to skip.

Function return:

   True if fields successfully skipped, otherwise False.

Coding history:

   WJB   23/ 9/98 First draft.

*/

Bool  TxrSkipField (TXR *ptxr, int nSkip)
{
   int   n;

   for ( n = 0; n < nSkip; n++ )
      {
      if ( ptxr->bEOL ) return False;
      TxrGetText (ptxr, 0, NULL);
      }

   return True;
}

/*** TxrNextLine ************************************************************

Moves to the start of the next line of a Text file. Optionally skips lines.

Inputs:

   ptxr  =  Text input file.
   nSkip =  Number of lines to advance.

Function return:

   True if the routine completed, False if End of File was encountered.

Coding history:

   WJB   22/ 9/98 First draft.
   WJB   23/ 9/98 Changed definition of lines to skip.
   WJB    1/ 9/00 Do not include comment lines (lines begining with '#'
                  or '!' in count of lines skipped.
   WJB   14/ 4/03 Revise skipping of white space.

*/

Bool TxrNextLine (TXR *ptxr, int nSkip)
{
   int   iCh;

   /* Skip lines */

   while ( nSkip > 0 )
      {
      /* Skip to end of current line */

      while ( ! ptxr->bEOL )
         {
         switch ( getc (ptxr->pfil) )
            {
            case EOF:
               {
               ptxr->bEOL = True;
               ptxr->bEOF = True;
               return False;
               }
            case '\n':
               {
               ptxr->bEOL = True;
               break;
               }
            default:
               {
               break;
               }
            }
         }

      /* Reset end of line flag */

      ptxr->bEOL = False;

      /* Skip leading white space */

      iCh = ptxr->sSpace[0];
      while ( strchr (ptxr->sSpace, iCh) != NULL )
         {
         iCh = getc (ptxr->pfil);
         }

      /* Test for comment */

      switch (iCh)
         {
         case EOF:
            {
            ptxr->bEOL = True;
            ptxr->bEOF = True;
            return False;
            }
         case '\n':
            {
            ptxr->bEOL = True;
            break;
            }
         case '#':
         case '!':
            {
            ++nSkip;
            break;
            }
         default:
            {
            ungetc (iCh, ptxr->pfil);
            break;
            }
         }

      /* Count lines skipped */

      --nSkip;
      }

   return True;
}

/*** TxrOpen ****************************************************************

Opens a Text file.

Inputs:

   psName   =  Name of the file to open.

Function return:

   Pointer to TXR structure if successful, otherwise NULL.

Coding history:

   WJB   22/ 9/98 First draft.
   WJB    1/ 9/00 Skip leading comments.
   WJB    1/ 9/00 Revised to return a TXR pointer as per fopen ().
   WJB   21/ 3/03 Set default file mode.
   WJB    4/ 4/03 Use call to TxrSetMode to perform more complex mode
                  initiation.

*/

TXR *TxrOpen (char const *psName)
{
   TXR   *ptxr;

   /* Allocate structure */

   ptxr = calloc (1, sizeof (TXR));
   if ( ptxr == NULL ) return ptxr;

   if ( ( ptxr->pfil = fopen (psName, "r") ) == NULL )
      {
      /* Failed to open file */

      free (ptxr);
      return NULL;
      }
   else
      {
      /* File was opened */

      ptxr->bEOF  =  False;
      ptxr->bEOL  =  True;
      TxrSetMode (ptxr, txrmDefault);

      /* Skip comment lines */

      TxrNextLine (ptxr, 1);
      }

   return ptxr;
}

/*** TxrClose ***************************************************************

Closes a specified Text file.

Inputs:

   ptxr  =  Pointer to Text data structure.

Coding history:

   WJB   22/ 9/98 First draft.
   WJB    1/ 9/00 Free TXR structure.

*/

void TxrClose (TXR *ptxr)
{
   if ( ptxr->pfil != NULL ) fclose (ptxr->pfil);
   ptxr->pfil = NULL;
   ptxr->bEOF = True;
   ptxr->bEOL = True;
   free (ptxr);
}

/*** TxrGetInt **************************************************************

Reads an integer value from a Text file.

Inputs:

   ptxr  =  Pointer to Text data structure.

Function return:

   Integer value read.

Coding history:

   WJB    1/ 9/00 First draft.

*/

int TxrGetInt (TXR *ptxr)
{
   int iValue = 0;

   /* Read next field */

   TxrGetText (ptxr, TXR_FLEN, sField);

   /* Convert to integer */

   sscanf (sField, "%i", &iValue);
   return iValue;
}

/*** TxrGetReal *************************************************************

Reads a real value from a Text file.

Inputs:

   ptxr  =  Pointer to Text data structure.

Function return:

   Real (double) value read.

Coding history:

   WJB    1/ 9/00 First draft.

*/

double TxrGetReal (TXR *ptxr)
{
   /* Read next field */

   TxrGetText (ptxr, TXR_FLEN, sField);

   /* Convert to integer */

   return atof (sField);
}

/*** TxrGetTerm ******************************************************

Gets the character that terminated the last token.

Inputs:

   ptxr  =  Pointer to TXR structure.

Function Return:

   The terminating character.

Coding history:

   WJB    3/ 4/03 First draft.

*/

int TxrGetTerm (TXR *ptxr)
   {
   return   ptxr->iChTerm;
   }

/*** TxrSetMode ******************************************************

Change the read mode of a currently open text file.

Inputs:

   ptxr  =  Pointer to Text data structure.
   txrm  =  The required read mode.

Coding history:

   WJB   21/ 3/03 First draft.
   WJB    3/ 4/03 Set terminator strings appropriately.
   WJB   16/10/06 Initialise quote characters.

*/

void TxrSetMode (TXR *ptxr, TxrMode txrm)
   {
   switch (txrm)
      {
      case  txrmToken:
         {
         TxrSetTerm (ptxr, ",");
         strcpy (ptxr->sSpace, " \t");
         strcpy (ptxr->sQuote, "\"'");
         break;
         }
      case  txrmCsv:
         {
         strcpy (ptxr->sTerm, ",");
         strcpy (ptxr->sSpace, " \t");
         strcpy (ptxr->sQuote, "\"'");
         break;
         }
      default:
         {
         strcpy (ptxr->sTerm, " \t");
         strcpy (ptxr->sSpace, " \t");
         strcpy (ptxr->sQuote, "\"'");
         txrm  =  txrmNormal;
         break;
         }
      }
   ptxr->txrm  =  txrm;
   }

/*** TxrSetTerm ******************************************************

Sets terminating characters for token mode.

Input:

   ptxr     =  Pointer to TXR structure.
   psTerm   =  String of terminating characters.

Coding history:

   WJB    3/ 4/03 First draft.
   WJB   14/ 4/03 Exclude tab from whitespace if it is a token
                  separator.

*/

void TxrSetTerm (TXR *ptxr, const char *psTerm)
   {
   strncpy (ptxr->sTerm, psTerm, MAX_TERM);
   ptxr->sTerm[MAX_TERM]   =  '\0';
   ptxr->txrm  =  txrmToken;
   strcpy (ptxr->sSpace, " ");
   if ( strchr (ptxr->sTerm, '\t') == NULL )
      {
      strcat (ptxr->sSpace, "\t" );
      }
   }

/*** TxrSetQuote *****************************************************

Sets quote characters.

Input:

   ptxr     =  Pointer to TXR structure.
   psQuote  =  String of quote characters.

Coding history:

   WJB   16/10/06 First draft, based upon TxrSetTerm.

*/

void TxrSetQuote (TXR *ptxr, const char *psQuote)
   {
   strncpy (ptxr->sQuote, psQuote, MAX_QUOTE);
   ptxr->sQuote[MAX_QUOTE] =  '\0';
   }

/*** TxrQueryMode ****************************************************

Gets the current file input mode for an open file.

Inputs:

   ptxr  =  Pointer to Text data structure.

Function return:

   The current file mode.

Coding history:

   WJB   21/ 3/03 First draft.
   WJB   16/10/06 Changed name from TxrGetMode to TxrQueryMode.

*/

TxrMode TxrQueryMode (TXR *ptxr)
   {
   return   ptxr->txrm;
   }

/*** TxrQueryTerm ****************************************************

Gets the current string termination characters for an open file.

Inputs:

   ptxr  =  Pointer to Text data structure.

Function return:

   Pointer to a string containing the termination characters.

Coding history:

   WJB   16/10/06 First draft.

*/

const char * TxrQueryTerm (TXR *ptxr)
   {
   return   ptxr->sTerm;
   }

/*** TxrQueryQuote ****************************************************

Gets the current string quotation characters for an open file.

Inputs:

   ptxr  =  Pointer to Text data structure.

Function return:

   Pointer to a string containing the quotation characters.

Coding history:

   WJB   16/10/06 First draft.

*/

const char * TxrQueryQuote (TXR *ptxr)
   {
   return   ptxr->sQuote;
   }

/*** TxrRewind *******************************************************

Rewind the file to the beginning.

Inputs:

   ptxr  =  Pointer to TXR structure.

Coding history:

   WJB   11/ 7/03 First draft.

*/

void TxrRewind (TXR *ptxr)
   {
   fseek (ptxr->pfil, 0L, SEEK_SET);
   ptxr->bEOF  =  False;
   ptxr->bEOL  =  True;

   /* Skip comment lines */

   TxrNextLine (ptxr, 1);
   }
