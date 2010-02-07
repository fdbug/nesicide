//    NESICIDE - an IDE for the 8-bit NES.  
//    Copyright (C) 2009  Christopher S. Pow

//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.

//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
// ROM.cpp: implementation of the CROM class.
//
//////////////////////////////////////////////////////////////////////

#include "cnesrom.h"
#include "cnesppu.h"
#include "cnes6502.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

unsigned char  CROM::m_SRAMmemory [] = { 0, };
unsigned char  CROM::m_EXRAMmemory [] = { 0, };
unsigned char  CROM::m_PRGROMmemory [][ MEM_8KB ] = { { 0, }, };
char*          CROM::m_PRGROMdisassembly [][ MEM_8KB ] = { { 0, }, };
unsigned short CROM::m_PRGROMsloc2addr [][ MEM_8KB ] = { { 0, }, };
int            CROM::m_PRGROMsloc [] = { 0, };
unsigned char  CROM::m_CHRROMmemory [][ MEM_8KB ] = { { 0, }, };
unsigned char  CROM::m_CHRRAMmemory [ MEM_8KB ] = { 0, };
UINT           CROM::m_PRGROMbank [] = { 0, 0, 0, 0 };
unsigned char* CROM::m_pPRGROMmemory [] = { NULL, NULL, NULL, NULL };
unsigned char* CROM::m_pCHRmemory [] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
unsigned char* CROM::m_pSRAMmemory = m_SRAMmemory; 

UINT           CROM::m_mapper = 0; 
UINT           CROM::m_numPrgBanks = 0;
UINT           CROM::m_numChrBanks = 0;

CCodeDataLogger* CROM::m_pLogger [] = { NULL, };
CRegisterData**  CROM::m_tblRegisters = NULL;
int              CROM::m_numRegisters = 0;

static CROM __init __attribute((unused));

CROM::CROM()
{
   int bank;

   for ( bank = 0; bank < NUM_ROM_BANKS; bank++ )
   {
      m_pLogger [ bank ] = new CCodeDataLogger ();
   }


   CROM::RESET ();
}

CROM::~CROM()
{
   int bank;
   int addr;
   char* val = 0;

   for ( bank = 0; bank < NUM_ROM_BANKS; bank++ )
   {
      delete m_pLogger [ bank ];

      // Blow away disassembly strings...
      for ( addr = 0 ; addr < MEM_8KB; addr++ )
      {
         if ( val != m_PRGROMdisassembly[bank][addr] )
         {
            val = m_PRGROMdisassembly[bank][addr];
            delete [] m_PRGROMdisassembly[bank][addr];
         }
      }
   }
}

void CROM::Clear16KBanks ()
{
   unsigned int bank;
   int addr;
   char* val = 0;

   for ( bank = 0; bank < m_numPrgBanks; bank++ )
   {
      // Blow away disassembly strings...
      for ( addr = 0 ; addr < MEM_8KB; addr++ )
      {
         if ( val != m_PRGROMdisassembly[bank][addr] )
         {
            val = m_PRGROMdisassembly[bank][addr];
            delete [] m_PRGROMdisassembly[bank][addr];
         }
      }
   }

   m_numPrgBanks = 0;
}

void CROM::Set16KBank ( int bank, unsigned char* data )
{
   memcpy ( m_PRGROMmemory[m_numPrgBanks], data, MEM_8KB );
   C6502::Disassemble(m_PRGROMdisassembly[m_numPrgBanks],m_PRGROMmemory[m_numPrgBanks],MEM_8KB,m_PRGROMsloc2addr[m_numPrgBanks],&(m_PRGROMsloc[m_numPrgBanks]),true);
   m_numPrgBanks++;
   memcpy ( m_PRGROMmemory[m_numPrgBanks], data+MEM_8KB, MEM_8KB );
   C6502::Disassemble(m_PRGROMdisassembly[m_numPrgBanks],m_PRGROMmemory[m_numPrgBanks],MEM_8KB,m_PRGROMsloc2addr[m_numPrgBanks],&(m_PRGROMsloc[m_numPrgBanks]),true);
   m_numPrgBanks++;
}

void CROM::Set8KBank ( int bank, unsigned char* data )
{
   memcpy ( m_CHRROMmemory[bank], data, MEM_8KB );
   m_numChrBanks = bank + 1;
}

void CROM::DoneLoadingBanks ()
{
   // This is called when the ROM loader is done so that fixup can be done...
   if ( m_numPrgBanks == 2 )
   {
      // If the ROM contains only one 16KB PRG-ROM bank then it needs to be replicated
      // to the second PRG-ROM bank slot...
      memcpy ( m_PRGROMmemory[2], m_PRGROMmemory[0], MEM_8KB );
      C6502::Disassemble(m_PRGROMdisassembly[2],m_PRGROMmemory[2],MEM_8KB,m_PRGROMsloc2addr[2],&(m_PRGROMsloc[2]),true);
      m_numPrgBanks++;
      memcpy ( m_PRGROMmemory[3], m_PRGROMmemory[1], MEM_8KB );
      C6502::Disassemble(m_PRGROMdisassembly[3],m_PRGROMmemory[3],MEM_8KB,m_PRGROMsloc2addr[3],&(m_PRGROMsloc[3]),true);
      m_numPrgBanks++;
   }
}

void CROM::RESET ()
{
   int bank;

   m_mapper = 0;

   m_pPRGROMmemory [ 0 ] = m_PRGROMmemory [ 0 ];
   m_PRGROMbank [ 0 ] = 0;
   m_pPRGROMmemory [ 1 ] = m_PRGROMmemory [ 1 ];
   m_PRGROMbank [ 1 ] = 1;
   m_pPRGROMmemory [ 2 ] = m_PRGROMmemory [ 2 ];
   m_PRGROMbank [ 2 ] = 2;
   m_pPRGROMmemory [ 3 ] = m_PRGROMmemory [ 3 ];
   m_PRGROMbank [ 3 ] = 3;

   // Assume no VROM...point to VRAM...
   for ( bank = 0; bank < 8; bank++ )
   {
      m_pCHRmemory [ bank ] = m_CHRRAMmemory + (bank<<UPSHIFT_1KB);
   }

   // If the cartridge has VROM, map it instead...
   if ( m_numChrBanks > 0 )
   {
      for ( bank = 0; bank < 8; bank++ )
      {
         m_pCHRmemory [ bank ] = m_CHRROMmemory [ 0 ] + (bank<<UPSHIFT_1KB);
      }
   }

   // Clear Code/Data Logger info...
   for ( bank = 0; bank < NUM_ROM_BANKS; bank++ )
   {
      m_pLogger [ bank ]->ClearData ();
   }
}

void CROM::LOAD ( MapperState* data )
{
   int idx;

   for ( idx = 0; idx < 4; idx++ )
   {
      m_pPRGROMmemory [ idx ] = (unsigned char*)m_PRGROMmemory + data->PRGROMOffset [ idx ];
   }
   for ( idx = 0; idx < 8; idx++ )
   {
      if ( m_numChrBanks )
      {
         m_pCHRmemory [ idx ] = (unsigned char*)m_CHRROMmemory + data->CHRMEMOffset [ idx ];
      }
      else
      {
         m_pCHRmemory [ idx ] = (unsigned char*)m_CHRRAMmemory + data->CHRMEMOffset [ idx ];
      }
   }
}

void CROM::SAVE ( MapperState* data )
{
   int idx;

   for ( idx = 0; idx < 4; idx++ )
   {
      data->PRGROMOffset [ idx ] = m_pPRGROMmemory [ idx ] - (unsigned char*)m_PRGROMmemory;
   }
   for ( idx = 0; idx < 8; idx++ )
   {
      if ( m_numChrBanks )
      {
         data->CHRMEMOffset [ idx ] = m_pCHRmemory [ idx ] - (unsigned char*)m_CHRROMmemory;
      }
      else
      {
         data->CHRMEMOffset [ idx ] = m_pCHRmemory [ idx ] - (unsigned char*)m_CHRRAMmemory;
      }
   }
}
