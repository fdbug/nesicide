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

#include "cnesapu.h"
#include "cnes6502.h"
#include "cnesppu.h"

FILE* wavOut = NULL;
int   wavOutFileNum = 1;
char wavOutName [ 100 ];
int wavFileSize;
typedef struct
{
   char ftype[4];
   int  len;
   char ctype[4];
} wavHdr;

typedef struct
{
   char ctype[4];
   int  len;
} wavChunkHdr;

typedef struct
{
   short ccode;
   short chans;
   int   rate;
   int   Bps;
   short align;
   short bps;
} wavFmtChunk;

wavHdr wavOutHdr;
wavChunkHdr wavChunk;
wavFmtChunk wavFmt;

// APU Event breakpoints
bool apuIRQEvent(BreakpointInfo* pBreakpoint,int data)
{
   // This breakpoint is checked in the right place
   // so if this breakpoint is enabled it should always fire when called.
   return true;
}

bool apuLengthCounterClockedEvent(BreakpointInfo* pBreakpoint,int data)
{
   // This breakpoint is checked in the right place
   // so if this breakpoint is enabled it should always fire when called.
   return true;
}

bool apuDMAEvent(BreakpointInfo* pBreakpoint,int data)
{
   // This breakpoint is checked in the right place
   // so if this breakpoint is enabled it should always fire when called.
   return true;
}

static CBreakpointEventInfo* tblAPUEvents [] =
{
   new CBreakpointEventInfo("IRQ", apuIRQEvent, 0, "Break if APU asserts IRQ", 10),
   new CBreakpointEventInfo("DMC channel DMA", apuDMAEvent, 0, "Break if APU DMC channel DMA occurs", 10),
   new CBreakpointEventInfo("Length Counter Clocked", apuLengthCounterClockedEvent, 0, "Break if APU sequencer clocks Length Counter", 10),
};

// APU Registers
static CBitfieldData* tblAPUSQCTRLBitfields [] =
{
   new CBitfieldData("Duty Cycle", 6, 2, "%X", 4, "25%", "50%", "75%", "12.5%"),
   new CBitfieldData("Channel State", 5, 1, "%X", 2, "Running", "Halted"),
   new CBitfieldData("Envelope Enabled", 4, 1, "%X", 2, "No", "Yes"),
   new CBitfieldData("Volume/Envelope", 0, 4, "%X", 0)
};

static CBitfieldData* tblAPUSQSWEEPBitfields [] =
{
   new CBitfieldData("Sweep Enabled", 7, 1, "%X", 2, "No", "Yes"),
   new CBitfieldData("Sweep Divider", 4, 3, "%X", 0),
   new CBitfieldData("Sweep Direction", 3, 1, "%X", 2, "Down", "Up"),
   new CBitfieldData("Sweep Shift", 0, 3, "%X", 0)
};

static CBitfieldData* tblAPUPERIODLOBitfields [] =
{
   new CBitfieldData("Period Low-bits", 0, 8, "%02X", 0)
};

static CBitfieldData* tblAPUPERIODLENBitfields [] =
{
   new CBitfieldData("Length", 3, 5, "%X", 32, "0A","FE","14","02","28","04","50","06","A0","08","3C","0A","0E","0C","1A","0E","0C","10","18","12","30","14","60","16","C0","18","48","1A","10","1C","20","1E"),
   new CBitfieldData("Period High-bits", 0, 3, "%X", 0)
};

static CBitfieldData* tblAPUTRICTRLBitfields [] =
{
   new CBitfieldData("Channel State", 7, 1, "%X", 2, "Running", "Halted"),
   new CBitfieldData("Linear Counter Reload", 0, 6, "%X", 0)
};

static CBitfieldData* tblAPUDMZBitfields [] =
{
   new CBitfieldData("Unused", 0, 8, "%02X", 0),
};

static CBitfieldData* tblAPUNZCTRLBitfields [] =
{
   new CBitfieldData("Channel State", 5, 1, "%X", 2, "Running", "Halted"),
   new CBitfieldData("Envelope Enabled", 4, 1, "%X", 2, "No", "Yes"),
   new CBitfieldData("Volume/Envelope", 0, 4, "%X", 0)
};

static CBitfieldData* tblAPUNZPERIODBitfields [] =
{
   new CBitfieldData("Mode", 7, 1, "%X", 2, "32,767 samples", "93 samples"),
   new CBitfieldData("Period", 0, 4, "%X", 16, "004","008","010","020","040","060","080","0A0","0CA","0FE","17C","1FC","2FA","3F8","7F2","FE4")
};

static CBitfieldData* tblAPUNZLENBitfields [] =
{
   new CBitfieldData("Length", 3, 5, "%X", 32, "0A","FE","14","02","28","04","50","06","A0","08","3C","0A","0E","0C","1A","0E","0C","10","18","12","30","14","60","16","C0","18","48","1A","10","1C","20","1E")
};

static CBitfieldData* tblAPUDMCCTRLBitfields [] =
{
   new CBitfieldData("IRQ Enabled", 7, 1, "%X", 2, "No", "Yes"),
   new CBitfieldData("Loop", 6, 1, "%X", 2, "No", "Yes"),
   new CBitfieldData("Period", 0, 4, "%X", 16, "1AC","17C","154","140","11E","0FE","0E2","0D6","0BE","0A0","08E","080","06A","054","048","036")
};

static CBitfieldData* tblAPUDMCVOLBitfields [] =
{
   new CBitfieldData("Volume", 0, 7, "%02X", 0)
};

static CBitfieldData* tblAPUDMCADDRBitfields [] =
{
   new CBitfieldData("Sample Address", 0, 8, "%02X", 0)
};

static CBitfieldData* tblAPUDMCLENBitfields [] =
{
   new CBitfieldData("Sample Length", 0, 8, "%02X", 0)
};

static CBitfieldData* tblAPUCTRLBitfields [] =
{
   new CBitfieldData("DMC IRQ", 7, 1, "%X", 2, "Not Asserted", "Asserted"),
   new CBitfieldData("Frame IRQ", 6, 1, "%X", 2, "Not Asserted", "Asserted"),
   new CBitfieldData("Delta Modulation Channel", 4, 1, "%X", 2, "Disabled", "Enabled"),
   new CBitfieldData("Noise Channel", 3, 1, "%X", 2, "Disabled", "Enabled"),
   new CBitfieldData("Triangle Channel", 2, 1, "%X", 2, "Disabled", "Enabled"),
   new CBitfieldData("Square2 Channel", 1, 1, "%X", 2, "Disabled", "Enabled"),
   new CBitfieldData("Square1 Channel", 0, 1, "%X", 2, "Disabled", "Enabled"),
};

static CBitfieldData* tblAPUMASKBitfields [] =
{
   new CBitfieldData("Sequencer Mode", 7, 1, "%X", 2, "4-step", "5-step"),
   new CBitfieldData("IRQ", 6, 1, "%X", 2, "Enabled", "Disabled")
};

static CRegisterData* tblAPURegisters [] =
{
   new CRegisterData(0x4000, "SQ1CTRL", nesGetAPURegister, nesSetAPURegister, 4, tblAPUSQCTRLBitfields),
   new CRegisterData(0x4001, "SQ1SWEEP", nesGetAPURegister, nesSetAPURegister, 4, tblAPUSQSWEEPBitfields),
   new CRegisterData(0x4002, "SQ1PERLO", nesGetAPURegister, nesSetAPURegister, 1, tblAPUPERIODLOBitfields),
   new CRegisterData(0x4003, "SQ1PERLEN", nesGetAPURegister, nesSetAPURegister, 2, tblAPUPERIODLENBitfields),
   new CRegisterData(0x4004, "SQ2CTRL", nesGetAPURegister, nesSetAPURegister, 4, tblAPUSQCTRLBitfields),
   new CRegisterData(0x4005, "SQ2SWEEP", nesGetAPURegister, nesSetAPURegister, 4, tblAPUSQSWEEPBitfields),
   new CRegisterData(0x4006, "SQ2PERLO", nesGetAPURegister, nesSetAPURegister, 1, tblAPUPERIODLOBitfields),
   new CRegisterData(0x4007, "SQ2PERLEN", nesGetAPURegister, nesSetAPURegister, 2, tblAPUPERIODLENBitfields),
   new CRegisterData(0x4008, "TRICTRL", nesGetAPURegister, nesSetAPURegister, 2, tblAPUTRICTRLBitfields),
   new CRegisterData(0x4009, "TRIDMZ", nesGetAPURegister, nesSetAPURegister, 1, tblAPUDMZBitfields),
   new CRegisterData(0x400A, "TRIPERLO", nesGetAPURegister, nesSetAPURegister, 1, tblAPUPERIODLOBitfields),
   new CRegisterData(0x400B, "TRIPERLEN", nesGetAPURegister, nesSetAPURegister, 2, tblAPUPERIODLENBitfields),
   new CRegisterData(0x400C, "NOISECTRL", nesGetAPURegister, nesSetAPURegister, 3, tblAPUNZCTRLBitfields),
   new CRegisterData(0x400D, "NOISEDMZ", nesGetAPURegister, nesSetAPURegister, 1, tblAPUDMZBitfields),
   new CRegisterData(0x400E, "NOISEPERIOD", nesGetAPURegister, nesSetAPURegister, 2, tblAPUNZPERIODBitfields),
   new CRegisterData(0x400F, "NOISELEN", nesGetAPURegister, nesSetAPURegister, 1, tblAPUNZLENBitfields),
   new CRegisterData(0x4010, "DMCCTRL", nesGetAPURegister, nesSetAPURegister, 3, tblAPUDMCCTRLBitfields),
   new CRegisterData(0x4011, "DMCVOL", nesGetAPURegister, nesSetAPURegister, 1, tblAPUDMCVOLBitfields),
   new CRegisterData(0x4012, "DMCADDR", nesGetAPURegister, nesSetAPURegister, 1, tblAPUDMCADDRBitfields),
   new CRegisterData(0x4013, "DMCLEN", nesGetAPURegister, nesSetAPURegister, 1, tblAPUDMCLENBitfields),
   new CRegisterData(0x4014, "APUDMZ", nesGetAPURegister, nesSetAPURegister, 1, tblAPUDMZBitfields),
   new CRegisterData(0x4015, "APUCTRL", nesGetAPURegister, nesSetAPURegister, 7, tblAPUCTRLBitfields),
   new CRegisterData(0x4016, "APUDMZ", nesGetAPURegister, nesSetAPURegister, 1, tblAPUDMZBitfields),
   new CRegisterData(0x4017, "APUMASK", nesGetAPURegister, nesSetAPURegister, 2, tblAPUMASKBitfields),
};

static const char* rowHeadings [] =
{
   "4000","4004","4008","400C","4010","4014"
};

static const char* columnHeadings [] =
{
   "x0","x1","x2","x3"
};

static CRegisterDatabase* dbRegisters = new CRegisterDatabase(eMemory_IOregs,6,4,24,tblAPURegisters,rowHeadings,columnHeadings);

CRegisterDatabase* CAPU::m_dbRegisters = dbRegisters;

CBreakpointEventInfo** CAPU::m_tblBreakpointEvents = tblAPUEvents;
int32_t                CAPU::m_numBreakpointEvents = NUM_APU_EVENTS;

uint8_t CAPU::m_APUreg [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t CAPU::m_APUregDirty [] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
bool          CAPU::m_irqEnabled = false;
bool          CAPU::m_irqAsserted = false;
CAPUSquare    CAPU::m_square[2];
CAPUTriangle  CAPU::m_triangle;
CAPUNoise     CAPU::m_noise;
CAPUDMC       CAPU::m_dmc;

uint16_t*      CAPU::m_waveBuf = NULL;
int32_t        CAPU::m_waveBufProduce = 0;
int32_t        CAPU::m_waveBufConsume = 0;

uint32_t CAPU::m_cycles = 0;

float        CAPU::m_sampleSpacer = 0.0;

int32_t CAPU::m_sequencerMode = 0;
int32_t CAPU::m_newSequencerMode = 0;
// Cycles to wait before changing sequencer modes...0 or 1 are valid values
// on a mode-change.
int32_t CAPU::m_changeModes = -1;
int32_t CAPU::m_sequenceStep = 0;

// Events that can occur during the APU sequence stepping
enum
{
   APU_SEQ_INT_FLAG = 0x01,
   APU_SEQ_CLK_LENGTH_CTR = 0x02,
   APU_SEQ_CLK_ENVELOPE_CTR = 0x04
};

static int32_t m_seq4 [ 4 ] =
{
   APU_SEQ_CLK_ENVELOPE_CTR,
   APU_SEQ_CLK_ENVELOPE_CTR|APU_SEQ_CLK_LENGTH_CTR,
   APU_SEQ_CLK_ENVELOPE_CTR,
   APU_SEQ_CLK_ENVELOPE_CTR|APU_SEQ_CLK_LENGTH_CTR|APU_SEQ_INT_FLAG,
};

static int32_t m_seq5 [ 5 ] =
{
   APU_SEQ_CLK_ENVELOPE_CTR|APU_SEQ_CLK_LENGTH_CTR,
   APU_SEQ_CLK_ENVELOPE_CTR,
   APU_SEQ_CLK_ENVELOPE_CTR|APU_SEQ_CLK_LENGTH_CTR,
   APU_SEQ_CLK_ENVELOPE_CTR,
   0
};

static uint8_t m_lengthLUT [ 32 ] =
{
   0x0A,
   0xFE,
   0x14,
   0x02,
   0x28,
   0x04,
   0x50,
   0x06,
   0xA0,
   0x08,
   0x3C,
   0x0A,
   0x0E,
   0x0C,
   0x1A,
   0x0E,
   0x0C,
   0x10,
   0x18,
   0x12,
   0x30,
   0x14,
   0x60,
   0x16,
   0xC0,
   0x18,
   0x48,
   0x1A,
   0x10,
   0x1C,
   0x20,
   0x1E
};

static uint16_t m_squareTable [ 31 ] =
{
   0x0000,
   0x023A,
   0x0467,
   0x0687,
   0x089A,
   0x0AA0,
   0x0C9B,
   0x0E8A,
   0x106E,
   0x1248,
   0x1417,
   0x15DC,
   0x1797,
   0x1949,
   0x1AF2,
   0x1C92,
   0x1E29,
   0x1FB9,
   0x2140,
   0x22BF,
   0x2437,
   0x25A7,
   0x2710,
   0x2873,
   0x29CE,
   0x2B23,
   0x2C72,
   0x2DBA,
   0x2EFD,
   0x303A,
   0x3171
};

static uint16_t m_tndTable [ 203 ] =
{
   0x0000,
   0x0149,
   0x028F,
   0x03D3,
   0x0515,
   0x0653,
   0x0790,
   0x08C9,
   0x0A01,
   0x0B35,
   0x0C68,
   0x0D97,
   0x0EC5,
   0x0FF0,
   0x1119,
   0x123F,
   0x1364,
   0x1486,
   0x15A5,
   0x16C3,
   0x17DE,
   0x18F8,
   0x1A0F,
   0x1B24,
   0x1C37,
   0x1D48,
   0x1E57,
   0x1F63,
   0x206E,
   0x2177,
   0x227E,
   0x2383,
   0x2487,
   0x2588,
   0x2687,
   0x2785,
   0x2881,
   0x297B,
   0x2A73,
   0x2B6A,
   0x2C5E,
   0x2D51,
   0x2E43,
   0x2F32,
   0x3020,
   0x310D,
   0x31F7,
   0x32E0,
   0x33C8,
   0x34AE,
   0x3592,
   0x3675,
   0x3756,
   0x3836,
   0x3914,
   0x39F0,
   0x3ACC,
   0x3BA5,
   0x3C7E,
   0x3D55,
   0x3E2A,
   0x3EFE,
   0x3FD1,
   0x40A2,
   0x4172,
   0x4241,
   0x430E,
   0x43DA,
   0x44A5,
   0x456E,
   0x4636,
   0x46FD,
   0x47C2,
   0x4886,
   0x4949,
   0x4A0B,
   0x4ACC,
   0x4B8B,
   0x4C49,
   0x4D06,
   0x4DC2,
   0x4E7D,
   0x4F37,
   0x4FEF,
   0x50A6,
   0x515C,
   0x5211,
   0x52C5,
   0x5378,
   0x542A,
   0x54DB,
   0x558A,
   0x5639,
   0x56E7,
   0x5793,
   0x583F,
   0x58E9,
   0x5993,
   0x5A3B,
   0x5AE3,
   0x5B89,
   0x5C2F,
   0x5CD4,
   0x5D77,
   0x5E1A,
   0x5EBC,
   0x5F5D,
   0x5FFD,
   0x609C,
   0x613A,
   0x61D7,
   0x6273,
   0x630F,
   0x63A9,
   0x6443,
   0x64DC,
   0x6574,
   0x660B,
   0x66A2,
   0x6737,
   0x67CC,
   0x6860,
   0x68F3,
   0x6985,
   0x6A17,
   0x6AA7,
   0x6B37,
   0x6BC6,
   0x6C55,
   0x6CE2,
   0x6D6F,
   0x6DFB,
   0x6E87,
   0x6F11,
   0x6F9B,
   0x7024,
   0x70AD,
   0x7134,
   0x71BB,
   0x7241,
   0x72C7,
   0x734C,
   0x73D0,
   0x7454,
   0x74D6,
   0x7559,
   0x75DA,
   0x765B,
   0x76DB,
   0x775B,
   0x77D9,
   0x7858,
   0x78D5,
   0x7952,
   0x79CE,
   0x7A4A,
   0x7AC5,
   0x7B40,
   0x7BB9,
   0x7C33,
   0x7CAB,
   0x7D23,
   0x7D9B,
   0x7E12,
   0x7E88,
   0x7EFE,
   0x7F73,
   0x7FE7,
   0x805B,
   0x80CF,
   0x8142,
   0x81B4,
   0x8226,
   0x8297,
   0x8308,
   0x8378,
   0x83E7,
   0x8456,
   0x84C5,
   0x8533,
   0x85A0,
   0x860D,
   0x867A,
   0x86E6,
   0x8751,
   0x87BC,
   0x8827,
   0x8891,
   0x88FA,
   0x8963,
   0x89CB,
   0x8A33,
   0x8A9B,
   0x8B02,
   0x8B69,
   0x8BCF,
   0x8C34,
   0x8C9A,
   0x8CFE,
   0x8D63,
   0x8DC6,
   0x8E2A,
   0x8E8D
};

int32_t apuDataAvailable = 0;

static CAPU __init __attribute((unused));

CAPU::CAPU()
{
   m_square[0].SetChannel ( 0 );
   m_square[1].SetChannel ( 1 );
   m_triangle.SetChannel ( 2 );
   m_noise.SetChannel ( 3 );
   m_dmc.SetChannel ( 4 );

   m_waveBuf = new uint16_t [ APU_BUFFER_SIZE ];
   memset( m_waveBuf, 0, APU_BUFFER_SIZE * sizeof m_waveBuf[ 0 ] );
}

uint8_t* CAPU::PLAY ( uint16_t samples )
{
   uint16_t* waveBuf;

   waveBuf = m_waveBuf + m_waveBufConsume;

   m_waveBufConsume += samples;
   m_waveBufConsume %= APU_BUFFER_SIZE;

   apuDataAvailable -= samples;

   return (uint8_t*)waveBuf;
}

uint16_t CAPU::AMPLITUDE ( void )
{
   int16_t amp;
   static int16_t old = 0;
   int16_t delta;
   static int16_t out;

   amp = *(m_squareTable+(m_square[0].AVERAGEDAC()+m_square[1].AVERAGEDAC()))
         + *(m_tndTable+((m_triangle.AVERAGEDAC()*3)+(m_noise.AVERAGEDAC()<<1)+m_dmc.AVERAGEDAC()));

   delta = amp - old;
   old = amp;

   out = ((out*65371)/65536)+delta; // 65371/65536 is 0.9975 adjusted to 16-bit fixed point.

   // Reset DAC averaging...
   m_square[0].CLEARDACAVG();
   m_square[1].CLEARDACAVG();
   m_triangle.CLEARDACAVG();
   m_noise.CLEARDACAVG();
   m_dmc.CLEARDACAVG();

   return out;
}

void CAPU::SEQTICK ( int32_t sequence )
{
   bool clockedLengthCounter = false;
   bool clockedLinearCounter = false;

   if ( m_sequencerMode )
   {
      if ( m_seq5[sequence]&APU_SEQ_CLK_ENVELOPE_CTR )
      {
         m_square[0].CLKENVELOPE ();
         m_square[1].CLKENVELOPE ();
         clockedLinearCounter |= m_triangle.CLKLINEARCOUNTER ();
         m_noise.CLKENVELOPE ();
      }

      if ( m_seq5[sequence]&APU_SEQ_CLK_LENGTH_CTR )
      {
         clockedLengthCounter |= m_square[0].CLKLENGTHCOUNTER ();
         m_square[0].CLKSWEEPUNIT ();
         clockedLengthCounter |= m_square[1].CLKLENGTHCOUNTER ();
         m_square[1].CLKSWEEPUNIT ();
         clockedLengthCounter |= m_triangle.CLKLENGTHCOUNTER ();
         clockedLengthCounter |= m_noise.CLKLENGTHCOUNTER ();
      }
   }
   else
   {
      if ( m_seq4[sequence]&APU_SEQ_CLK_ENVELOPE_CTR )
      {
         m_square[0].CLKENVELOPE ();
         m_square[1].CLKENVELOPE ();
         clockedLinearCounter |= m_triangle.CLKLINEARCOUNTER ();
         m_noise.CLKENVELOPE ();
      }

      if ( m_seq4[sequence]&APU_SEQ_CLK_LENGTH_CTR )
      {
         clockedLengthCounter |= m_square[0].CLKLENGTHCOUNTER ();
         m_square[0].CLKSWEEPUNIT ();
         clockedLengthCounter |= m_square[1].CLKLENGTHCOUNTER ();
         m_square[1].CLKSWEEPUNIT ();
         clockedLengthCounter |= m_triangle.CLKLENGTHCOUNTER ();
         clockedLengthCounter |= m_noise.CLKLENGTHCOUNTER ();
      }

      if ( m_seq4[sequence]&APU_SEQ_INT_FLAG )
      {
         if ( m_irqEnabled )
         {
            m_irqAsserted = true;
            C6502::ASSERTIRQ ( eNESSource_APU );

            if ( nesIsDebuggable() )
            {
               // Check for IRQ breakpoint...
               CNES::CHECKBREAKPOINT(eBreakInAPU,eBreakOnAPUEvent,0,APU_EVENT_IRQ);
            }
         }
      }
   }

   // Check for Length Counter clocking breakpoint...
   if ( nesIsDebuggable() )
   {
      if ( clockedLengthCounter )
      {
         CNES::CHECKBREAKPOINT(eBreakInAPU,eBreakOnAPUEvent,0,APU_EVENT_LENGTH_COUNTER_CLOCKED);
      }
   }
}

void CAPU::RESET ( void )
{
   int32_t idx;

#if defined ( OUTPUT_WAV )
   if ( wavOut )
   {
      fclose(wavOut);
   }
   sprintf(wavOutName,"nes%d.wav",wavOutFileNum);
   wavOut = fopen(wavOutName,"wb");
   wavFileSize = 0;
   strncpy(wavOutHdr.ftype,"RIFF",4);
   strncpy(wavOutHdr.ctype,"WAVE",4);
   wavOutHdr.len = (88200*60)+4+16+4;
   fwrite(&wavOutHdr,1,sizeof(wavHdr),wavOut);
   strncpy(wavChunk.ctype,"fmt ",4);
   wavChunk.len = 16;
   wavFmt.ccode = 1;
   wavFmt.chans = 5;
   wavFmt.Bps = 88200;
   wavFmt.align = 2;
   wavFmt.rate = 44100;
   wavFmt.bps = 16;
   fwrite(&wavChunk,1,sizeof(wavChunkHdr),wavOut);
   fwrite(&wavFmt,1,sizeof(wavFmtChunk),wavOut);
   strncpy(wavChunk.ctype,"data",4);
   wavChunk.len = 88200*60;
   fwrite(&wavChunk,1,sizeof(wavChunkHdr),wavOut);
#endif

   for ( idx = 0; idx < 32; idx++ )
   {
      m_APUreg [ idx ] = 0x00;
      m_APUregDirty [ idx ] = 1;
   }

   m_square[0].RESET ();
   m_square[1].RESET ();
   m_triangle.RESET ();
   m_noise.RESET ();
   m_dmc.RESET ();

   // Reset DAC averaging...
   m_square[0].CLEARDACAVG();
   m_square[1].CLEARDACAVG();
   m_triangle.CLEARDACAVG();
   m_noise.CLEARDACAVG();
   m_dmc.CLEARDACAVG();

   m_irqEnabled = true;
   m_irqAsserted = false;
   C6502::RELEASEIRQ ( eNESSource_APU );
   m_sequencerMode = 0;
   m_sequenceStep = 0;

   m_waveBufProduce = 0;
   m_waveBufConsume = 0;

   memset( m_waveBuf, 0, APU_BUFFER_SIZE * sizeof m_waveBuf[ 0 ] );

   if ( CNES::VIDEOMODE() == MODE_NTSC )
   {
      m_sampleSpacer = APU_SAMPLE_SPACE_NTSC;
   }
   else
   {
      m_sampleSpacer = APU_SAMPLE_SPACE_PAL;
   }

   m_cycles = 0;
   apuDataAvailable = 0;
}

CAPUOscillator::CAPUOscillator ()
{
   int32_t idx;

   m_muted = false;
   m_linearCounter = 0;
   m_linearCounterReload = 0;
   m_lengthCounter = 0;
   m_period = 0;
   m_periodCounter = 0;
   m_envelope = 0;
   m_envelopeCounter = 0;
   m_envelopeDivider = 0;
   m_volume = 0;
   m_volumeSet = 0;
   m_sweepDivider = 0;
   m_sweepShift = 0;
   m_sweepNegate = false;
   m_sweep = 0;
   m_enabled = false;
   m_halted = false;
   m_newHalted = false;
   m_envelopeEnabled = false;
   m_sweepEnabled = false;
   m_linearCounterHalted = false;
   m_dac = 0x00;
   m_reg1Wrote = false;
   m_reg3Wrote = false;

   for ( idx = 0; idx < 4; idx++ )
   {
      m_reg [ idx ] = 0x00;
   }
}

void CAPUOscillator::APU ( uint32_t addr, uint8_t data )
{
   *(m_reg+(addr&0x03)) = data;
}

void CAPUOscillator::CLKSWEEPUNIT ( void )
{
   bool sweepClkEdge = false;
   int16_t sweepPeriod = 0;

   // sweep unit...
   m_sweep++;

   if ( m_sweep >= m_sweepDivider )
   {
      m_sweep = 0;
      sweepClkEdge = true;
   }

   if ( m_reg1Wrote == true )
   {
      m_reg1Wrote = false;
      m_sweep = 0;
   }

   if ( m_sweepEnabled )
   {
      sweepPeriod = m_period;
      sweepPeriod >>= m_sweepShift;

      if ( m_sweepNegate )
      {
         sweepPeriod = ~sweepPeriod;

         if ( m_channel == 1 )
         {
            sweepPeriod++;
         }
      }

      sweepPeriod += m_period;
   }

   if ( (m_period < 8) ||
        (sweepPeriod > 0x7FF) )
   {
      m_volume = 0;
   }
   else
   {
      if ( (m_sweepEnabled) && (m_sweepShift) )
      {
         if ( sweepClkEdge )
         {
            m_period = sweepPeriod&0x7FF;
         }
      }
   }
}

bool CAPUOscillator::CLKLENGTHCOUNTER ( void )
{
   bool clockedIt = false;

   if ( (!m_halted) && m_clockLengthCounter )
   {
      // length counter...
      if ( m_lengthCounter )
      {
         m_lengthCounter--;
         clockedIt = true;
      }
   }

   // Reset clocking flag...
   m_clockLengthCounter = true;

   return clockedIt;
}

bool CAPUOscillator::CLKLINEARCOUNTER ( void )
{
   bool clockedIt = false;

   // linear counter...
   if ( m_linearCounterHalted )
   {
      m_linearCounter = m_linearCounterReload;
   }
   else if ( m_linearCounter )
   {
      m_linearCounter--;
      clockedIt = true;
   }

   if ( !(m_reg[0]&0x80) )
   {
      m_linearCounterHalted = false;
   }

   return clockedIt;
}

void CAPUOscillator::CLKENVELOPE ( void )
{
   bool envelopeClkEdge = false;

   // envelope...
   if ( m_reg3Wrote )
   {
      m_reg3Wrote = false;
      m_envelopeCounter = 15;
      m_envelope = 0;
   }
   else
   {
      m_envelope++;

      if ( m_envelope >= m_envelopeDivider )
      {
         m_envelope = 0;
         envelopeClkEdge = true;
      }
   }

   if ( envelopeClkEdge )
   {
      if ( (m_halted) && (m_envelopeCounter == 0) )
      {
         m_envelopeCounter = 15;
      }
      else if ( m_envelopeCounter )
      {
         m_envelopeCounter--;
      }
   }

   if ( m_envelopeEnabled )
   {
      m_volume = m_envelopeCounter;
   }
   else
   {
      m_volume = m_volumeSet;
   }
}

uint32_t CAPUOscillator::CLKDIVIDER ( void )
{
   uint32_t clockIt = 0;

   if ( m_periodCounter )
   {
      m_periodCounter--;
   }

   if ( m_periodCounter == 0 )
   {
      m_periodCounter = m_period;
      clockIt = 1;
   }

   return clockIt;
}

uint8_t CAPUOscillator::AVERAGEDAC ( void )
{
   uint32_t val;
   uint32_t avg = 0;

   if ( !m_muted )
   {
      for ( val = 0; val < m_dacSamples; val++ )
      {
         avg += (m_dacAverage[val]);
      }

      avg /= m_dacSamples;
      m_dac = avg;
   }
   else
   {
      m_dac = 0;
   }

   return m_dac;
}

static int32_t m_squareSeq [ 4 ] [ 8 ] =
{
   { 0, 1, 0, 0, 0, 0, 0, 0 },
   { 0, 1, 1, 0, 0, 0, 0, 0 },
   { 0, 1, 1, 1, 1, 0, 0, 0 },
   { 1, 0, 0, 1, 1, 1, 1, 1 }
};

void CAPUSquare::APU ( uint32_t addr, uint8_t data )
{
   CAPUOscillator::APU ( addr, data );

   if ( addr == 0 )
   {
      m_duty = ((data&0xC0)>>6);
      m_newHalted = data&0x20;
      m_envelopeEnabled = !(data&0x10);
      m_envelopeDivider = (data&0x0F)+1;
      m_volume = (data&0x0F);
      m_volumeSet = m_volume;
   }
   else if ( addr == 1 )
   {
      m_sweepEnabled = data&0x80;
      m_sweepDivider = ((data&0x70)>>4)+1;
      m_sweepNegate = data&0x08;
      m_sweepShift = (data&0x07);
      m_reg1Wrote = true;
   }
   else if ( addr == 2 )
   {
      m_period &= 0xFF00;
      m_period |= data;
   }
   else if ( addr == 3 )
   {
      if ( m_enabled )
      {
         if ( CNES::VIDEOMODE() == MODE_NTSC )
         {
            if ( CAPU::SEQUENCERMODE() == 0 )
            {
               if ( (CAPU::CYCLES() == 14915) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 29831) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 14915) && (CAPU::CYCLES() != 29831)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
            else
            {
               if ( (CAPU::CYCLES() == 1) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 14915) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 1) && (CAPU::CYCLES() != 14915)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
         }
         else
         {
            if ( CAPU::SEQUENCERMODE() == 0 )
            {
               if ( (CAPU::CYCLES() == 16629) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 33255) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 16629) && (CAPU::CYCLES() != 33255)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
            else
            {
               if ( (CAPU::CYCLES() == 1) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 16629) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 1) && (CAPU::CYCLES() != 16629)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
         }
      }

      m_period &= 0x00FF;
      m_period |= ((data&0x07)<<8);
      m_period += 1;
      m_reg3Wrote = true;
      m_seqTick = 0;
   }
}

void CAPUSquare::TIMERTICK ( void )
{
   uint32_t clockIt = CLKDIVIDER ();
   uint32_t seqTicks;

   // divide timer by 2...
   m_timerClk += clockIt;
   seqTicks = m_timerClk>>1;
   m_timerClk &= 0x1;
   m_seqTick += seqTicks;
   m_seqTick &= 0x7;

   if ( (m_lengthCounter) && (m_period > 7) )
   {
      if ( *(*(m_squareSeq+m_duty)+m_seqTick) )
      {
         SETDAC ( m_volume );
      }
      else
      {
         SETDAC ( 0 );
      }
   }
   else
   {
      SETDAC ( 0 );
   }

   m_halted = m_newHalted;
}

static int32_t m_triangleSeq [ 32 ] =
{
   0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
   0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
   0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8,
   0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0
};

void CAPUTriangle::APU ( uint32_t addr, uint8_t data )
{
   CAPUOscillator::APU ( addr, data );

   if ( addr == 0 )
   {
      m_newHalted = data&0x80;
      m_linearCounterReload = (data&0x7F);
   }
   else if ( addr == 2 )
   {
      m_period &= 0xFF00;
      m_period |= data;
   }
   else if ( addr == 3 )
   {
      if ( m_enabled )
      {
         if ( CNES::VIDEOMODE() == MODE_NTSC )
         {
            if ( CAPU::SEQUENCERMODE() == 0 )
            {
               if ( (CAPU::CYCLES() == 14915) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 29831) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 14915) && (CAPU::CYCLES() != 29831)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
            else
            {
               if ( (CAPU::CYCLES() == 1) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 14915) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 1) && (CAPU::CYCLES() != 14915)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
         }
         else
         {
            if ( CAPU::SEQUENCERMODE() == 0 )
            {
               if ( (CAPU::CYCLES() == 16629) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 33255) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 16629) && (CAPU::CYCLES() != 33255)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
            else
            {
               if ( (CAPU::CYCLES() == 1) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 16629) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 1) && (CAPU::CYCLES() != 16629)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
         }
      }

      m_period &= 0x00FF;
      m_period |= ((data&0x07)<<8);
      m_period += 1;
      m_linearCounterHalted = true;
      m_reg3Wrote = true;
   }
}

void CAPUTriangle::TIMERTICK ( void )
{
   uint32_t clockIt = CLKDIVIDER ();

   if ( (m_linearCounter) &&
        (m_lengthCounter) && (m_period>1) )
   {
      m_seqTick += clockIt;
      m_seqTick &= 0x1F;
   }

   SETDAC ( *(m_triangleSeq+m_seqTick) );

   m_halted = m_newHalted;
}

static uint16_t m_noisePeriod [ 2 ][ 16 ] =
{
   {
      4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
   },
   {
      4, 7, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708,  944, 1890, 3778
   }
};

CAPUNoise::CAPUNoise ()
   : CAPUOscillator(), m_mode(0)
{
   uint8_t preShiftXor;
   uint16_t shift;
   int32_t idx;

   // Allocate noise tables.
   m_shortTable = new uint8_t[93];
   m_longTable = new uint8_t[32767];

   // Calculate short table...
   shift = 1;

   for ( idx = 0; idx < 93; idx++ )
   {
      preShiftXor = (shift&1)^((shift>>6)&1);
      shift >>= 1;
      shift |= preShiftXor<<13;
      m_shortTable [ idx ] = shift;
   }

   // Calculate long table...
   shift = 1;

   for ( idx = 0; idx < 32767; idx++ )
   {
      preShiftXor = (shift&1)^((shift>>1)&1);
      shift >>= 1;
      shift |= preShiftXor<<13;
      m_longTable [ idx ] = shift;
   }
}

CAPUNoise::~CAPUNoise()
{
   delete [] m_shortTable;
   delete [] m_longTable;
}

void CAPUNoise::APU ( uint32_t addr, uint8_t data )
{
   CAPUOscillator::APU ( addr, data );

   if ( addr == 0 )
   {
      m_newHalted = data&0x20;
      m_envelopeEnabled = !(data&0x10);
      m_envelopeDivider = (data&0x0F)+1;
      m_volume = (data&0x0F);
      m_volumeSet = m_volume;
   }
   else if ( addr == 2 )
   {
      m_mode = data&0x80;
      m_period = *(*(m_noisePeriod+CNES::VIDEOMODE())+(data&0xF));
   }
   else if ( addr == 3 )
   {
      if ( m_enabled )
      {
         if ( CNES::VIDEOMODE() == MODE_NTSC )
         {
            if ( CAPU::SEQUENCERMODE() == 0 )
            {
               if ( (CAPU::CYCLES() == 14915) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 29831) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 14915) && (CAPU::CYCLES() != 29831)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
            else
            {
               if ( (CAPU::CYCLES() == 1) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 14915) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 1) && (CAPU::CYCLES() != 14915)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
         }
         else
         {
            if ( CAPU::SEQUENCERMODE() == 0 )
            {
               if ( (CAPU::CYCLES() == 16629) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 33255) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 16629) && (CAPU::CYCLES() != 33255)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
            else
            {
               if ( (CAPU::CYCLES() == 1) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( (CAPU::CYCLES() == 16629) && (m_lengthCounter == 0) )
               {
                  m_clockLengthCounter = false;
               }

               if ( ((CAPU::CYCLES() != 1) && (CAPU::CYCLES() != 16629)) || (m_lengthCounter == 0) )
               {
                  m_lengthCounter = *(m_lengthLUT+(data>>3));
               }
            }
         }
      }

      m_reg3Wrote = true;
   }
}

void CAPUNoise::TIMERTICK ( void )
{
   uint32_t clockIt = CLKDIVIDER ();
   uint16_t shift;

   if ( m_mode )
   {
      m_shortTableIdx += clockIt;
      m_shortTableIdx %= 93;
      shift = *(m_shortTable+m_shortTableIdx);
   }
   else
   {
      m_longTableIdx += clockIt;
      m_longTableIdx %= 32767;
      shift = *(m_longTable+m_longTableIdx);
   }

   if ( !(shift&1) )
   {
      if ( m_lengthCounter )
      {
         SETDAC ( m_volume );
      }
      else
      {
         SETDAC ( 0 );
      }
   }
   else
   {
      SETDAC ( 0 );
   }

   m_halted = m_newHalted;
}

static uint16_t m_dmcPeriod [ 2 ][ 16 ] =
{
   // NTSC
   {
      428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54
   },
   // PAL
   {
      398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118,  98,  78,  66,  50
   }
};

void CAPUDMC::RESET ( void )
{
   CAPUOscillator::RESET();
   m_loop = false;
   m_sampleAddr = 0xc000;
   m_sampleLength = 0x0000;
   m_dmaReaderAddrPtr = 0xc000;
   m_dmcIrqEnabled = true;
   m_dmcIrqAsserted = false;
   m_sampleBuffer = 0x00;
   m_sampleBufferFull = false;
   m_outputShift = 0x00;
   m_outputShiftCounter = 0;
   m_silence = true;
   m_dmaSource = NULL;
   m_dmaSourcePtr = NULL;
   m_period = (*(*(m_dmcPeriod+CNES::VIDEOMODE())));
}

void CAPUDMC::APU ( uint32_t addr, uint8_t data )
{
   CAPUOscillator::APU ( addr, data );

   if ( addr == 0 )
   {
      m_dmcIrqEnabled = data&0x80;

      if ( !m_dmcIrqEnabled )
      {
         m_dmcIrqAsserted = false;
         CAPU::RELEASEIRQ ();
      }

      m_loop = data&0x40;
      m_period = (*(*(m_dmcPeriod+CNES::VIDEOMODE())+(data&0xF)));
   }
   else if ( addr == 1 )
   {
      m_volume = data&0x7f;
   }
   else if ( addr == 2 )
   {
      m_sampleAddr = (((uint16_t)data)<<6)|0xC000;
   }
   else if ( addr == 3 )
   {
      m_sampleLength = (((uint16_t)data)<<4)+1;
   }
}

void CAPUDMC::ENABLE ( bool enabled )
{
   CAPUOscillator::ENABLE(enabled);

   m_dmcIrqAsserted = false;
   CAPU::RELEASEIRQ ();

   if ( enabled )
   {
      if ( !m_lengthCounter )
      {
         if ( m_dmaSource != NULL )
         {
            m_dmaSourcePtr = m_dmaSource;
         }
         else
         {
            m_dmaReaderAddrPtr = m_sampleAddr;
         }

         m_lengthCounter = m_sampleLength;

         DMAREADER();
      }
   }
}

void CAPUDMC::DMASAMPLE ( uint8_t data )
{
   m_sampleBuffer = data;

   m_sampleBufferFull = true;

   if ( m_dmaSource != NULL )
   {
      m_dmaSourcePtr++;
   }
   else
   {
      if ( m_dmaReaderAddrPtr == 0xFFFF )
      {
         m_dmaReaderAddrPtr = 0x8000;
      }
      else
      {
         m_dmaReaderAddrPtr++;
      }
   }

   m_lengthCounter--;

   if ( (!m_lengthCounter) && m_dmcIrqEnabled && (!m_loop) )
   {
      m_dmcIrqAsserted = true;
      C6502::ASSERTIRQ ( eNESSource_APU );
   }

   if ( (!m_lengthCounter) && m_loop )
   {
      if ( m_dmaSource != NULL )
      {
         m_dmaSourcePtr = m_dmaSource;
      }
      else
      {
         m_dmaReaderAddrPtr = m_sampleAddr;
      }

      m_lengthCounter = m_sampleLength;
   }
}

void CAPUDMC::DMAREADER ( void )
{
   if ( !m_sampleBufferFull )
   {
      if ( m_lengthCounter )
      {
         if ( m_dmaSource != NULL )
         {
            m_sampleBuffer = (*m_dmaSourcePtr);
         }
         else
         {
            C6502::APUDMAREQ ( m_dmaReaderAddrPtr );

            if ( nesIsDebuggable() )
            {
               // Check for APU DMC channel DMA breakpoint event...
               CNES::CHECKBREAKPOINT(eBreakInAPU,eBreakOnAPUEvent,0,APU_EVENT_DMC_DMA);
            }
         }
      }
   }
}

void CAPUDMC::TIMERTICK ( void )
{
   uint32_t clockIt = CLKDIVIDER ();

   if ( clockIt )
   {
      if ( m_outputShiftCounter == 0 )
      {
         m_outputShiftCounter = 8;

         if ( m_sampleBufferFull )
         {
            m_outputShift = m_sampleBuffer;
            m_sampleBufferFull = false;
            DMAREADER();

            m_silence = false;
         }
         else
         {
            m_silence = true;
         }
      }

      if ( !m_silence )
      {
         if ( (!(m_outputShift&0x01)) && (m_volume > 1) )
         {
            m_volume -= 2;
         }
         else if ( (m_outputShift&0x01) && (m_volume < 126) )
         {
            m_volume += 2;
         }
      }

      if ( m_outputShiftCounter )
      {
         m_outputShift >>= 1;
         m_outputShiftCounter--;
      }
   }

   SETDAC ( m_volume );
}

void CAPU::EMULATE ( void )
{
   static float takeSample = 0.0f;
   uint16_t* pWaveBuf;
   int32_t cycle;

   // Handle APU clock jitter.  Mode changes occur
   // only on even APU clocks.  On a mode change write
   // to $4017, m_changeModes is set to either 0 or
   // 1 indicating that the mode change should happen
   // in 0 or 1 clocks from now.  Do the mode change
   // when m_changeModes is 0; decrement it if it isn't 0.
   if ( m_changeModes == 0 )
   {
      // Do mode-change now...
      m_changeModes--;
      m_sequencerMode = m_newSequencerMode;
      m_sequenceStep = 0;
      RESETCYCLECOUNTER(0);

      if ( nesIsDebuggable() )
      {
         // Emit frame-start indication to Tracer...
         CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_StartAPUFrame, eNESSource_APU, 0, 0, 0 );
      }
   }

   if ( m_changeModes > 0 )
   {
      m_changeModes--;
   }

   // Clock the 240Hz sequencer.
   // NTSC APU
   if ( CNES::VIDEOMODE() == MODE_NTSC )
   {
      // APU sequencer mode 1
      if ( m_sequencerMode )
      {
         if ( m_cycles == 1 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 0 );
         }
         else if ( m_cycles == 7459 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 1 );
         }
         else if ( m_cycles == 14915 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 2 );
         }
         else if ( m_cycles == 22373 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 3 );
         }
         else if ( m_cycles == 29829 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            // Nothing to do this "tick"...
         }
      }
      // APU sequencer mode 0
      else
      {
         if ( m_cycles == 7459 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 0 );
         }
         else if ( m_cycles == 14915 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 1 );
         }
         else if ( m_cycles == 22373 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 2 );
         }
         else if ( (m_cycles == 29830) ||
                   (m_cycles == 29832) )
         {
            if ( m_irqEnabled )
            {
               m_irqAsserted = true;
               C6502::ASSERTIRQ(eNESSource_APU);

               if ( nesIsDebuggable() )
               {
                  // Check for IRQ breakpoint...
                  CNES::CHECKBREAKPOINT(eBreakInAPU,eBreakOnAPUEvent,0,APU_EVENT_IRQ);
               }
            }
         }
         else if ( m_cycles == 29831 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            // IRQ asserted inside SEQTICK...
            SEQTICK ( 3 );
         }
      }
   }
   // PAL APU
   else
   {
      // APU sequencer mode 1
      if ( m_sequencerMode )
      {
         if ( m_cycles == 1 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 0 );
         }
         else if ( m_cycles == 8315 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 1 );
         }
         else if ( m_cycles == 16629 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 2 );
         }
         else if ( m_cycles == 24941 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 3 );
         }
         else if ( m_cycles == 33255 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            // Nothing to do this "tick"...
         }
      }
      // APU sequencer mode 0
      else
      {
         if ( m_cycles == 8315 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 0 );
         }
         else if ( m_cycles == 16629 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 1 );
         }
         else if ( m_cycles == 24941 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            SEQTICK ( 2 );
         }
         else if ( (m_cycles == 33254) ||
                   (m_cycles == 33256) )
         {
            if ( m_irqEnabled )
            {
               m_irqAsserted = true;
               C6502::ASSERTIRQ(eNESSource_APU);

               if ( nesIsDebuggable() )
               {
                  // Check for IRQ breakpoint...
                  CNES::CHECKBREAKPOINT(eBreakInAPU,eBreakOnAPUEvent,0,APU_EVENT_IRQ);
               }
            }
         }
         else if ( m_cycles == 33255 )
         {
            if ( nesIsDebuggable() )
            {
               // Emit frame-end indication to Tracer...
               CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_SequencerStep, eNESSource_APU, 0, 0, 0 );
            }

            // IRQ asserted inside SEQTICK...
            SEQTICK ( 3 );
         }
      }
   }

   // Clock the individual channels.
   m_square[0].TIMERTICK ();
   m_square[1].TIMERTICK ();
   m_triangle.TIMERTICK ();
   m_noise.TIMERTICK ();
   m_dmc.TIMERTICK ();

   // Generate audio samples.
   takeSample += 1.0;

   if ( takeSample >= m_sampleSpacer )
   {
#if defined ( OUTPUT_WAV )
if ( wavOut )
{
   uint8_t s1,s2,t,n,d;
   CAPU::GETDACS(&s1,&s2,&t,&n,&d);
   fwrite(&s1,1,1,wavOut);
   fwrite(&s2,1,1,wavOut);
   fwrite(&t,1,1,wavOut);
   fwrite(&n,1,1,wavOut);
   fwrite(&d,1,1,wavOut);
   wavFileSize += 5;
   if ( wavFileSize == 88200*20 )
   {
      fclose(wavOut);
      wavOut = NULL;
   }
}
#endif

      takeSample -= m_sampleSpacer;

      pWaveBuf = m_waveBuf+m_waveBufProduce;
      (*pWaveBuf) = AMPLITUDE ();
      m_waveBufProduce++;

      m_waveBufProduce %= APU_BUFFER_SIZE;

      apuDataAvailable++;

      if ( apuDataAvailable >= APU_BUFFER_PRERENDER )
      {
         nesBreakAudio();
      }
   }

   // Go to next cycle and restart if necessary...
   m_cycles++;

   if ( CNES::VIDEOMODE() == MODE_NTSC )
   {
      if ( (m_sequencerMode) && (m_cycles >= 37283) )
      {
         if ( nesIsDebuggable() )
         {
            // Emit frame-end indication to Tracer...
            CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_EndAPUFrame, eNESSource_APU, 0, 0, 0 );
         }

         RESETCYCLECOUNTER(1);

         if ( nesIsDebuggable() )
         {
            // Emit frame-start indication to Tracer...
            CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_StartAPUFrame, eNESSource_APU, 0, 0, 0 );
         }
      }
      else if ( (!m_sequencerMode) && (m_cycles >= 37289) )
      {
         if ( nesIsDebuggable() )
         {
            // Emit frame-end indication to Tracer...
            CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_EndAPUFrame, eNESSource_APU, 0, 0, 0 );
         }

         RESETCYCLECOUNTER(7459);

         if ( nesIsDebuggable() )
         {
            // Emit frame-start indication to Tracer...
            CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_StartAPUFrame, eNESSource_APU, 0, 0, 0 );
         }
      }
   }
   else // MODE_PAL
   {
      if ( (m_sequencerMode) && (m_cycles >= 41567) )
      {
         if ( nesIsDebuggable() )
         {
            // Emit frame-end indication to Tracer...
            CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_EndAPUFrame, eNESSource_APU, 0, 0, 0 );
         }

         RESETCYCLECOUNTER(1);

         if ( nesIsDebuggable() )
         {
            // Emit frame-start indication to Tracer...
            CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_StartAPUFrame, eNESSource_APU, 0, 0, 0 );
         }
      }
      else if ( (!m_sequencerMode) && (m_cycles >= 41569) )
      {
         if ( nesIsDebuggable() )
         {
            // Emit frame-end indication to Tracer...
            CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_EndAPUFrame, eNESSource_APU, 0, 0, 0 );
         }

         RESETCYCLECOUNTER(8315);

         if ( nesIsDebuggable() )
         {
            // Emit frame-start indication to Tracer...
            CNES::TRACER()->AddSample ( CAPU::CYCLES(), eTracer_StartAPUFrame, eNESSource_APU, 0, 0, 0 );
         }
      }
   }
}

void CAPU::RELEASEIRQ ( void )
{
   if ( (!m_irqAsserted) && (!m_dmc.IRQASSERTED()) )
   {
      C6502::RELEASEIRQ ( eNESSource_APU );
   }
}

uint32_t CAPU::APU ( uint32_t addr )
{
   uint32_t data = 0x00;

   if ( addr == APUCTRL )
   {
      data |= (m_square[0].LENGTH()?0x01:0x00);
      data |= (m_square[1].LENGTH()?0x02:0x00);
      data |= (m_triangle.LENGTH()?0x04:0x00);
      data |= (m_noise.LENGTH()?0x08:0x00);
      data |= (m_dmc.LENGTH()?0x10:0x00);
      data |= (m_irqAsserted?0x40:0x00);
      data |= (m_dmc.IRQASSERTED()?0x80:0x00);

      m_irqAsserted = false;
      CAPU::RELEASEIRQ ();

      if ( nesIsDebuggable() )
      {
         CNES::CHECKBREAKPOINT(eBreakInAPU,eBreakOnAPUState, addr&0x1F);
      }
   }
   else
   {
      // All addresses but $4015 read as $40 since the
      // high-byte of $4015 is what was last put on the bus.
      data = 0x40;
   }

   return data;
}

void CAPU::APU ( uint32_t addr, uint8_t data )
{
   // For APU recording...
   m_APUreg [ addr&0x1F ] = data;
   m_APUregDirty [ addr&0x1F ] = 1;

   if ( addr < 0x4004 )
   {
      // Square 1
      m_square[0].APU ( addr&0x3, data );
   }
   else if ( addr < 0x4008 )
   {
      // Square 2
      m_square[1].APU ( addr&0x3, data );
   }
   else if ( addr < 0x400C )
   {
      // Triangle
      m_triangle.APU ( addr&0x3, data );
   }
   else if ( addr < 0x4010 )
   {
      // Noise
      m_noise.APU ( addr&0x3, data );
   }
   else if ( addr < 0x4014 )
   {
      // DMC
      m_dmc.APU ( addr&0x3, data );
   }
   else if ( addr == APUCTRL )
   {
      m_square[0].ENABLE ( !!(data&0x01) );
      m_square[1].ENABLE ( !!(data&0x02) );
      m_triangle.ENABLE ( !!(data&0x04) );
      m_noise.ENABLE ( !!(data&0x08) );
      m_dmc.ENABLE ( !!(data&0x10) );
   }
   else if ( addr == 0x4017 )
   {
      m_newSequencerMode = data&0x80;
      m_irqEnabled = !(data&0x40);

      if ( !m_irqEnabled )
      {
         m_irqAsserted = false;
         m_dmc.IRQASSERTED(false);
         CAPU::RELEASEIRQ ();
      }

      // Change modes on even cycle...
      m_changeModes = C6502::_CYCLES()&1;
   }

   if ( nesIsDebuggable() )
   {
      CNES::CHECKBREAKPOINT(eBreakInAPU,eBreakOnAPUState,addr&0x1F);
   }
}
