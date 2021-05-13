//
// main_cir.cpp
//
// MEMU version based upon:
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <circle/startup.h>
#include <circle/string.h>
#include <circle/i2cmaster.h>
#include <circle/timer.h>
#include <circle/util.h>
#include <circle/machineinfo.h>
#include <stdlib.h>
#include "kernel.h"
#include "kfuncs.h"
#include "keyeventbuffer.h"
#include "gpio.h"

#define N_I2C_DEV   2   /* Number of I2C devices */
#define N_I2C_CON   8   /* Maximum number of I2C connections */

static CKernel * pker = 0;
static CI2CMaster *s_pI2CMaster[N_I2C_DEV] = { NULL, NULL };
struct
    {
    int iDev;
    int iAddr;
    }
    s_i2c_con[N_I2C_DEV];
static int n_i2cc = 0;

int main (void)
    {
    // cannot return here because some destructors used in CKernel are not implemented

    CKernel Kernel;
    pker = & Kernel;

    if ( ! Kernel.Initialize () )
        {
        halt ();
        return EXIT_HALT;
        }

    TShutdownMode ShutdownMode = Kernel.Run ();

    switch (ShutdownMode)
        {
        case ShutdownReboot:
            reboot ();
            return EXIT_REBOOT;

        case ShutdownHalt:
        default:
            halt ();
            return EXIT_HALT;
        }
    }

void Quit (void)
    {
    pker->Quit ();
    }

void GetFBInfo (struct FBInfo *pfinfo)
    {
    pker->GetFBInfo (pfinfo);
    }

void SetFBPalette (int iCol, u32 iRGB)
    {
    pker->SetFBPalette (iCol, iRGB);
    }

void UpdateFBPalette (void)
    {
    pker->UpdateFBPalette ();
    }

int GetKeyEvent (u8 *pkey)
    {
    u8  key = CKeyEventBuffer::GetEvent ();
    if ( key != 0 )
        {
        *pkey = key;
        return 1;
        }
    return 0;
    }

void SetKbdLeds (u8 uLeds)
    {
    CKeyEventBuffer::SetLeds (uLeds);
    }

int InitSound (void)
    {
    return pker->InitSound ();
    }

void TermSound (void)
    {
    pker->TermSound ();
    }

void CircleYield (void)
    {
    pker->Yield ();
    }

int i2c_init (const char *psDev, int iAddr)
	{
    int iDev = psDev[strlen (psDev)-1] - '0';
    int iCon = -1;
    if ( ( iDev < 0 ) || ( iDev >= N_I2C_DEV ) ) return I2C_EOPEN;
    if ( n_i2cc < N_I2C_CON - 1 )
        {
        iCon = n_i2cc;
        ++n_i2cc;
        }
    else
        {
        int i;
        for ( i = 0; i < N_I2C_CON; ++i )
            {
            if ( s_i2c_con[i].iDev < 0 )
                {
                iCon = i;
                break;
                }
            }
        }
    if ( iCon < 0 ) return I2C_EOPEN;
    s_i2c_con[iCon].iDev = iDev;
    s_i2c_con[iCon].iAddr = iAddr;
    if ( s_pI2CMaster[iDev] == NULL )
        {
        s_pI2CMaster[iDev] = new CI2CMaster (iDev, TRUE);
        if ( ! s_pI2CMaster[iDev]->Initialize () )
            {
            i2c_term (iCon + 1);
            return I2C_EOPEN;
            }
        }
    return iCon + 1;
	}

void i2c_term (int fd)
	{
    int iDev = s_i2c_con[fd - 1].iDev;
    CI2CMaster *pI2C = s_pI2CMaster[iDev];
    int i;
    s_i2c_con[fd - 1].iDev = -1;
    while ( ( n_i2cc > 0 ) && ( s_i2c_con[n_i2cc - 1].iDev < 0 ) ) --n_i2cc;
    for ( i = 0; i < n_i2cc; ++ i )
        {
        if ( s_i2c_con[i].iDev == iDev ) return;
        }
    if ( pI2C != NULL ) delete pI2C;
    s_pI2CMaster[iDev] = NULL;
	}

int i2c_put (int fd, int iReg, int iLen, unsigned char *pbData)
	{
    int iDev = s_i2c_con[fd - 1].iDev;
    int iAddr = s_i2c_con[fd - 1].iAddr;
    CI2CMaster *pI2C = s_pI2CMaster[iDev];
	int	 iSta  =  I2C_OK;
    unsigned char *pbBuff = NULL;
    if ( fd < 0 ) return I2C_EOPEN;
    pbBuff = (unsigned char *) malloc (iLen + 1);
    if ( pbBuff == NULL )	return	 I2C_EMEM;
    pbBuff[0]  =  (unsigned char) iReg;
    memcpy (&pbBuff[1], pbData, iLen);
    if ( pI2C->Write ((u8) iAddr, pbBuff, iLen + 1) < iLen + 1 ) iSta = I2C_EIO;
	free (pbBuff);
    return	iSta;
	}

int i2c_get (int fd, int iReg, int iLen, unsigned char *pbData)
	{
    int iDev = s_i2c_con[fd - 1].iDev;
    int iAddr = s_i2c_con[fd - 1].iAddr;
    CI2CMaster *pI2C = s_pI2CMaster[iDev];
    unsigned char bReg = (unsigned char) iReg;
    if ( pI2C->Write ((u8) iAddr, &bReg, 1) < 1 ) return I2C_EIO;
    if ( pI2C->Read ((u8) iAddr, pbData, iLen) < iLen ) return I2C_EIO;
    return I2C_OK;
	}

int gpio_revision (void)
	{
	static int	iRev   =  -1;
	if ( iRev > 0 )	 return	  iRev;
    CMachineInfo *pinfo = CMachineInfo::Get ();
    switch ( pinfo->GetMachineModel () )
        {
        case MachineModelA:
            if ( pinfo->GetModelRevision () == 1 )  iRev = 1;
            else                                    iRev = 2;
            break;
        case MachineModelBRelease1MB256:
            iRev = 1;
            break;
        case MachineModelBRelease2MB256:
        case MachineModelBRelease2MB512:
            iRev = 2;
            break;
        default:
            iRev = 3;
            break;
        }
    return iRev;
    }
