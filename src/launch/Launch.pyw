#!/usr/bin/python
#  MEMU Launcher GUI
#
#  WJB   30/ 1/14 First draft.
#  WJB    8/ 2/14 Added configuration save.
#  WJB   15/ 2/14 v0.03 - Further options added (fdxb, tape disable, cpm tail...)
#  WJB   16/ 2/14 v0.04 - Bug fix on FDXB option.
#  WJB   23/ 2/14 v0.05 - Some consistency checks.
#  WJB   23/ 2/14 v0.06 - Correct stupid typo.
#  WJB   28/ 2/14 v0.07 - Trap insufficient memory for CP/M. Add support for ROM loading and mfloppies
#  WJB    2/ 3/14 v0.08 - Added -mon-ignore-init and -cpm-open-hack.
#  WJB    8/ 3/14 v0.09 - Added general memory initialisation (-iobyte, -subpage, -addr, -mem).
#                         Tidied up window management and resizing.
#                         Added -snd-latency and -kbd-country.
#  WJB    9/ 3/14 v0.10 - Added further options.
#  WJB   15/ 3/14         Bug fix on memory initialisation. Added configuration load, save and reset.
#  WJB   22/ 3/14 v0.11 - Revised command line options.
#  WJB   23/ 3/14 v0.12 - Added Dave's run command option.
#
try:
    from tkinter import *                 # Python 3.x imports
    from tkinter.filedialog import *
    from tkinter.messagebox import *
except ImportError:
    from Tkinter import *                 # Python 2.x imports
    from tkFileDialog import *
    from tkMessageBox import *
import subprocess
import os
import sys

iovals = ('0x00', '0x01', '0x02', '0x03', '0x04', '0x05', '0x06', '0x07',
          '0x08', '0x09', '0x0A', '0x0B', '0x0C', '0x0D', '0x0E', '0x0F',
          '0x10', '0x20', '0x30', '0x40', '0x50', '0x60', '0x70',
          '0x80', '0x81', '0x82', '0x83', '0x84', '0x85', '0x86', '0x87',
          '0x88', '0x89', '0x8A', '0x8B', '0x8C', '0x8D', '0x8E', '0x8F')
joyvals = ('Default', '1%', '2%', '5%', '10%', '20%', '30%', '40%', '50%', '60%', '70%', '80%', '90%', '100%')
spdvals = ('1MHz', '2MHz', '3MHz', '4MHz', '5MHz', '6MHz', '8MHz', '10MHz', 'Fast')

class InOut:
    items = (('Auto type file', '-kbd-type-file'),
             ('Printer output file', '-prn-file'),
             ('Serial 1 input', '-serial1-in'),
             ('Serial 1 output', '-serial1-out'),
             ('Serial 2 input', '-serial2-in'),
             ('Serial 2 output', '-serial2-out'))

    def __init__ (self, parent):
        self.parent = parent
        self.joybut = StringVar ()
        self.typein = StringVar ()
        self.files = []
        for iFile in range (len (InOut.items)):
            self.files.append (StringVar ())
        self.Reset ()

    def Reset (self):
        self.typdir = '.'
        self.prndir = '.'
        self.serdir = '.'
        self.joybut.set ('')
        self.typein.set ('')
        for f in self.files:
            f.set ('')

    def Show (self):
        self.win = Toplevel ()
        self.win.transient (self.parent)
        self.win.resizable (width=True, height=False)
        self.win.title ('Inputs and Outputs')
        self.win.grid_columnconfigure (0, weight=0)
        self.win.grid_columnconfigure (1, weight=1)
        self.win.grid_columnconfigure (2, weight=0)
        r = 0
        Label (self.win, text='Joystick buttons').grid (row=r, column=0, sticky='E')
        Entry (self.win, textvariable=self.joybut).grid (row=r, column=1, columnspan=2, sticky='EW')
        r += 1
        Label (self.win, text='Keyboard autotype').grid (row=r, column=0, sticky='E')
        Entry (self.win, textvariable=self.typein).grid (row=r, column=1, columnspan=2, sticky='EW')
        for iFile in range (len (InOut.items)):
            if ( iFile == 2 ):
                r += 1
                Label (self.win, text='Note: Linux only for the following').grid (row=r, column=0, columnspan=3, sticky='W')
            r += 1
            Label (self.win, text=InOut.items[iFile][0]).grid (row=r, column=0, sticky='E')
            Entry (self.win, textvariable=self.files[iFile]).grid (row=r, column=1, sticky='EW')
            Button (self.win, text='...', command=lambda i=iFile: self.OnBrowse (i)).grid (row=r, column=2, sticky='EW')
        r += 1
        Button (self.win, text='OK', command=self.win.destroy).grid (row=r, column=0, columnspan=3)
        self.win.focus_set ()
        self.win.grab_set ()
        self.win.wait_window ()

    def OnBrowse (self, iFile):
        if ( iFile == 0 ):
            dn = self.typdir
        elif ( iFile == 1 ):
            dn = self.prndir
        else:
            dn = self.serdir
        if ( iFile % 2 ):
            fn = asksaveasfilename (initialdir=dn,
                                  filetypes=[('All files', '*')]);
        else:
            fn = askopenfilename (initialdir=dn,
                                  filetypes=[('All files', '*')]);
        if ( fn ):
            self.files[iFile].set (fn)
            dn = os.path.dirname (fn)
            if ( iFile == 0 ):
                self.typdir = dn
            elif ( iFile == 1 ):
                self.prndir = dn
            else:
                self.serdir = dn

    def SetConfig (self, l):
        args = l[0]
        i = l[1]
        if ( i + 1 < len (args) ):
            if ( args[i] == '-joy-buttons' ):
                i += 1
                self.joybut.set (args[i])
                l[1] = i
                return True
            if ( args[i] == '-kbd-type' ):
                i += 1
                self.typein.set (args[i])
                l[1] = i
                return True
            for iFile in range (len (InOut.items)):
                if ( args[i] == InOut.items[iFile][1] ):
                    i += 1
                    self.files[iFile].set (args[i])
                    l[1] = i
                    return True
        return False

    def GetConfig (self):
        args = []
        joybut = self.joybut.get ()
        if ( joybut ):
            args.extend (['-joy-buttons', joybut])
        typein = self.typein.get ()
        if ( typein ):
            args.extend (['-kbd-type', typein])
        for iFile in range (len (self.files)):
            fn = self.files[iFile].get ()
            if ( fn ):
                args.extend ([InOut.items[iFile][1], fn])
        return args

class MemLine:
    def __init__ (self, parent):
        self.parent = parent
        self.iobyte = StringVar ()
        self.subpage = StringVar ()
        self.address = StringVar ()
        self.file = StringVar ()
        self.Reset ()

    def Reset (self):
        self.iobyte.set ('0x00')
        self.subpage.set ('0')
        self.address.set ('')
        self.file.set ('')

    def Show (self, win, r):
        if ( not self.file.get () ):
            ioval = '0x00'
            self.subpage.set ('0')
            self.address.set ('')
        else:
            ioval = self.iobyte.get ()
        Spinbox (win, textvariable=self.iobyte, values=iovals, width=4).grid (row=r, column=0, sticky='EW')
        self.iobyte.set (ioval)
        Spinbox (win, textvariable=self.subpage, from_=0, to=15, width=2).grid (row=r, column=1, sticky='EW')
        Entry (win, textvariable=self.address, width=6).grid (row=r, column=2, sticky='EW')
        Entry (win, textvariable=self.file).grid (row=r, column=3, sticky='EW')
        Button (win, text='...', command=self.Browse).grid (row=r, column=4, sticky='EW')

    def Browse (self):
        fn = askopenfilename (initialdir=self.parent.romdir,
                              filetypes=[('ROM files', '*.rom'),
                                         ('Tape files', '*.mtx'),
                                         ('Run files', '*.run'),
                                         ('Binary files', '*.bin'),
                                         ('All files', '*')]);
        if ( fn ):
            self.file.set (fn)
            self.parent.romdir = os.path.dirname (fn)

class Memory:
    def __init__ (self, parent):
        self.parent = parent
        self.files = []
        for iFile in range (8):
            self.files.append (MemLine (self))
        self.iobyte = StringVar ()
        self.subpage = StringVar ()
        self.address = StringVar ()

    def ResetBase (self):
        self.romdir = '.'
        self.iobyte.set ('0x00')
        self.subpage.set ('0')
        self.address.set ('0x0000')
        self.ncfg = -1

    def Reset (self):
        self.ResetBase ()
        for f in self.files:
            f.Reset ()

    def Show (self):
        self.win = Toplevel ()
        self.win.transient (self.parent)
        self.win.resizable (width=True, height=False)
        self.win.title ('Memory Initialisation')
        self.win.grid_columnconfigure (0, weight=0)
        self.win.grid_columnconfigure (1, weight=0)
        self.win.grid_columnconfigure (2, weight=0)
        self.win.grid_columnconfigure (3, weight=1)
        self.win.grid_columnconfigure (4, weight=0)
        r = 0
        Label (self.win, text='Note: For loading system ROMs it is usually easier ' +
               'to use the ROMS configuration dialog.\n').grid (row=r, column=0, columnspan=5)
        r += 1
        Label (self.win, text='IOByte').grid (row=r, column=0, sticky='EW')
        Label (self.win, text='Sub-page').grid (row=r, column=1, sticky='EW')
        Label (self.win, text='Address').grid (row=r, column=2, sticky='EW')
        Label (self.win, text='File').grid (row=r, column=3, sticky='EW')
        for iFile in range (8):
            r += 1
            self.files[iFile].Show (self.win, r)
        r += 1
        Label (self.win, text='Starting Location:').grid (row=r, column=0, columnspan=5, sticky='W')
        r += 1
        Spinbox (self.win, textvariable=self.iobyte, values=iovals, width=4).grid (row=r, column=0, sticky='EW')
        Spinbox (self.win, textvariable=self.subpage, from_=0, to=15, width=2).grid (row=r, column=1, sticky='EW')
        Entry (self.win, textvariable=self.address, width=6).grid (row=r, column=2, sticky='EW')
        r += 1
        Button (self.win, text='OK', command=self.win.destroy).grid (row=r, column=0, columnspan=5)
        self.win.focus_set ()
        self.win.grab_set ()
        self.win.wait_window ()

    def SetConfig (self, l):
        args = l[0]
        i = l[1]
        if ( i + 1 < len (args) ):
            if ( args[i] == '-iobyte' ):
                if ( self.ncfg < 0 ):
                    self.ncfg = 0
                i += 1
                self.iobyte.set (args[i])
                l[1] = i
                return True
            elif ( ( args[i] == '-subpage' ) and ( self.ncfg >= 0 ) ):
                i += 1
                self.subpage.set (args[i])
                l[1] = i
                return True
            elif ( args[i] == '-addr' ):
                if ( self.ncfg < 0 ):
                    self.ncfg = 0
                i += 1
                self.address.set (args[i])
                l[1] = i
                return True
            elif ( args[i] == '-mem' ):
                if ( self.ncfg < 0 ):
                    self.ncfg = 0
                if ( self.ncfg < 8 ):
                    i += 1
                    self.files[self.ncfg].iobyte.set (self.iobyte.get ())
                    self.files[self.ncfg].subpage.set (self.subpage.get ())
                    self.files[self.ncfg].address.set (self.address.get ())
                    self.files[self.ncfg].file.set (args[i])
                    self.romdir = os.path.dirname (args[i])
                    self.ncfg += 1
                    l[1] = i
                    return True
        return False

    def GetConfig (self):
        args = []
        l_iobyte = ''
        l_subpage = '0'
        l_address = '0x0000'
        for iFile in range (8):
            iobyte = self.files[iFile].iobyte.get ()
            subpage = self.files[iFile].subpage.get ()
            address = self.files[iFile].address.get ()
            file = self.files[iFile].file.get ()
            if ( file ):
                if ( iobyte != l_iobyte ):
                    args.extend (['-iobyte', iobyte])
                    l_iobyte = iobyte
                if ( subpage != l_subpage ):
                    args.extend (['-subpage', subpage])
                    l_subpage = subpage
                if ( ( address ) and ( address != l_address ) ):
                    args.extend (['-addr', address])
                    l_address = address
                args.extend (['-mem', file])
        iobyte = self.iobyte.get ()
        subpage = self.subpage.get ()
        address = self.address.get ()
        if ( ( l_iobyte == '' ) and ( subpage == '0' ) ):
            l_iobyte = '0x00'
        if ( iobyte != l_iobyte ):
            args.extend (['-iobyte', iobyte])
        if ( subpage != l_subpage ):
            args.extend (['-subpage', subpage])
        if ( ( address ) and ( address != l_address ) ):
            args.extend (['-addr', address])
        return args

class ROM2:
    def __init__ (self, parent):
        self.parent = parent
        self.files = {}
        for iPage in range (0, 16):
            self.files[iPage] = StringVar ()
        self.init = StringVar ()
        self.Reset ()

    def Reset (self):
        self.page = 0
        for iPage in self.files:
            self.files[iPage].set ('')
        self.init.set ('0')

    def Show (self):
        self.win = Toplevel ()
        self.win.transient (self.parent.win)
        self.win.resizable (width=True, height=False)
        self.win.grid_columnconfigure (0, weight=0)
        self.win.grid_columnconfigure (1, weight=1)
        self.win.grid_columnconfigure (2, weight=0)
        self.win.grid_columnconfigure (3, weight=0)
        self.win.grid_columnconfigure (4, weight=1)
        self.win.grid_columnconfigure (5, weight=0)
        self.win.title ('ROM2 Sub-pages')
        for iPage in range (0, 16):
            r = iPage % 8
            c = 3 * ( iPage // 8 )
            Label (self.win, text=str (iPage)).grid (row=r, column=c, sticky='E')
            Entry (self.win, textvariable=self.files[iPage]).grid (row=r, column=c+1, sticky='EW')
            Button (self.win, text='...', command=lambda i=iPage:
                       self.SetPage (i)).grid (row=r, column=c+2, sticky='W')
        Label (self.win, text='Initial subpage').grid (row=8, column=0, columnspan=3, sticky='E')
        Spinbox (self.win, textvariable=self.init, width=2, from_=0, to=15, increment=1
                 ).grid (row=8, column=3, sticky='W')
        Button (self.win, text='OK', command=self.OnOK).grid (row=9, column=0, columnspan=6)
        self.win.focus_set ()
        self.win.grab_set ()
        self.win.wait_window ()

    def SetPage (self, iPage):
        fn = askopenfilename (initialdir=self.parent.romdir,
                              filetypes=[('ROM files', '*.rom'),
                                         ('All files', '*')]);
        if ( fn ):
            self.files[iPage].set (fn)
            self.parent.romdir = os.path.dirname (fn)

    def OnOK (self):
        self.win.destroy ()

    def SetConfig (self, l):
        args = l[0]
        i = l[1]
        if ( args[i] == '-subpage' ):
            i += 1
            if ( i < len (args) ):
                iPage = int (args[i])
                if ( ( i >= 0 ) and ( i < 16 ) ):
                    self.page = iPage
                    self.init.set (str (iPage))
                    l[1] = i
                    return True
        elif ( args[i] == '-rom2' ):
            i += 1
            if ( i < len (args) ):
                self.files[self.page].set (args[i])
                self.parent.romdir = os.path.dirname (args[i])
                l[1] = i
                return True
        return False

    def GetConfig (self):
        margs = []
        for iPage in range (0, 16):
            fn = self.files[iPage].get ()
            if ( fn ):
                margs.extend (['-subpage', str (iPage), '-rom2', fn])
        if ( len (margs) > 0 ):
            margs.extend (['-subpage', self.init.get ()])
        return margs

class ROMS:
    def __init__ (self, parent):
        self.parent = parent
        self.rom2 = ROM2 (self)
        self.files = {}
        for iRom in range (3, 8):
            self.files[iRom] = StringVar ()
        self.ResetBase ()

    def ResetBase (self):
        self.romdir = '.'
        for iPage in self.files:
            self.files[iPage].set ('')

    def Reset (self):
        self.ResetBase ()
        self.rom2.Reset ()

    def Show (self):
        self.win = Toplevel ()
        self.win.resizable (width=True, height=False)
        self.win.transient (self.parent)
        self.win.grid_columnconfigure (0, weight=0)
        self.win.grid_columnconfigure (1, weight=1)
        self.win.grid_columnconfigure (2, weight=0)
        self.win.title ('Initialise ROM memory')
        Label (self.win, text='ROM 2').grid (row=0, column=0, sticky='E')
        Button (self.win, text='Sub-pages', command=self.rom2.Show).grid (row=0, column=1, sticky='W')
        for iRom in range (3, 8):
            Label (self.win, text='ROM %d' % iRom).grid (row=iRom-2, column=0, sticky='E')
            Entry (self.win, textvariable=self.files[iRom]).grid (row=iRom-2, column=1, sticky='EW')
            Button (self.win, text='...', command=lambda i=iRom: self.SetRom (i)).grid (row=iRom-2, column=2, sticky='EW')
        Button (self.win, text='OK', command=self.OnOK).grid (row=6, column=1)
        self.win.focus_set ()
        self.win.grab_set ()
        self.win.wait_window ()

    def SetRom (self, iRom):
        fn = askopenfilename (initialdir=self.romdir,
                              filetypes=[('ROM files', '*.rom'),
                                         ('All files', '*')]);
        if ( fn ):
            self.files[iRom].set (fn)
            self.romdir = os.path.dirname (fn)

    def OnOK (self):
        self.win.destroy ()

    def SetConfig (self, l):
        args = l[0]
        i = l[1]
        if ( self.rom2.SetConfig (l) ):
            return True
        elif ( args[i].startswith ('-rom') ):
            iRom = int (args[i][4:])
            i += 1
            if ( ( iRom >= 3 ) and ( iRom <= 7 ) and ( i < len (args) ) ):
                self.files[iRom].set (args[i])
                self.romdir = os.path.dirname (args[i])
                l[1] = i
                return True
        return False

    def GetConfig (self):
        margs = self.rom2.GetConfig ()
        for iRom in range (3, 8):
            fn = self.files[iRom].get ()
            if ( fn ):
                margs.extend (['-rom%d' % iRom, fn])
        return margs

class DISKS:
    def __init__ (self, parent):
        self.parent = parent
        self.fdisk = {}
        self.tracks = {}
        self.sdisk = {}
        for iDisk in range (2):
            self.fdisk[iDisk] = StringVar ()
            self.tracks[iDisk] = StringVar ()
        for iDisk in range (4):
            self.sdisk[iDisk] = StringVar ()
        self.Reset ()

    def Reset (self):
        self.diskdir = '.'
        for iDisk in range (2):
            self.fdisk[iDisk].set ('')
            self.tracks[iDisk].set ('80')
        for iDisk in range (4):
            self.sdisk[iDisk].set ('')

    def Show (self):
        self.win = Toplevel ()
        self.win.transient (self.parent)
        self.win.resizable (width=True, height=False)
        self.win.title ('Disk Images')
        self.win.grid_columnconfigure (0, weight=0)
        self.win.grid_columnconfigure (1, weight=0)
        self.win.grid_columnconfigure (2, weight=1)
        self.win.grid_columnconfigure (3, weight=0)
        r = 0
        Label (self.win,
               text='To use disk images, select MTX emulation, and load CP/M or SDX ROMS.\n' +
               'Do not select SDX, CPM or FDXB emulation (those use "Drive A directory").\n'
               ).grid (row=r, column=0, columnspan=4)
        r += 1
        Label (self.win, text='Disk').grid (row=r, column = 0, sticky='W')
        Label (self.win, text='Tracks').grid (row=r, column = 1, sticky='W')
        Label (self.win, text='File').grid (row=r, column=2, sticky='W')
        for iDisk in range (2):
            r += 1
            Label (self.win, text='Floppy Disk %d' % (iDisk + 1)).grid (row=r, column=0, sticky='E')
            Spinbox (self.win, textvariable=self.tracks[iDisk], from_=40, to=80, increment=40, width=2
                     ).grid (row=r, column=1, sticky='EW')
            Entry (self.win, textvariable=self.fdisk[iDisk]).grid (row=r, column=2, sticky='EW')
            Button (self.win, text='...', command=lambda i=iDisk: self.SetFloppy (i)).grid (row=r, column=3, sticky='EW')
        for iDisk in range (4):
            r += 1
            Label (self.win, text='Silicon Disk %d' % (iDisk + 1)).grid (row=r, column=0, sticky='E')
            Entry (self.win, textvariable=self.sdisk[iDisk]).grid (row=r, column=2, sticky='EW')
            Button (self.win, text='...', command=lambda i=iDisk: self.SetSiDisk (i)).grid (row=r, column=3, sticky='EW')
        r += 1
        Button (self.win, text='OK', command=self.OnOK).grid (row=r, column=0, columnspan=4)

    def SetFloppy (self, iDisk):
        fn = askopenfilename (initialdir=self.diskdir,
                              filetypes=[('MFLOPPY files', '*.mfloppy'),
                                         ('All files', '*')]);
        if ( fn ):
            self.fdisk[iDisk].set (fn)
            self.diskdir = os.path.dirname (fn)

    def SetSiDisk (self, iDisk):
        fn = askopenfilename (initialdir=self.diskdir,
                              filetypes=[('MFLOPPY files', '*.mfloppy'),
                                         ('All files', '*')]);
        if ( fn ):
            self.sdisk[iDisk].set (fn)
            self.diskdir = os.path.dirname (fn)

    def OnOK (self):
        self.win.destroy ()

    def SetConfig (self, l):
        args = l[0]
        i = l[1]
        if ( args[i] == '-sdx-tracks' ):
            i += 1
            if ( i < len (args) ):
                self.tracks[0].set (args[i])
                l[1] = i
                return True
        elif ( args[i] == '-sdx-mfloppy' ):
            i += 1
            if ( i < len (args) ):
                self.fdisk[0].set (args[i])
                l[1] = i
                return True
        elif ( args[i] == '-sdx-tracks2' ):
            i += 1
            if ( i < len (args) ):
                self.tracks[1].set (args[i])
                l[1] = i
                return True
        elif ( args[i] == '-sdx-mfloppy2' ):
            i += 1
            if ( i < len (args) ):
                self.fdisk[1].set (args[i])
                l[1] = i
                return True
        elif ( args[i] == '-sidisc-file' ):
            i += 1
            if ( i < len (args) ):
                iDisk = int (args[i])
                i += 1
                if ( ( i < len (args) ) and ( iDisk >= 1 ) and ( iDisk <= 4 ) ):
                    self.sdisk[iDisk-1].set (args[i])
                    l[1] = i
                    return True
        return False

    def GetConfig (self):
        margs = []
        fn = self.fdisk[0].get ()
        if ( fn ):
            margs.extend (['-sdx-tracks', self.tracks[0].get (), '-sdx-mfloppy', fn])
        fn = self.fdisk[1].get ()
        if ( fn ):
            margs.extend (['-sdx-tracks2', self.tracks[1].get (), '-sdx-mfloppy2', fn])
        for iDisk in range (4):
            fn = self.sdisk[iDisk].get ()
            if ( fn ):
                margs.extend (['-sidisc-file', str (iDisk+1), fn])
        return margs

class Launcher:
    def __init__ (self, args):
        self.ParseArgs (args)
        self.root = Tk ()
        self.root.resizable (width=True, height=False)
        self.root.grid_columnconfigure (0, weight=0)
        self.root.grid_columnconfigure (1, weight=1)
        self.root.title ('MEMU Launcher v0.12')
        self.roms = ROMS (self.root)
        self.disks = DISKS (self.root)
        self.address = Memory (self.root)
        self.inout = InOut (self.root)
        r = 0
        Label (self.root, text='Emulation').grid (row=r, column=0, sticky='E')
        self.em_mode = StringVar ()
        OptionMenu (self.root, self.em_mode, 'MTX', 'SDX', 'CPM', 'FDXB',
                    command=self.SetEmMode).grid (row=r, column=1, sticky='W')
        r += 1
        Label (self.root, text='Speed').grid (row=r, column=0, sticky='E')
        self.speed = StringVar ()
        Spinbox (self.root, textvariable=self.speed, values=spdvals, width=6
                 ).grid (row=r, column=1, sticky='W')
        r += 1
        Label (self.root, text='Memory (KB)').grid (row=r, column=0, sticky='E')
        self.mem = StringVar ()
        Spinbox (self.root, textvariable=self.mem, from_=32, to=512, increment=32, width=6
                 ).grid (row=r, column=1, sticky='W')
        r += 1
        Label (self.root, text='Video size').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        self.vid = StringVar ()
        Spinbox (f, textvariable=self.vid, from_=0, to=5, width=6).pack (side=LEFT)
        self.hwpal = IntVar ()
        Checkbutton (f, text='Hardware Palette', variable=self.hwpal).pack (side=LEFT)
        r += 1
        Label (self.root, text='Monitor size').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        self.mon = StringVar ()
        Spinbox (f, textvariable=self.mon, from_=0, to=5, width=6).pack (side=LEFT)
        self.mono = IntVar ()
        Checkbutton (f, variable=self.mono, text='Mono').pack (side=LEFT)
        self.mon_ninit = IntVar ()
        Checkbutton (f, variable=self.mon_ninit, text='Ignore Init').pack (side=LEFT)
        r += 1
        Label (self.root, text='Text Monitor').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='W')
        self.tmode = IntVar ()
        Radiobutton (f, text='None', variable=self.tmode, value=0).pack (side=LEFT)
        Radiobutton (f, text='TH', variable=self.tmode, value=1).pack (side=LEFT)
        Radiobutton (f, text='Console', variable=self.tmode, value=2).pack (side=LEFT)
        Radiobutton (f, text='No-key', variable=self.tmode, value=3).pack (side=LEFT)
        r += 1
        Label (self.root, text='Sound').grid (row=r,column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        self.snd = IntVar ()
        Checkbutton (f, variable=self.snd).pack (side=LEFT)
        self.latency = StringVar ()
        Label (f, text='Latency').pack (side=LEFT)
        Entry (f, textvariable=self.latency, width=10).pack (side=LEFT)
        r += 1
        Label (self.root, text='Keyboard').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        self.kbd = IntVar ()
        Checkbutton (f, text='Remap', variable=self.kbd).pack (side=LEFT)
        Label (f, text='     Country').pack (side=LEFT)
        self.country = StringVar ()
        Spinbox (f, textvariable=self.country, from_=0, to=3, width=2).pack (side=LEFT)
        r += 1
        Label (self.root, text='Joystick').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        self.joy = IntVar ()
        Checkbutton (f, variable=self.joy).pack (side=LEFT)
        Label (f, text='Centre').pack (side=LEFT)
        self.joycen = StringVar ()
        Spinbox (f, textvariable=self.joycen, values=joyvals, width=7).pack (side=LEFT)
        r += 1
        Label (self.root, text='Tape Directory').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        Button (f, text='...', command=self.BrowseTape).pack (side=RIGHT)
        self.tape = StringVar ()
        Entry (f, textvariable=self.tape).pack (side=LEFT, fill=X, expand=1)
        r += 1
        Label (self.root, text='Tape').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        self.tape_ovr = IntVar ()
        Checkbutton (f, text='Overwrite', variable=self.tape_ovr).pack (side=LEFT)
        self.tape_dis = IntVar ()
        Checkbutton (f, text='Disable', variable=self.tape_dis).pack (side=LEFT)
        r += 1
        Label (self.root, text='ZX Spectrum Tape').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        Button (f, text='...', command=self.BrowseZXTape).pack (side=RIGHT)
        self.zx_tape = StringVar ()
        Entry (f, textvariable=self.zx_tape).pack (side=LEFT, fill=X, expand=1)
        r += 1
        Label (self.root, text='ZX Spectrum Snapshot').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        Button (f, text='...', command=self.BrowseZXSnap).pack (side=RIGHT)
        self.zx_snap = StringVar ()
        Entry (f, textvariable=self.zx_snap).pack (side=LEFT, fill=X, expand=1)
        r += 1
        Label (self.root, text='Drive A Directory').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        Button (f, text='...', command=self.BrowseDriveA).pack (side=RIGHT)
        self.drivea = StringVar ()
        Entry (f, textvariable=self.drivea).pack (side=LEFT, fill=X, expand=1)
        r += 1
        Label (self.root, text='Drive A').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        self.daic = IntVar ()
        Checkbutton (f, variable=self.daic, text='Invert Case').pack (side=LEFT)
        self.daoh = IntVar ()
        Checkbutton (f, variable=self.daoh, text='Open Hack').pack (side=LEFT)
        r += 1
        Label (self.root, text='Advanced').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        Button (f, text='ROMs', command=self.roms.Show).pack (side=LEFT)
        Button (f, text='DISKs', command=self.disks.Show).pack (side=LEFT)
        Button (f, text='Memory', command=self.address.Show).pack (side=LEFT)
        Button (f, text='In/Out', command=self.inout.Show).pack (side=LEFT)
        r += 1
        Label (self.root, text='Run File').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        Button (f, text='...', command=self.BrowseRun).pack (side=RIGHT)
        self.run = StringVar ()
        Entry (f, textvariable=self.run).pack (side=LEFT, fill=X, expand=1)
        r += 1
        Label (self.root, text='Command Tail').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        Button (f, text='...', command=self.BrowseCTail).pack (side=RIGHT)
        self.ctail = StringVar ()
        Entry (f, textvariable=self.ctail).pack (side=LEFT, fill=X, expand=1)
        r += 1
        Label (self.root, text='Configuration').grid (row=r, column=0, sticky='E')
        f = Frame (self.root)
        f.grid (row=r, column=1, sticky='EW')
        Button (f, text='Load', command=self.OnLoad).pack (side=LEFT)
        Button (f, text='Save', command=self.OnSave).pack (side=LEFT)
        Button (f, text='Reset', command=self.OnReset).pack (side=LEFT)
        r += 1
        f = LabelFrame (self.root, pady=5)
        f.grid (row=r, column=0, columnspan=2, sticky='EW')
        f.grid_columnconfigure (0, weight=1)
        f.grid_columnconfigure (1, weight=1)
        Button (f, text='Close', command=self.Quit).grid (row=0, column=0)
        Button (f, text='Launch', command=self.Launch).grid (row=0, column=1)
        self.ResetBase ()
        self.ReadCfg (self.cfg2)
        if ( self.set_run ):
            self.run.set (self.set_run)
            if ( self.set_ctail ):
                self.ctail.set (' '.join (self.set_ctail))
        self.root.mainloop ()

    def ResetBase (self):
        self.em_mode.set ('MTX')
        self.speed.set ('4MHz')
        self.mem.set (64)
        self.vid.set (0)
        self.hwpal.set (0)
        self.mon.set (0)
        self.mono.set (0)
        self.mon_ninit.set (0)
        self.tmode.set (0)
        self.snd.set (0)
        self.latency.set ('')
        self.kbd.set (0)
        self.country.set (0)
        self.joy.set (0)
        self.joycen.set (joyvals[0])
        self.tape.set ('')
        self.tape_ovr.set (0)
        self.tape_dis.set (0)
        self.zx_tape.set ('')
        self.zx_snap.set ('')
        self.drivea.set ('')
        self.daic.set (0)
        self.daoh.set (0)
        self.run.set ('')
        self.ctail.set ('')

    def Reset (self):
        self.ResetBase ()
        self.roms.Reset ()
        self.disks.Reset ()
        self.address.Reset ()
        self.inout.Reset ()

    def GetProgDir (self, arg0):
        (pdir, name) = os.path.split (arg0)
        if ( pdir ):
            return os.path.abspath (pdir)
        if ( os.path.exists (name) ):
            return os.getcwd ()
        for pdir in os.getenv ('PATH').split (os.pathsep):
            if ( os.path.exists (os.path.join (pdir, name)) ):
                return os.path.abspath (pdir)
        return None

    def GetConfigFile (self, arg0):
        name, ext = os.path.splitext (os.path.basename (arg0))
        name += '.cfg'
        if ( os.path.exists (name) ):
            return (os.path.abspath (name))
        if ( self.pdir ):
            return os.path.join (self.pdir, name)
        return None

    def FindMemu (self):
        memu = os.path.join (self.pdir, 'memu.exe')
        if ( os.path.exists (memu) ):
            return memu
        memu = os.path.join (self.pdir, 'memu')
        if ( os.path.exists (memu) ):
            return memu
        for pdir in os.getenv ('PATH').split (os.pathsep):
            memu = os.path.join (pdir, 'memu.exe')
            if ( os.path.exists (memu) ):
                return os.path.abspath (memu)
            memu = os.path.join (pdir, 'memu')
            if ( os.path.exists (memu) ):
                return os.path.abspath (memu)
        return 'memu'

    def ParseArgs (self, args):
        self.debug = False
        self.pdir = self.GetProgDir (args[0])
        self.cfg  = self.GetConfigFile (args[0])
        self.cfg2 = self.cfg
        self.memu = self.FindMemu ()
        self.set_run = None
        self.set_ctail = []
        for arg in args[1:]:
            if ( arg == '-debug' ):
                self.debug = True
            elif ( arg.lower ().endswith ('.cfg') ):
                self.cfg2 = arg
            elif ( arg.lower ().find ('memu') >= 0 ):
                self.memu = os.path.abspath (arg)
            elif ( not self.set_run ):
                self.set_run = arg
            else:
                self.set_ctail.append (arg)

    def SetEmMode (self, em):
        if ( em == 'FDXB' ):
            if ( not self.run.get () ):
                self.run.set ('FDXB.COM')

    def BrowseTape (self):
        dir = askdirectory ()
        if ( dir ):
            self.tape.set (dir)

    def BrowseZXTape (self):
        defdir = self.tape.get ()
        if ( not defdir ):
            defdir = "."
        fn = askopenfilename (initialdir=defdir,
                              filetypes=[('ZX Tape files', '*.tap'),
                                         ('All files', '*')]);
        if ( fn ):
            self.zx_tape.set (fn)

    def BrowseZXSnap (self):
        defdir = self.tape.get ()
        if ( not defdir ):
            defdir = "."
        fn = askopenfilename (initialdir=defdir,
                              filetypes=[('ZX Snapshot files', '*.sna'),
                                         ('All files', '*')]);
        if ( fn ):
            self.zx_snap.set (fn)

    def BrowseDriveA (self):
        dir = askdirectory ()
        if ( dir ):
            self.drivea.set (dir)

    def BrowseRun (self):
        defdir = self.tape.get ()
        if ( not defdir ):
            defdir = self.drivea.get ()
            if ( not defdir ):
               defdir = "."
        fn = askopenfilename (initialdir=defdir,
                              filetypes=[('Tape files', '*.mtx'),
                                         ('RUN files', '*.run'),
                                         ('CPM files', '*.com'),
                                         ('All files', '*')]);
        if ( fn ):
            if ( fn[-4:].lower () == '.mtx' ):
                tape_dn = self.tape.get ()
                if ( not tape_dn ):
                   (tape_dn, fn) = os.path.split (fn)
                   self.tape.set (tape_dn)
            elif ( fn[-4:].lower () == '.com' ):
                a_dn = self.tape.get ()
                if ( not a_dn ):
                   a_dn = os.path.dirname (fn)
                   self.drivea.set (a_dn)
            self.run.set (fn)

    def BrowseCTail (self):
        defdir = self.tape.get ()
        if ( not defdir ):
            defdir = self.drivea.get ()
            if ( not defdir ):
               defdir = "."
        fn = askopenfilename (initialdir=defdir,
                              filetypes=[('CPM files', '*.com'),
                                         ('BASIC files', '*.bas'),
                                         ('All files', '*')]);
        if ( fn ):
            fn = os.path.basename (fn)
            ct = self.ctail.get ()
            if ( ct ):
                self.ctail.set (ct + ' ' + fn)
            else:
                self.ctail.set (fn)

    def WriteCfg (self, sFile):
        if ( sFile ):
           f = open (sFile, 'w')
           f.write (self.margs[1])
           for ma in self.margs[2:self.nswitch]:
               if ( ma[0] == '-' ):
                   f.write ('\n' + ma)
               elif ( ma.find (' ') >= 0 ):
                   f.write (' "' + ma + '"')
               else:
                   f.write (' ' + ma)
           f.write ('\n')
           if ( len (self.margs) > self.nswitch ):
               if ( self.margs[self.nswitch].find (' ') >= 0 ):
                   f.write ('"' + self.margs[self.nswitch] + '"')
               else:
                   f.write (self.margs[self.nswitch])
               for ma in self.margs[self.nswitch+1:]:
                   if ( ma.find (' ') >= 0 ):
                       f.write (' "' + ma + '"')
                   else:
                       f.write (' ' + ma)
               f.write ('\n')
           f.close ()

    def Parse (self, text):
        margs = []
        even = True
        for part in text.split ('"'):
            if ( even ):
                margs.extend (part.split ())
            else:
                margs.append (part)
            even = not even
        return margs

    def ReadCfg (self, sConfig):
        if ( ( sConfig ) and ( os.path.exists (sConfig) ) ):
            f = open (sConfig, 'r')
            margs = self.Parse (f.read ())
            f.close ()
            self.Reset ()
            i = 0
            while ( i < len (margs) ):
                l = [margs, i]
                if ( margs[i] == '-sdx' ):
                    self.em_mode.set ('SDX')
                elif ( margs[i] == '-cpm' ):
                    self.em_mode.set ('CPM')
                elif ( margs[i] == '-fdxb' ):
                    self.em_mode.set ('FDXB')
                elif ( margs[i] == '-fast' ):
                    self.speed.set ('Fast')
                elif ( margs[i] == '-speed' ):
                    if ( i+1 < len (margs) ):
                        i += 1
                        self.speed.set ('%dMHz' % (int (margs[i]) // 1000000 ))
                elif ( margs[i] == '-mem-blocks' ):
                    if ( i+1 < len (margs) ):
                        i += 1
                        self.mem.set (str (16 * int (margs[i])))
                elif ( margs[i] == '-vid-win' ):
                    if ( self.vid.get () == '0' ):
                        self.vid.set ('1')
                elif ( margs[i] == '-vid-win-big' ):
                    self.vid.set (str (int (self.vid.get ()) + 1))
                elif ( margs[i] == '-vid-win-hw-palette' ):
                    self.hwpal.set (1)
                elif ( margs[i] == '-mon-win' ):
                    if ( self.mon.get () == '0' ):
                        self.mon.set ('1')
                elif ( margs[i] == '-mon-win-big' ):
                    self.mon.set (str (int (self.mon.get ()) + 1))
                elif ( margs[i] == '-mon-win-mono' ):
                    self.mono.set (1)
                elif ( margs[i] == '-mon-ignore-init' ):
                    self.mon_ninit.set (1)
                elif ( margs[i] == '-mon-th' ):
                    self.tmode.set (1)
                elif ( margs[i] == '-mon-console' ):
                    self.tmode.set (2)
                elif ( margs[i] == '-mon-console-nokey' ):
                    self.tmode.set (3)
                elif ( margs[i] == '-snd-portaudio' ):
                    self.snd.set (1)
                elif ( margs[i] == '-snd-latency' ):
                    if ( i+1 < len (margs) ):
                        self.latency.set (margs[i+1])
                        i += 1
                elif ( margs[i] == '-kbd-remap' ):
                    self.kbd.set (1)
                elif ( margs[i] == '-kbd-country' ):
                    if ( i+1 < len (margs) ):
                        self.country.set (margs[i+1])
                        i += 1
                elif ( margs[i] == '-joy' ):
                    self.joy.set (1)
                elif ( margs[i] == '-joy-central' ):
                    if ( i+1 < len (margs) ):
                        self.country.set (margs[i+1] + '%')
                        i += 1
                elif ( margs[i] == '-tape-dir' ):
                    if ( i+1 < len (margs) ):
                        self.tape.set (margs[i+1])
                        i += 1
                elif ( margs[i] == '-tape-overwrite' ):
                    self.tape_ovr.set (1)
                elif ( margs[i] == '-tape-disable' ):
                    self.tape_dis.set (1)
                elif ( margs[i] == '-tap-file' ):
                    if ( i+1 < len (margs) ):
                        self.zx_tape.set (margs[i+1])
                        i += 1
                elif ( margs[i] == '-sna-file' ):
                    if ( i+1 < len (margs) ):
                        self.zx_snap.set (margs[i+1])
                        i += 1
                elif ( margs[i] == '-cpm-drive-a' ):
                    if ( i+1 < len (margs) ):
                        self.drivea.set (margs[i+1])
                        i += 1
                elif ( margs[i] == '-cpm-invert-case' ):
                    self.daic.set (1)
                elif ( margs[i] == '-cpm-open-hack' ):
                    self.daoh.set (1)
                elif ( self.address.SetConfig (l) ):
                    i = l[1]
                elif ( self.roms.SetConfig (l) ):
                    i = l[1]
                elif ( self.disks.SetConfig (l) ):
                    i = l[1]
                elif ( self.inout.SetConfig (l) ):
                    i = l[1]
                elif (margs[i][0] != '-' ):
                    break
                i += 1
            if ( i < len (margs) ):
                self.run.set (margs[i])
                i += 1
            if ( i < len (margs) ):
                self.ctail.set (margs[i])
                i += 1
        if ( ( self.vid.get () == '0' ) and ( self.mon.get () == '0' ) and
             ( self.tmode.get () == 0 ) ):
            self.vid.set ('1')

    def GetConfig (self):
        margs = [self.memu]
        em = self.em_mode.get ()
        if ( em == 'SDX' ):
            margs.append ('-sdx')
        elif ( em == 'CPM' ):
            margs.append ('-cpm')
        elif ( em == 'FDXB' ):
            margs.append ('-fdxb')
        speed = self.speed.get ()
        if ( speed == 'Fast' ):
            margs.append ('-fast')
        elif ( speed != '4MHz' ):
            margs.extend (['-speed', speed.replace ('MHz', '000000')])
        nBlocks = int (self.mem.get ()) // 16
        if ( ( nBlocks < 4 ) and ( ( em == 'CPM' ) or ( em == 'FDXB' ) ) ):
            showwarning ('CP/M Emulation',
                         'CP/M emulation modes require at least 64KB of memory.\n' +
                         'Please increase the memory allocation.')
            return False
        margs.extend (['-mem-blocks', str (nBlocks)])
        vsize = int (self.vid.get ())
        if ( vsize > 0 ):
            margs.append ('-vid-win')
            for i in range (vsize -1):
                margs.append ('-vid-win-big')
        if ( self.hwpal.get () ):
            margs.append ('-vid-win-hw-palette')
        msize = int (self.mon.get ())
        if ( msize > 0 ):
            margs.append ('-mon-win')
            for i in range (msize - 1):
                margs.append ('-mon-win-big')
        if ( self.mono.get () ):
            margs.append ('-mon-win-mono')
        if ( self.mon_ninit.get () ):
            margs.append ('-mon-ignore-init')
        tm = self.tmode.get ()
        if ( tm == 1 ):
            margs.append ('-mon-th')
        elif ( tm == 2 ):
            margs.append ('-mon-console')
        elif ( tm == 3 ):
            margs.append ('-mon-console-nokey')
        if ( ( vsize == 0 ) and ( msize == 0 ) and ( tm == 0 ) ):
            showwarning ('Display',
                         'No display output has been selected.\n' +
                         'Please select a non-zero video or monitor size, ' +
                         'or enable text console output.')
            return False
        if ( self.snd.get () ):
            margs.append ('-snd-portaudio')
        latency = self.latency.get ()
        if ( latency ):
            margs.extend (['-snd-latency', latency])
        if ( self.kbd.get () ):
            margs.append ('-kbd-remap')
        country = self.country.get ()
        if ( country != '0' ):
            margs.extend (['-kbd-country', country])
        if ( self.joy.get () ):
            margs.append ('-joy')
        joycen = self.joycen.get ()
        if ( joycen != joyvals[0] ):
            margs.extend (['-joy-central', joycen.replace ('%', '')])
        tape_dn = self.tape.get ()
        if ( tape_dn ):
            margs.extend (['-tape-dir', tape_dn])
        if ( self.tape_ovr.get () ):
            margs.append ('-tape-overwrite')
        zx_tape_fn = self.zx_tape.get ()
        if ( zx_tape_fn ):
            margs.extend (['-tap-file', zx_tape_fn])
        zx_snap_fn = self.zx_snap.get ()
        if ( zx_snap_fn ):
            margs.extend (['-sna-file', zx_snap_fn])
        if ( self.tape_dis.get () ):
            if ( tape_dn or zx_tape_fn or zx_snap_fn ):
                showwarning ('Tape Emulation',
                             'A tape has been requested, but tape emulation is disabled.\n' +
                             'Please enable tape support.')
                return False
            margs.append ('-tape-disable')
        drivea_dn = self.drivea.get ()
        if ( drivea_dn ):
            if ( em == 'MTX' ):
                showwarning ('CP/M Emulation',
                             'A drive A folder has been selected,\n' +
                             'but no software disk emulation.')
                return False
            margs.extend (['-cpm-drive-a', drivea_dn])
        elif ( em != 'MTX' ):
            showwarning ('CP/M Emulation',
                         'CP/M software emulation requested.\n' +
                         'Please specify folder to use as A drive.')
            return False
        if ( self.daic.get () ):
            margs.append ('-cpm-invert-case')
        if ( self.daoh.get () ):
            margs.append ('-cpm-open-hack')
        roms = self.roms.GetConfig ()
        margs.extend (roms)
        addr = self.address.GetConfig ()
        margs.extend (addr)
        disks = self.disks.GetConfig ()
        if ( len (disks) > 0 ):
            if ( em != 'MTX' ):
                showwarning ('Disk Emulation',
                             'Use of disk images requires hardware disk emulation,\n' +
                             'but you have selected a software disk emulation mode.')
                return False
            if ( ( len (roms) == 0 ) and ( '-mem' not in addr ) ):
                showwarning ('Disk Emulation',
                             'Use of disk images requires an SDX or CP/M ROM.\n' +
                             'Please select an appropriate ROM image.')
                return False
        margs.extend (disks)
        margs.extend (self.inout.GetConfig ())
        nswitch = len (margs)
        run_fn = self.run.get ()
        if ( em == 'FDXB' ):
            if ( not run_fn ):
                run_fn = 'FDXB.COM'
            if ( not os.path.exists (run_fn) ):
                run_fn = os.path.join (drivea_dn, run_fn)
            if ( not os.path.exists (run_fn) ):
                showwarning ('FDXB Emulation',
                             'Unable to locate FDXB.COM.\n' +
                             'Please specify this as "Run File".')
                return False
        if ( run_fn ):
            if ( len (disks) > 0 ):
                showwarning ('Disk Emulation',
                             'Use of disk images not generally compatible with a run file.\n' +
                             'Please select a software disk emulation or remove the run file.')
                return False
            if ( ( tape_dn ) and ( run_fn[-4:].lower () == '.mtx' ) ):
                run_dn = os.path.dirname (run_fn)
                if ( run_dn ):
                    run_fn = os.path.relpath (run_fn, tape_dn)
            margs.append (run_fn)
        ct = self.ctail.get ()
        if ( ct ):
            if ( not run_fn.lower ().endswith ('.com') ):
                showwarning ('CPM Emulation',
                             'Command tail specified with no CP/M program.\n' +
                             'Please specifiy CP/M program to run as "Run File".')
                return False
            margs.append (ct)
        self.margs = margs
        self.nswitch = nswitch
        return True

    def OnLoad (self):
        fn = askopenfilename (initialdir=self.pdir,
                              filetypes=[('Config files', '*.cfg'),
                                         ('All files', '*')]);
        if ( fn ):
            self.ReadCfg (fn)

    def OnSave (self):
        if ( self.GetConfig () ):
            fn = asksaveasfilename (initialdir=self.pdir,
                                    defaultextension='.cfg',
                                    filetypes=[('Config files', '*.cfg'),
                                               ('All files', '*')]);
            if ( fn ):
                self.WriteCfg (fn)

    def OnReset (self):
        if ( askyesno ('Confirm Reset',
                       'This will reset the configuration to a basic MTX512.\n' +
                       'Are you sure?') ):
            self.Reset ()
            self.vid.set ('1')

    def Launch (self):
        if ( not self.GetConfig () ):
            return
        self.WriteCfg (self.cfg)
        if ( self.debug ):
            print (self.margs)
        else:
            try:
                subprocess.Popen (self.margs)
            except OSError as e:
                showerror ('Launch MEMU',
                           ( 'An error occured attempting to launch MEMU as:\n' +
                             '%s\n\n%s' ) %
                           (self.margs[0], str (e)))
        self.root.destroy ()

    def Quit (self):
        self.root.destroy ()

Launcher (sys.argv)
