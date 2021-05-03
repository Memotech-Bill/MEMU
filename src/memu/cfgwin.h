/* cfgwin.h - Window functions for Configuration dialog */

#ifndef CFGWIN_H
#define CFGWIN_H

void cfg_wininit (void);
void cfg_set_display (void);
void config_term (void);
void cfg_keypress(int wk);
void cfg_keyrelease(int wk);
void cfg_print (int iRow, int iCol, int iSty, const char *psTxt, int nCh);
void cfg_csr (int iRow, int iCol, int iSty);
void cfg_clear_rows (int iFirst, int iLast);
int cfg_key (void);

#endif
