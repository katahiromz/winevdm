/*
	MS-DOS Player for Win32 console

	Author : Takeda.Toshiya
	Date   : 2009.11.09-
*/

#include "msdos.h"

#define my_strchr(str, chr) (char *)_mbschr((unsigned char *)(str), (unsigned int)(chr))
#define my_strtok(tok, del) (char *)_mbstok((unsigned char *)(tok), (const unsigned char *)(del))
#define my_strupr(str) (char *)_mbsupr((unsigned char *)(str))

#define fatalerror(...) { \
	fprintf(stderr, __VA_ARGS__); \
	exit(1); \
}
#define error(...) fprintf(stderr, "error: " __VA_ARGS__)

#if defined(__MINGW32__)
extern "C" int _CRT_glob = 0;
#endif

/*
	kludge for "more-standardized" C++
*/
#if !defined(_MSC_VER)
inline int kludge_min(int a, int b) { return (a<b ? a:b); }
inline int kludge_max(int a, int b) { return (a>b ? a:b); }
#define min(a,b) kludge_min(a,b)
#define max(a,b) kludge_max(a,b)
#endif

void change_console_size_to_80x25();

/* ----------------------------------------------------------------------------
	MAME i86/i386
---------------------------------------------------------------------------- */

#define SUPPORT_DISASSEMBLER

#if defined(HAS_I86)
	#define CPU_MODEL i8086
#elif defined(HAS_I186)
	#define CPU_MODEL i80186
#elif defined(HAS_I286)
	#define CPU_MODEL i80286
#elif defined(HAS_I386)
	#define CPU_MODEL i386
#else
	#if defined(HAS_I386SX)
		#define CPU_MODEL i386SX
	#else
		#if defined(HAS_I486)
			#define CPU_MODEL i486
		#else
			#if defined(HAS_PENTIUM)
				#define CPU_MODEL pentium
			#elif defined(HAS_MEDIAGX)
				#define CPU_MODEL mediagx
			#elif defined(HAS_PENTIUM_PRO)
				#define CPU_MODEL pentium_pro
			#elif defined(HAS_PENTIUM_MMX)
				#define CPU_MODEL pentium_mmx
			#elif defined(HAS_PENTIUM2)
				#define CPU_MODEL pentium2
			#elif defined(HAS_PENTIUM3)
				#define CPU_MODEL pentium3
			#elif defined(HAS_PENTIUM4)
				#define CPU_MODEL pentium4
			#endif
			#define SUPPORT_RDTSC
		#endif
		#define SUPPORT_FPU
	#endif
	#define HAS_I386
#endif

#ifndef __BIG_ENDIAN__
#define LSB_FIRST
#endif

#ifndef INLINE
#define INLINE inline
#endif
#define U64(v) UINT64(v)

#ifdef _DEBUG
void logerror(const char *format, ...)
{
	va_list arg;

	va_start(arg, format);
	vfprintf(stderr, format, arg);
	va_end(arg);
}
#else
#define logerror(...)
#endif
//#define logerror(...) fprintf(stderr, __VA_ARGS__)
//#define logerror(...)
//#define popmessage(...) fprintf(stderr, __VA_ARGS__)
#define popmessage(...)

/*****************************************************************************/
/* src/emu/devcpu.h */

// CPU interface functions
#define CPU_INIT_NAME(name)			cpu_init_##name
#define CPU_INIT(name)				void CPU_INIT_NAME(name)()
#define CPU_INIT_CALL(name)			CPU_INIT_NAME(name)()

#define CPU_RESET_NAME(name)			cpu_reset_##name
#define CPU_RESET(name)				void CPU_RESET_NAME(name)()
#define CPU_RESET_CALL(name)			CPU_RESET_NAME(name)()

#define CPU_EXECUTE_NAME(name)			cpu_execute_##name
#define CPU_EXECUTE(name)			void CPU_EXECUTE_NAME(name)()
#define CPU_EXECUTE_CALL(name)			CPU_EXECUTE_NAME(name)()

#define CPU_TRANSLATE_NAME(name)		cpu_translate_##name
#define CPU_TRANSLATE(name)			int CPU_TRANSLATE_NAME(name)(address_spacenum space, int intention, offs_t *address)
#define CPU_TRANSLATE_CALL(name)		CPU_TRANSLATE_NAME(name)(space, intention, address)

#define CPU_DISASSEMBLE_NAME(name)		cpu_disassemble_##name
#define CPU_DISASSEMBLE(name)			int CPU_DISASSEMBLE_NAME(name)(char *buffer, offs_t eip, const UINT8 *oprom)
#define CPU_DISASSEMBLE_CALL(name)		CPU_DISASSEMBLE_NAME(name)(buffer, eip, oprom)

/*****************************************************************************/
/* src/emu/didisasm.h */

// Disassembler constants
const UINT32 DASMFLAG_SUPPORTED     = 0x80000000;   // are disassembly flags supported?
const UINT32 DASMFLAG_STEP_OUT      = 0x40000000;   // this instruction should be the end of a step out sequence
const UINT32 DASMFLAG_STEP_OVER     = 0x20000000;   // this instruction should be stepped over by setting a breakpoint afterwards
const UINT32 DASMFLAG_OVERINSTMASK  = 0x18000000;   // number of extra instructions to skip when stepping over
const UINT32 DASMFLAG_OVERINSTSHIFT = 27;           // bits to shift after masking to get the value
const UINT32 DASMFLAG_LENGTHMASK    = 0x0000ffff;   // the low 16-bits contain the actual length

/*****************************************************************************/
/* src/emu/diexec.h */

// I/O line states
enum line_state
{
	CLEAR_LINE = 0,				// clear (a fired or held) line
	ASSERT_LINE,				// assert an interrupt immediately
	HOLD_LINE,				// hold interrupt line until acknowledged
	PULSE_LINE				// pulse interrupt line instantaneously (only for NMI, RESET)
};

// I/O line definitions
enum
{
	INPUT_LINE_IRQ = 0,
	INPUT_LINE_NMI
};

/*****************************************************************************/
/* src/emu/dimemory.h */

// Translation intentions
const int TRANSLATE_TYPE_MASK       = 0x03;     // read write or fetch
const int TRANSLATE_USER_MASK       = 0x04;     // user mode or fully privileged
const int TRANSLATE_DEBUG_MASK      = 0x08;     // debug mode (no side effects)

const int TRANSLATE_READ            = 0;        // translate for read
const int TRANSLATE_WRITE           = 1;        // translate for write
const int TRANSLATE_FETCH           = 2;        // translate for instruction fetch
const int TRANSLATE_READ_USER       = (TRANSLATE_READ | TRANSLATE_USER_MASK);
const int TRANSLATE_WRITE_USER      = (TRANSLATE_WRITE | TRANSLATE_USER_MASK);
const int TRANSLATE_FETCH_USER      = (TRANSLATE_FETCH | TRANSLATE_USER_MASK);
const int TRANSLATE_READ_DEBUG      = (TRANSLATE_READ | TRANSLATE_DEBUG_MASK);
const int TRANSLATE_WRITE_DEBUG     = (TRANSLATE_WRITE | TRANSLATE_DEBUG_MASK);
const int TRANSLATE_FETCH_DEBUG     = (TRANSLATE_FETCH | TRANSLATE_DEBUG_MASK);

/*****************************************************************************/
/* src/emu/emucore.h */

// constants for expression endianness
enum endianness_t
{
	ENDIANNESS_LITTLE,
	ENDIANNESS_BIG
};

// declare native endianness to be one or the other
#ifdef LSB_FIRST
const endianness_t ENDIANNESS_NATIVE = ENDIANNESS_LITTLE;
#else
const endianness_t ENDIANNESS_NATIVE = ENDIANNESS_BIG;
#endif

// endian-based value: first value is if 'endian' is little-endian, second is if 'endian' is big-endian
#define ENDIAN_VALUE_LE_BE(endian,leval,beval)	(((endian) == ENDIANNESS_LITTLE) ? (leval) : (beval))

// endian-based value: first value is if native endianness is little-endian, second is if native is big-endian
#define NATIVE_ENDIAN_VALUE_LE_BE(leval,beval)	ENDIAN_VALUE_LE_BE(ENDIANNESS_NATIVE, leval, beval)

// endian-based value: first value is if 'endian' matches native, second is if 'endian' doesn't match native
#define ENDIAN_VALUE_NE_NNE(endian,leval,beval)	(((endian) == ENDIANNESS_NATIVE) ? (neval) : (nneval))

/*****************************************************************************/
/* src/emu/memory.h */

// address spaces
enum address_spacenum
{
	AS_0,                           // first address space
	AS_1,                           // second address space
	AS_2,                           // third address space
	AS_3,                           // fourth address space
	ADDRESS_SPACES,                 // maximum number of address spaces

	// alternate address space names for common use
	AS_PROGRAM = AS_0,              // program address space
	AS_DATA = AS_1,                 // data address space
	AS_IO = AS_2                    // I/O address space
};

// offsets and addresses are 32-bit (for now...)
typedef UINT32	offs_t;
extern "C" void *wine_ldt_get_ptr(unsigned short sel, unsigned long offset);
void *read_ptr(offs_t byteaddress)
{
	return nullptr;
}
// read accessors
UINT8 read_byte(offs_t byteaddress)
{
#if defined(HAS_I386)
	if(byteaddress < MAX_MEM) {
		return mem[byteaddress];
//	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
//		return read_byte(byteaddress & 0xfffff);
	}
	return 0;
#else
	return mem[byteaddress];
#endif
}

UINT16 read_word(offs_t byteaddress)
{
#if defined(HAS_I386)
	if(byteaddress < MAX_MEM - 1) {
		return *(UINT16 *)(mem + byteaddress);
//	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
//		return read_word(byteaddress & 0xfffff);
	}
	return 0;
#else
	return *(UINT16 *)(mem + byteaddress);
#endif
}

UINT32 read_dword(offs_t byteaddress)
{
#if defined(HAS_I386)
	if(byteaddress < MAX_MEM - 3) {
		return *(UINT32 *)(mem + byteaddress);
//	} else if((byteaddress & 0xfffffff0) == 0xfffffff0) {
//		return read_dword(byteaddress & 0xfffff);
	}
	return 0;
#else
	return *(UINT32 *)(mem + byteaddress);
#endif
}

void write_byte(offs_t byteaddress, UINT8 data)
{
	/*
	if(byteaddress < MEMORY_END) {
		mem[byteaddress] = data;
	} else if(byteaddress >= text_vram_top_address && byteaddress < text_vram_end_address) {
		if(!restore_console_on_exit && (scr_width != 80 || scr_height != 25)) {
			change_console_size_to_80x25();
			restore_console_on_exit = true;
		}
		write_text_vram_byte(byteaddress - text_vram_top_address, data);
		mem[byteaddress] = data;
	} else if(byteaddress >= shadow_buffer_top_address && byteaddress < shadow_buffer_end_address) {
		if(int_10h_feh_called && !int_10h_ffh_called) {
			write_text_vram_byte(byteaddress - shadow_buffer_top_address, data);
		}
		mem[byteaddress] = data;
#if defined(HAS_I386)
	} else if(byteaddress < MAX_MEM) {
#else
	} else {
#endif
		mem[byteaddress] = data;
	}
	*/
	mem[byteaddress] = data;
}

void write_word(offs_t byteaddress, UINT16 data)
{
	*(UINT16 *)(mem + byteaddress) = data;
	/*
	if(byteaddress < MEMORY_END) {
		*(UINT16 *)(mem + byteaddress) = data;
	} else if(byteaddress >= text_vram_top_address && byteaddress < text_vram_end_address) {
		if(!restore_console_on_exit && (scr_width != 80 || scr_height != 25)) {
			change_console_size_to_80x25();
			restore_console_on_exit = true;
		}
		write_text_vram_word(byteaddress - text_vram_top_address, data);
		*(UINT16 *)(mem + byteaddress) = data;
	} else if(byteaddress >= shadow_buffer_top_address && byteaddress < shadow_buffer_end_address) {
		if(int_10h_feh_called && !int_10h_ffh_called) {
			write_text_vram_word(byteaddress - shadow_buffer_top_address, data);
		}
		*(UINT16 *)(mem + byteaddress) = data;
#if defined(HAS_I386)
	} else if(byteaddress < MAX_MEM - 1) {
#else
	} else {
#endif
		*(UINT16 *)(mem + byteaddress) = data;
	}*/
}

void write_dword(offs_t byteaddress, UINT32 data)
{
	*(UINT32 *)(mem + byteaddress) = data;
	/*
	if(byteaddress < MEMORY_END) {
		*(UINT32 *)(mem + byteaddress) = data;
	} else if(byteaddress >= text_vram_top_address && byteaddress < text_vram_end_address) {
		if(!restore_console_on_exit && (scr_width != 80 || scr_height != 25)) {
			change_console_size_to_80x25();
			restore_console_on_exit = true;
		}
		write_text_vram_dword(byteaddress - text_vram_top_address, data);
		*(UINT32 *)(mem + byteaddress) = data;
	} else if(byteaddress >= shadow_buffer_top_address && byteaddress < shadow_buffer_end_address) {
		if(int_10h_feh_called && !int_10h_ffh_called) {
			write_text_vram_dword(byteaddress - shadow_buffer_top_address, data);
		}
		*(UINT32 *)(mem + byteaddress) = data;
#if defined(HAS_I386)
	} else if(byteaddress < MAX_MEM - 3) {
#else
	} else {
#endif
		*(UINT32 *)(mem + byteaddress) = data;
	}
	*/
}

#define read_decrypted_byte read_byte
#define read_decrypted_word read_word
#define read_decrypted_dword read_dword

#define read_raw_byte read_byte
#define write_raw_byte write_byte

#define read_word_unaligned read_word
#define write_word_unaligned write_word

#define read_io_word_unaligned read_io_word
#define write_io_word_unaligned write_io_word

UINT8 read_io_byte(offs_t byteaddress);
UINT16 read_io_word(offs_t byteaddress);
UINT32 read_io_dword(offs_t byteaddress);

void write_io_byte(offs_t byteaddress, UINT8 data);
void write_io_word(offs_t byteaddress, UINT16 data);
void write_io_dword(offs_t byteaddress, UINT32 data);

/*****************************************************************************/
/* src/osd/osdcomm.h */

/* Highly useful macro for compile-time knowledge of an array size */
#define ARRAY_LENGTH(x)     (sizeof(x) / sizeof(x[0]))

#if defined(HAS_I386)
	static CPU_TRANSLATE(i386);
	#include "mame/lib/softfloat/softfloat.c"
	#include "mame/emu/cpu/i386/i386.c"
	#include "mame/emu/cpu/vtlb.c"
#elif defined(HAS_I286)
	#include "mame/emu/cpu/i86/i286.c"
#else
	#include "mame/emu/cpu/i86/i86.c"
#endif
#ifdef SUPPORT_DISASSEMBLER
	#include "mame/emu/cpu/i386/i386dasm.c"
	bool dasm = false;
#endif

#if defined(HAS_I386)
	#define SREG(x)				m_sreg[x].selector
	#define SREG_BASE(x)			m_sreg[x].base

	int cpu_type, cpu_step;
#else
	#define REG8(x)				m_regs.b[x]
	#define REG16(x)			m_regs.w[x]
	#define SREG(x)				m_sregs[x]
	#define SREG_BASE(x)			m_base[x]
	#define m_CF				m_CarryVal
	#define m_a20_mask			AMASK
	//#define i386_load_segment_descriptor(x)	m_base[x] = SegBase(x)
	void load_segment_descriptor_wine(int sreg);
#define i386_load_segment_descriptor(x) load_segment_descriptor_wine(x)
	#if defined(HAS_I286)
		#define i386_set_a20_line(x)	i80286_set_a20_line(x)
	#else
		#define i386_set_a20_line(x)
	#endif
	#define i386_set_irq_line(x, y)		set_irq_line(x, y)
#endif

void i386_jmp_far(UINT16 selector, UINT32 address)
{
#if defined(HAS_I386)
	if(PROTECTED_MODE && !V8086_MODE) {
		i386_protected_mode_jump(selector, address, 1, m_operand_size);
	} else {
		SREG(CS) = selector;
		m_performed_intersegment_jump = 1;
		i386_load_segment_descriptor(CS);
		m_eip = address;
		CHANGE_PC(m_eip);
	}
#elif defined(HAS_I286)
	i80286_code_descriptor(selector, address, 1);
#else
	SREG(CS) = selector;
	i386_load_segment_descriptor(CS);
	m_pc = (SREG_BASE(CS) + address) & m_a20_mask;
#endif
}

/* ----------------------------------------------------------------------------
MS-DOS virtual machine
---------------------------------------------------------------------------- */
void msdos_syscall(unsigned int a)
{
}

/* ----------------------------------------------------------------------------
	PC/AT hardware emulation
---------------------------------------------------------------------------- */

void hardware_init()
{
	CPU_INIT_CALL(CPU_MODEL);
	CPU_RESET_CALL(CPU_MODEL);
#if defined(HAS_I386)
	cpu_type = (REG32(EDX) >> 8) & 0x0f;
	cpu_step = (REG32(EDX) >> 0) & 0x0f;
#endif
	i386_set_a20_line(0);
}

void hardware_finish()
{
#if defined(HAS_I386)
	vtlb_free(m_vtlb);
#endif
}


int pic_ack()
{
	return 0;
}

// i/o bus

UINT8 read_io_byte(offs_t addr)
{
	return(0xff);
}

UINT16 read_io_word(offs_t addr)
{
	return(read_io_byte(addr) | (read_io_byte(addr + 1) << 8));
}

UINT32 read_io_dword(offs_t addr)
{
	return(read_io_byte(addr) | (read_io_byte(addr + 1) << 8) | (read_io_byte(addr + 2) << 16) | (read_io_byte(addr + 3) << 24));
}

void write_io_byte(offs_t addr, UINT8 val)
{
}

void write_io_word(offs_t addr, UINT16 val)
{
	write_io_byte(addr + 0, (val >> 0) & 0xff);
	write_io_byte(addr + 1, (val >> 8) & 0xff);
}

void write_io_dword(offs_t addr, UINT32 val)
{
	write_io_byte(addr + 0, (val >>  0) & 0xff);
	write_io_byte(addr + 1, (val >>  8) & 0xff);
	write_io_byte(addr + 2, (val >> 16) & 0xff);
	write_io_byte(addr + 3, (val >> 24) & 0xff);
}
#include <vector>
BOOL is_32bit_segment(int sreg);
extern "C"
{
    //kenel16_private.h
#include "../krnl386/kernel16_private.h"
#define KRNL386 "krnl386.exe16"
	PVOID dynamic_setWOW32Reserved(PVOID w)
	{
		static PVOID(*setWOW32Reserved)(PVOID);
		if (!setWOW32Reserved)
		{
			HMODULE krnl386 = LoadLibraryA(KRNL386);
			setWOW32Reserved = (PVOID(*)(PVOID))GetProcAddress(krnl386, "setWOW32Reserved");
		}
		return setWOW32Reserved(w);
	}
	PVOID dynamic_getWOW32Reserved()
	{
		static PVOID(*getWOW32Reserved)();
		if (!getWOW32Reserved)
		{
			HMODULE krnl386 = LoadLibraryA(KRNL386);
			getWOW32Reserved = (PVOID(*)())GetProcAddress(krnl386, "getWOW32Reserved");
		}
		return getWOW32Reserved();
	}
	void dynamic__wine_call_int_handler(CONTEXT *context, BYTE intnum)
	{
		static void(*WINAPI __wine_call_int_handler)(CONTEXT *context, BYTE intnum);
		if (!__wine_call_int_handler)
		{
			HMODULE krnl386 = LoadLibraryA(KRNL386);
			__wine_call_int_handler = (void(*)(CONTEXT *context, BYTE intnum))GetProcAddress(krnl386, "__wine_call_int_handler");
		}
		return __wine_call_int_handler(context, intnum);
	}
//	__declspec(dllimport) PVOID getWOW32Reserved();
//	__declspec(dllimport) PVOID setWOW32Reserved(PVOID w);
//	unsigned short wine_ldt_alloc_entries(int count);
//	int wine_ldt_set_entry(unsigned short sel, const LDT_ENTRY *entry);
//	void wine_ldt_get_entry(unsigned short sel, LDT_ENTRY *entry);
//    struct __wine_ldt_copy
//    {
//        void         *base[8192];  /* base address or 0 if entry is free   */
//        unsigned long limit[8192]; /* limit in bytes or 0 if entry is free */
//        unsigned char flags[8192]; /* flags (defined below) */
//    };
//	_declspec(dllimport) struct __wine_ldt_copy wine_ldt_copy;
    _declspec(dllimport) LDT_ENTRY wine_ldt[8192];
//#define WINE_LDT_FLAGS_DATA      0x13  /* Data segment */
//#define WINE_LDT_FLAGS_CODE      0x1b  /* Code segment */
	/* helper functions to manipulate the LDT_ENTRY structure */
	/*static inline void wine_ldt_set_base(LDT_ENTRY *ent, const void *base)
	{
		ent->BaseLow = (WORD)(ULONG_PTR)base;
		ent->HighWord.Bits.BaseMid = (BYTE)((ULONG_PTR)base >> 16);
		ent->HighWord.Bits.BaseHi = (BYTE)((ULONG_PTR)base >> 24);
	}
	static inline void wine_ldt_set_limit(LDT_ENTRY *ent, unsigned int limit)
	{
		if ((ent->HighWord.Bits.Granularity = (limit >= 0x100000))) limit >>= 12;
		ent->LimitLow = (WORD)limit;
		ent->HighWord.Bits.LimitHi = (limit >> 16);
	}
	static inline void *wine_ldt_get_base(const LDT_ENTRY *ent)
	{
		return (void *)(ent->BaseLow |
			(ULONG_PTR)ent->HighWord.Bits.BaseMid << 16 |
			(ULONG_PTR)ent->HighWord.Bits.BaseHi << 24);
	}
	static inline unsigned int wine_ldt_get_limit(const LDT_ENTRY *ent)
	{
		unsigned int limit = ent->LimitLow | (ent->HighWord.Bits.LimitHi << 16);
		if (ent->HighWord.Bits.Granularity) limit = (limit << 12) | 0xfff;
		return limit;
	}
#define WINE_LDT_FLAGS_32BIT     0x40  /* Segment is 32-bit (code or stack) *//*
	static inline void wine_ldt_set_flags(LDT_ENTRY *ent, unsigned char flags)
	{
		ent->HighWord.Bits.Dpl = 3;
		ent->HighWord.Bits.Pres = 1;
		ent->HighWord.Bits.Type = flags;
		ent->HighWord.Bits.Sys = 0;
		ent->HighWord.Bits.Reserved_0 = 0;
		ent->HighWord.Bits.Default_Big = (flags & WINE_LDT_FLAGS_32BIT) != 0;
	}
	static inline unsigned char wine_ldt_get_flags(const LDT_ENTRY *ent)
	{
		unsigned char ret = ent->HighWord.Bits.Type;
		if (ent->HighWord.Bits.Default_Big) ret |= WINE_LDT_FLAGS_32BIT;
		return ret;
	}
	static inline int wine_ldt_is_empty(const LDT_ENTRY *ent)
	{
		const DWORD *dw = (const DWORD *)ent;
		return (dw[0] | dw[1]) == 0;
	}*/
	/***********************************************************************
	*           SELECTOR_SetEntries
	*
	* Set the LDT entries for an array of selectors.
	*/
	static BOOL SELECTOR_SetEntries(WORD sel, const void *base, DWORD size, unsigned char flags)
	{
		LDT_ENTRY entry;
		WORD i, count;

		wine_ldt_set_base(&entry, base);
		wine_ldt_set_limit(&entry, size - 1);
		wine_ldt_set_flags(&entry, flags);
		count = (size + 0xffff) / 0x10000;
		for (i = 0; i < count; i++)
		{
			if (wine_ldt_set_entry(sel + (i << 3), &entry) < 0) return FALSE;
			wine_ldt_set_base(&entry, (char*)wine_ldt_get_base(&entry) + 0x10000);
			/* yep, Windows sets limit like that, not 64K sel units */
			wine_ldt_set_limit(&entry, wine_ldt_get_limit(&entry) - 0x10000);
		}
		return TRUE;
	}
	void wine_ldt_free_entries(unsigned short sel, int count);
	/***********************************************************************
	*           SELECTOR_AllocBlock
	*
	* Allocate selectors for a block of linear memory.
	*/
	WORD SELECTOR_AllocBlock(const void *base, DWORD size, unsigned char flags)
	{
		WORD sel, count;

		if (!size) return 0;
		count = (size + 0xffff) / 0x10000;
		if ((sel = wine_ldt_alloc_entries(count)))
		{
			if (SELECTOR_SetEntries(sel, base, size, flags)) return sel;
			wine_ldt_free_entries(sel, count);
			sel = 0;
		}
		return sel;
	}
	void save_context(CONTEXT *context)
	{
		context->Eax = REG16(AX);
		context->Ecx = REG16(CX);
		context->Edx = REG16(DX);
		context->Ebx = REG16(BX);
		context->Esp = REG16(SP);
		context->Ebp = REG16(BP);
		context->Esi = REG16(SI);
		context->Edi = REG16(DI);
		context->Ebp = REG16(BP);
		context->Eip = m_eip;
		context->SegEs = SREG(ES);
		context->SegCs = SREG(CS);
		context->SegSs = SREG(SS);
		context->SegDs = SREG(DS);
		context->SegFs = SREG(FS);
		context->SegGs = SREG(GS);
        context->EFlags = m_eflags;// &~0x20000;
		dynamic_setWOW32Reserved((PVOID)(SREG(SS) << 16 | REG16(SP)));
	}
	void load_context(CONTEXT *context)
	{
		REG16(AX) = (WORD)context->Eax;
		REG16(CX) = (WORD)context->Ecx;
		REG16(DX) = (WORD)context->Edx;
		REG16(BX) = (WORD)context->Ebx;
		REG16(SP) = (WORD)context->Esp;
		REG16(BP) = (WORD)context->Ebp;
		REG16(SI) = (WORD)context->Esi;
		REG16(DI) = (WORD)context->Edi;
		REG16(BP) = (WORD)context->Ebp;
		SREG(ES) = (WORD)context->SegEs;
		SREG(CS) = (WORD)context->SegCs;
		SREG(SS) = (WORD)context->SegSs;
		SREG(DS) = (WORD)context->SegDs;//ES, CS, SS, DS
		//ES, CS, SS, DS, FS, GS
		SREG(FS) = (WORD)context->SegFs;
		SREG(GS) = (WORD)context->SegGs;
		i386_load_segment_descriptor(ES);
		i386_load_segment_descriptor(SS);
		i386_load_segment_descriptor(DS);
		i386_load_segment_descriptor(FS);
		i386_load_segment_descriptor(GS);
		m_eip = context->Eip;
		i386_jmp_far(SREG(CS), context->Eip);
		set_flags(context->EFlags);
	}
	void WINAPI DOSVM_Int21Handler(CONTEXT *context);
	unsigned char table[256 * 4 + 2 + 0x8 * 256] = { 0xcf };
	unsigned char iret[256] = { 0xcf };
	WORD SELECTOR_AllocBlock(const void *base, DWORD size, unsigned char flags);
#include <imagehlp.h>

#pragma comment(lib, "imagehlp.lib")
	const size_t stack_frame_size = 100;
	thread_local void* current_stack_frame[100];
	thread_local size_t current_stack_frame_size;
	void capture_stack_trace()
	{
		auto process = GetCurrentProcess();
		current_stack_frame_size = CaptureStackBackTrace(0, stack_frame_size, current_stack_frame, NULL);
	}
	void dump_stack_trace(void)
	{
		unsigned int   i;
		SYMBOL_INFO  * symbol;
		HANDLE         process;

		process = GetCurrentProcess();

		SymInitialize(process, NULL, TRUE);

		symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
		symbol->MaxNameLen = 255;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		for (i = 0; i < current_stack_frame_size; i++)
		{
			IMAGEHLP_LINE line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE);
			DWORD d = 0;
			auto success = SymGetLineFromAddr(process, (DWORD64)current_stack_frame[i], &d, &line);
			SymFromAddr(process, (DWORD64)(current_stack_frame[i]), 0, symbol);
			printf("%i: %s - 0x%llx %s(%d)\n", current_stack_frame_size - i - 1, symbol->Name, symbol->Address, line.FileName, line.LineNumber);
			if (!strcmp(symbol->Name, "KiUserExceptionDispatcher"))
			{
				printf("=============================\n");
			}
		}

		free(symbol);
		current_stack_frame_size = 0;
	}
	LONG NTAPI vm86_vectored_exception_handler(struct _EXCEPTION_POINTERS *ExceptionInfo)
	{
		capture_stack_trace();
		return EXCEPTION_CONTINUE_SEARCH;
	}
	__declspec(dllexport) BOOL init_vm86(BOOL is_vm86)
	{

		AddVectoredExceptionHandler(TRUE, vm86_vectored_exception_handler);
		WORD sel = SELECTOR_AllocBlock(iret, 256, WINE_LDT_FLAGS_CODE);
		CPU_INIT_CALL(CPU_MODEL);
		//enable x87
		build_x87_opcode_table();
		build_opcode_table(OP_I386 | OP_FPU);
		CPU_RESET_CALL(CPU_MODEL);
        UINT8 *base = 0;//mem;
		m_idtr.base = (UINT32)(table - base);
        m_ldtr.limit = 8192;
        m_ldtr.base = (UINT32)((UINT8*)&wine_ldt - base);
        m_gdtr.limit = 8192;
        m_gdtr.base = (UINT32)((UINT8*)&wine_ldt - base);
        m_CPL = 3;
        wine_ldt[4].HighWord.Bits.Type = 0x18;
        wine_ldt[4].HighWord.Bits.Pres = 1;
        wine_ldt[4].HighWord.Bits.Sys = 1;
        wine_ldt[4].LimitLow = 0xFFFF;
        wine_ldt[4].HighWord.Bits.LimitHi = 0xF;
        wine_ldt[4].HighWord.Bits.Default_Big = 1;
        wine_ldt[4].HighWord.Bits.Granularity = 1;
        wine_ldt[4].HighWord.Bits.Dpl = m_CPL;
		memset(iret, 0xcf, 256);
		for (int i = 0; i < 256; i++)
		{
			*(WORD*)&table[i * 4 + 2] = sel;
			*(WORD*)&table[i * 4] = i;
			*(WORD*)&table[i * 4 + 2] = sel;
			*(WORD*)&table[i * 4] = i;
		}
        //PROTECTED MODE
        for (int i = 0; i < 256; i++)
        {
            *(WORD*)&table[i * 8] = i;
            *(WORD*)&table[i * 8 + 2] = sel;
            *(BYTE*)&table[i * 8 + 4] = 0x00;
            *(BYTE*)&table[i * 8 + 5] = 0x66 | 0x80;
            *(WORD*)&table[i * 8 + 6] = 0;
        }
#if defined(HAS_I386)
		cpu_type = (REG32(EDX) >> 8) & 0x0f;
		cpu_step = (REG32(EDX) >> 0) & 0x0f;
#endif
		//m_amask = -1;
		m_a20_mask = -1;
        if (is_vm86)
        {
            m_cr[0] |= 1;//PROTECTED MODE
            set_flags(0x20000);//V8086_MODE
           //assert(V8086_MODE);
        }
        else
        {
            m_cr[0] |= 1;//PROTECTED MODE
        }return TRUE;
	}
    DWORD mergeReg(DWORD a1, DWORD a2)
    {
        return a1 == a2 ? a1 : a2;
    }
	BOOL initflag;
	void vm86main(CONTEXT *context, DWORD cbArgs, PEXCEPTION_HANDLER handler,
		void(*from16_reg)(void),
		LONG(*__wine_call_from_16)(void),
		int(*relay_call_from_16)(void *entry_point, unsigned char *args16, CONTEXT *context),
		void(*__wine_call_to_16_ret)(void),
		bool dasm
		);
	//__declspec(dllexport) void wine_call_to_16_regs_vm86(CONTEXT *context, DWORD cbArgs, PEXCEPTION_RECORD handler);void wine_call_to_16_regs_vm86(CONTEXT *context, DWORD cbArgs, PEXCEPTION_RECORD handler,
	void wine_call_to_16_regs_vm86(CONTEXT *context, DWORD cbArgs, PEXCEPTION_HANDLER handler,
		void(*from16_reg)(void),
		LONG(*__wine_call_from_16)(void),
		int(*relay_call_from_16)(void *entry_point, unsigned char *args16, CONTEXT *context),
		void(*__wine_call_to_16_ret)(void),
		bool dasm,
        bool vm86,
        void *memory_base
		)
	{
        mem = vm86 ? (UINT8*)memory_base : NULL;
		if (!initflag)
			initflag = init_vm86(vm86);
		vm86main(context, cbArgs, handler, from16_reg, __wine_call_from_16, relay_call_from_16, __wine_call_to_16_ret, dasm);
	}	
	DWORD wine_call_to_16_vm86(DWORD target, DWORD cbArgs, PEXCEPTION_HANDLER handler,
		void(*from16_reg)(void),
		LONG(*__wine_call_from_16)(void),
		int(*relay_call_from_16)(void *entry_point, unsigned char *args16, CONTEXT *context),
		void(*__wine_call_to_16_ret)(void),
		bool dasm,
        bool vm86,
        void *memory_base
		)
	{
        mem = vm86 ? (UINT8*)memory_base : NULL;
		if (!initflag)
			initflag = init_vm86(vm86);
		CONTEXT context;
        PVOID oldstack = dynamic_getWOW32Reserved();
		save_context(&context);
		//why??
		dynamic_setWOW32Reserved(oldstack);
		context.SegSs = ((size_t)dynamic_getWOW32Reserved() >> 16) & 0xFFFF;
		context.Esp = ((size_t)dynamic_getWOW32Reserved()) & 0xFFFF;
		context.SegCs = target >> 16;
		context.Eip = target & 0xFFFF;//i386_jmp_far(target >> 16, target & 0xFFFF);
		vm86main(&context, cbArgs, handler, from16_reg, __wine_call_from_16, relay_call_from_16, __wine_call_to_16_ret, dasm);
		return context.Eax | context.Edx << 16;
	}
	UINT old_eip = 0;
	struct dasm_buffer
	{
		size_t index;
		size_t current_size = 0;
		const size_t size = 1000;
		std::vector<char[1000]> buffer;
		dasm_buffer(size_t cap) : buffer(cap), size(cap)
		{

		}
		char *get_current()
		{
			char *buf = buffer[index];
			buf[0] = '\0';
			current_size++;
			index = (index + 1) % size;
			return buf;
		}
		void dump()
		{
			size_t base = current_size < size ? 0 : (index + size) % size;
			for (int i = 0; i < current_size && i < size; i++)
			{
				fprintf(stderr, "%s", buffer[(base + i) % size]);
			}
		}
	};
	struct dasm_buffer dasm_buffer(1000);
	//for debug
	__declspec(dllexport) void dasm_buffer_dump()
	{
		dasm_buffer.dump();
	}
	LONG catch_exception(_EXCEPTION_POINTERS *ep, PEXCEPTION_ROUTINE er);
	void vm86main(CONTEXT *context, DWORD cbArgs, PEXCEPTION_HANDLER handler,
		void(*from16_reg)(void),
		LONG(*__wine_call_from_16)(void),
		int(*relay_call_from_16)(void *entry_point,	unsigned char *args16, CONTEXT *context),
		void(*__wine_call_to_16_ret)(void),
		bool dasm
		)
	{
		bool dasm_buffering = false;
		if (dasm == 2)
		{
			dasm_buffering = true;
		}
		__try
        {
			if (!initflag)
				initflag = init_vm86(false);
			//wine_ldt_copy.base[0] = iret;
			REG16(AX) = (WORD)context->Eax;
			REG16(CX) = (WORD)context->Ecx;
			REG16(DX) = (WORD)context->Edx;
			REG16(BX) = (WORD)context->Ebx;
			REG16(SP) = (WORD)context->Esp - cbArgs;
			REG16(BP) = (WORD)context->Ebp;
			REG16(SI) = (WORD)context->Esi;
			REG16(DI) = (WORD)context->Edi;
			SREG(ES) = (WORD)context->SegEs;
			SREG(CS) = (WORD)context->SegCs;
			SREG(SS) = (WORD)context->SegSs;
			SREG(DS) = (WORD)context->SegDs;//ES, CS, SS, DS
			//ES, CS, SS, DS, FS, GS
			SREG(FS) = (WORD)context->SegFs;
			SREG(GS) = (WORD)context->SegGs;
			i386_load_segment_descriptor(ES);
			i386_jmp_far(context->SegCs, context->Eip);
			i386_load_segment_descriptor(SS);
			i386_load_segment_descriptor(DS);
			i386_load_segment_descriptor(FS);
			i386_load_segment_descriptor(GS);
            set_flags(context->EFlags);
			m_IOP1 = 1;
			m_IOP2 = 1;
			m_eflags |= 0x3000;
			DWORD ret_addr = 0;
			//IOPL = 3;
			if (cbArgs >= 2)
			{
				unsigned char *stack = (unsigned char*)i386_translate(SS, REG16(SP), 0);
				ret_addr = *(DWORD*)stack;
			}
            bool isVM86mode = false;
			//dasm = true;
			while (!m_halted) {
                //merge_vm86_pending_flags
                if (get_vm86_teb_info()->vm86_pending & 0x100000)//VIP flag
                {
                    if (!V8086_MODE)
                    {

                    }
                    else
                    {
                        //dlls/ntdll/signal_i386.c merge_vm86_pending_flags
                        BOOL check_pending = TRUE;
                        /*
                        * In order to prevent a race when SIGUSR2 occurs while
                        * we are returning from exception handler, pending events
                        * will be rechecked after each raised exception.
                        */
                        while (check_pending && get_vm86_teb_info()->vm86_pending)
                        {
                            check_pending = FALSE;

                            //x86_thread_data()->vm86_ptr = NULL;

                            /*
                            * If VIF is set, throw exception.
                            * Note that SIGUSR2 may turn VIF flag off so
                            * VIF check must occur only when TEB.vm86_ptr is NULL.
                            */
                            if (1||m_VIF)
                            {
                                CONTEXT vcontext = {};
                                save_context(&vcontext);

                                EXCEPTION_RECORD reca, *rec = &reca;
                                rec->ExceptionCode = 0x80000111;
                                rec->ExceptionFlags = 0;
                                rec->ExceptionRecord = NULL;
                                rec->NumberParameters = 0;
                                rec->ExceptionAddress = (LPVOID)vcontext.Eip;

                                vcontext.EFlags &= ~0x100000;
                                get_vm86_teb_info()->vm86_pending = 0;
                                //dosvm.c exception_handler
                                CONTEXT *ppcontext = &vcontext;
                                ULONG exception_args[1 + sizeof(CONTEXT*) / sizeof(ULONG)] = {};
                                *(CONTEXT**)(&exception_args[1]) = ppcontext;
                                RaiseException(0x80000111, 0, sizeof(exception_args) / sizeof(ULONG), (ULONG_PTR*)exception_args);
                                //NTSTATUS a = NtRaiseException(rec, &vcontext, TRUE);

                                load_context(&vcontext);
                                check_pending = TRUE;
                            }

                            //x86_thread_data()->vm86_ptr = vm86;
                        }

                        /*
                        * Merge VIP flags in a signal safe way. This requires
                        * that the following operation compiles into atomic
                        * instruction.
                        */
                        set_flags(get_flags() | get_vm86_teb_info()->vm86_pending);
                    }
                }
				if ((m_eip & 0xFFFF) == (ret_addr & 0xFFFF) && SREG(CS) == ret_addr >> 16)
				{
					__wine_call_to_16_ret();
					break;//return VM
				}
				bool reg = false;
				if (m_pc >= (UINT)/*ptr!*/iret && m_pc <= (UINT)/*ptr!*/iret + 255)
				{
					CONTEXT context;
                    //GP
                    /*if (m_pc - (UINT)iret == 0x0d)
                    {
                        WORD err = POP16();
                        WORD ip = POP16();
                        WORD cs = POP16();
                        WORD oldflags = POP16();
                        EXCEPTION_RECORD rec = {};
                        rec.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
                        save_context(&context);
                        context.SegCs = cs;
                        context.Eip = ip;
                        EXCEPTION_DISPOSITION result = handler(&rec, NULL, &context, NULL);
                        if (result == ExceptionContinueExecution)
                        {
                            load_context(&context);
                            continue;
                        }
                    }
                    else*/
                    {
                        WORD ip = POP16();
                        WORD cs = POP16();
                        PUSH16(cs);
                        PUSH16(ip);
                        save_context(&context);
                        DWORD cs2 = context.SegCs;
                        DWORD eip2 = context.Eip;
                        context.Eip = ip;
                        context.SegCs = cs;
                        //wine int handler はCS:IPをいじる場合がある
                        dynamic__wine_call_int_handler(&context, m_pc - (UINT)/*ptr!*/iret);
                        WORD ip3 = context.Eip;
                        WORD cs3 = context.SegCs;
                        context.SegCs = cs2;
                        context.Eip = eip2;
                        load_context(&context);
                        WORD a = POP16();
                        WORD b = POP16();
                        WORD c = POP16();
                        PUSH16((WORD)context.EFlags);
                        PUSH16(cs3);
                        PUSH16(ip3);
                    }
				}
				if ((void(*)(void))m_eip == from16_reg)
				{
					reg = true;
				}
				if ((LONG(*)(void))m_eip == __wine_call_from_16 || reg)
				{
					unsigned char *stack1 = (unsigned char*)i386_translate(SS, REG16(SP), 0);
					unsigned char *stack = stack1;
					/*
					* (sp+24) word   first 16-bit arg
					* (sp+22) word   cs
					* (sp+20) word   ip
					* (sp+18) word   bp
					* (sp+14) long   32-bit entry point (reused for Win16 mutex recursion count)
					* (sp+12) word   ip of actual entry point (necessary for relay debugging)
					* (sp+8)  long   relay (argument conversion) function entry point
					* (sp+4)  long   cs of 16-bit entry point
					* (sp)    long   ip of 16-bit entry point
					*/
					DWORD ip = *(DWORD*)stack;
					stack += sizeof(DWORD);
					DWORD cs = *(DWORD*)stack;
					stack += sizeof(DWORD);
					DWORD relay = *(DWORD*)stack;
					stack += sizeof(DWORD);
					WORD ip2 = *(WORD*)stack;
					stack += sizeof(WORD);
					DWORD entry = *(DWORD*)stack;
					//for debug
					void *entryf = (void*)entry;
					stack += sizeof(DWORD);
					WORD bp = *(WORD*)stack;
					stack += sizeof(WORD);
					WORD ip19 = *(WORD*)stack;
					stack += sizeof(WORD);
					WORD cs16 = *(WORD*)stack;
					stack += sizeof(WORD);
					WORD *args = (WORD*)stack;
					m_eip = ip;
					m_sreg[CS].selector = cs;
					i386_load_segment_descriptor(CS);
					CHANGE_PC(m_eip);
					if (relay == (UINT)relay_call_from_16 || 1)
					{
#include <pshpack1.h>
                        /* 16-bit stack layout after __wine_call_from_16() */
                        typedef struct _STACK16FRAME
                        {
                            struct STACK32FRAME *frame32;        /* 00 32-bit frame from last CallTo16() */
                            DWORD         edx;            /* 04 saved registers */
                            DWORD         ecx;            /* 08 */
                            DWORD         ebp;            /* 0c */
                            WORD          ds;             /* 10 */
                            WORD          es;             /* 12 */
                            WORD          fs;             /* 14 */
                            WORD          gs;             /* 16 */
                            DWORD         callfrom_ip;    /* 18 callfrom tail IP */
                            DWORD         module_cs;      /* 1c module code segment */
                            DWORD         relay;          /* 20 relay function address */
                            WORD          entry_ip;       /* 22 entry point IP */
                            DWORD         entry_point;    /* 26 API entry point to call, reused as mutex count */
                            WORD          bp;             /* 2a 16-bit stack frame chain */
                            WORD          ip;             /* 2c return address */
                            WORD          cs;             /* 2e */
                        } STACK16FRAME;
#include <poppack.h>
						CONTEXT context;
						WORD osp = REG16(SP);
						PUSH16(SREG(GS));
						PUSH16(SREG(FS));
						PUSH16(SREG(ES));
						PUSH16(SREG(DS));
						PUSH32(REG32(EBP));
						PUSH32(REG32(ECX));
						PUSH32(REG32(EDX));
						PUSH32(/*context.Esp*/osp);
						save_context(&context);
                        STACK16FRAME *oa = (STACK16FRAME*)wine_ldt_get_ptr(context.SegSs, context.Esp);
						DWORD ooo = (WORD)context.Esp;
						int fret;
						if (relay != (UINT)relay_call_from_16)
						{
							fret = relay_call_from_16((void*)entry, (unsigned char*)args, &context);
						}
						else
							fret = ((int(*)(void *, unsigned char *, CONTEXT *))relay)((void*)entry, (unsigned char*)args, &context);
						//int fret = relay_call_from_16((void*)entry, (unsigned char*)args, &context);
						if (!reg)
						{
							//REG16(AX) = fret & 0xFFFF;
							//REG16(DX) = (fret >> 16) & 0xFFFF;
							//shldされる
							REG32(EAX) = fret;
						}
                        oa = (STACK16FRAME*)wine_ldt_get_ptr(context.SegSs, context.Esp);
						if (reg) REG16(AX) = (WORD)context.Eax;
						REG16(CX) = reg ? (WORD)context.Ecx : (WORD)oa->ecx;
						if (reg) REG16(DX) = (WORD)context.Edx;
                        else
                            REG16(DX) = (WORD)oa->edx;
						REG16(BX) = (WORD)context.Ebx;
						REG16(SP) = (WORD)context.Esp;
						REG16(BP) = (WORD)context.Ebp;
						REG16(SI) = (WORD)context.Esi;
						REG16(DI) = (WORD)context.Edi;
                        SREG(ES) = reg ? (WORD)context.SegEs : (WORD)oa->es;
						SREG(CS) = (WORD)context.SegCs;
						SREG(SS) = (WORD)context.SegSs;
						SREG(DS) = reg ? (WORD)context.SegDs : (WORD)oa->ds;//ES, CS, SS, DS
						//ES, CS, SS, DS, FS, GS
						SREG(FS) = (WORD)context.SegFs;
						SREG(GS) = (WORD)context.SegGs;
						REG16(SP) = osp + 18 + 2;
						REG16(SP) -= (ooo - context.Esp);
						REG16(BP) = bp;
						set_flags(context.EFlags);
						i386_load_segment_descriptor(ES);
						i386_load_segment_descriptor(SS);
						i386_load_segment_descriptor(DS);
						i386_load_segment_descriptor(FS);
						i386_load_segment_descriptor(GS);
						m_eip = context.Eip;
						i386_jmp_far(SREG(CS), context.Eip);
					}
				}
				if (is_32bit_segment(CS))
				{
					m_VM = 0;
					m_eflags &= ~0x20000;
					//????
					m_sreg[CS].d = 1;
					m_operand_size = 1;
				}
				else
				{
					if (dasm && !m_VM)
					{
						//printf("==ENTER 16BIT CODE==\n");
					}
                    if (isVM86mode)
                    {
                        m_VM = 1;
                        m_eflags |= 0x20000;
                    }
                    m_sreg[CS].d = 0;
					m_operand_size = 0;
				}
#ifdef SUPPORT_DISASSEMBLER
				if (dasm) {
					char buffer[256];
#if defined(HAS_I386)
					UINT64 eip = m_eip;
#else
					UINT64 eip = m_pc - SREG_BASE(CS);
#endif
					UINT8 *oprom = mem + SREG_BASE(CS) + eip;

					char *dbuf = NULL;
					if (dasm_buffering)
					{
						dbuf = dasm_buffer.get_current();
						dbuf += sprintf(dbuf, "%04x:%04x", SREG(CS), (unsigned)eip);
					}
					else
					{
						fprintf(stderr, "%04x:%04x", SREG(CS), (unsigned)eip);
						fflush(stderr);
					}
					int result;
#if defined(HAS_I386)
					if (m_operand_size) {
                        result = CPU_DISASSEMBLE_CALL(x86_32);
					}
					else
#endif
                        result = i386_dasm_one_ex(buffer, m_eip, oprom, 16);//CPU_DISASSEMBLE_CALL(x86_16);
                    int opsize = result & 0xFF;
                    while (opsize--)
                    {
						if (dasm_buffering)
							dbuf += sprintf(dbuf, "%02X", *oprom);
						else
	                        fprintf(stderr, "%02X", *oprom);
                        oprom++;
                    }
					if (dasm_buffering)
						dbuf += sprintf(dbuf, "\t%s\n", buffer);
					else
						fprintf(stderr, "\t%s\n", buffer);
				}
#endif
                if (V8086_MODE)
                {
                    UINT8 *op = mem + SREG_BASE(CS) + m_eip;
                    UINT8 vec;
                    if (*op == 0xCD)//INT imm8
                    {

                        vec = *(op + 1);

                        CONTEXT context;
                        WORD ip = m_eip;
                        WORD cs = SREG(CS);
                        PUSH16(cs);
                        PUSH16(ip);
                        save_context(&context);
                        DWORD cs2 = context.SegCs;
                        DWORD eip2 = context.Eip;
                        context.Eip = ip;
                        context.SegCs = cs;
                        //wine int handler はCS:IPをいじる場合がある
                        dynamic__wine_call_int_handler(&context, vec);
                        WORD ip3 = context.Eip;
                        WORD cs3 = context.SegCs;
                        context.SegCs = cs2;
                        context.Eip = eip2;
                        load_context(&context);
                        WORD a = POP16();
                        WORD b = POP16();
                        m_eip += 2;
                        continue;
                    }
                    else if (*op == 0xCC)
                    {
                        vec = 0x03;
                        m_eip += 1;
                        continue;
                    }
                }
#if defined(HAS_I386)
				m_cycles = 1;
				CPU_EXECUTE_CALL(i386);
#else
				CPU_EXECUTE_CALL(CPU_MODEL);
#endif
			}
			save_context(context);
		}
		__except (catch_exception(GetExceptionInformation(), (PEXCEPTION_ROUTINE)handler))
		{
		}
	}

#include <imagehlp.h>

#pragma comment(lib, "imagehlp.lib")

    void printStack(void)
    {
        unsigned int   i;
        void         * stack[100];
        unsigned short frames;
        SYMBOL_INFO  * symbol;
        HANDLE         process;

        process = GetCurrentProcess();

        SymInitialize(process, NULL, TRUE);

        frames = CaptureStackBackTrace(0, 100, stack, NULL);
        symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
        symbol->MaxNameLen = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

        for (i = 0; i < frames; i++)
        {
            SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);

            printf("%i: %s - 0x%p\n", frames - i - 1, symbol->Name, symbol->Address);
        }

        free(symbol);
    }
	LONG catch_exception(_EXCEPTION_POINTERS *ep, PEXCEPTION_ROUTINE winehandler)
	{
		PEXCEPTION_RECORD rec = ep->ExceptionRecord;
		if (rec->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
			return EXCEPTION_CONTINUE_SEARCH;
        CONTEXT context;
        save_context(&context);
        //return winehandler(ep->ExceptionRecord, NULL, &context, NULL);
        char buffer[256] = {}, buffer2[256] = {};
		if (!1) {
#if defined(HAS_I386)
			UINT64 eip = m_eip;
#else
			UINT64 eip = m_pc - SREG_BASE(CS);
#endif
			UINT8 *oprom = mem + SREG_BASE(CS) + eip;
#if defined(HAS_I386)
			if (m_operand_size) {
				CPU_DISASSEMBLE_CALL(x86_32);
			}
			else
#endif
				i386_dasm_one_ex(buffer, m_eip, oprom, 16);//CPU_DISASSEMBLE_CALL(x86_16);
			oprom = mem + SREG_BASE(CS) + m_prev_eip;
#if defined(HAS_I386)
			if (m_operand_size) {
				CPU_DISASSEMBLE_CALL(x86_32);
			}
			else
#endif
				i386_dasm_one_ex(buffer2, m_prev_eip, oprom, 16);//CPU_DISASSEMBLE_CALL(x86_16);
		}
		char buf[2048];
		sprintf(buf, 
"Access violation\naddress=%p\naccess address=%p\n\
%dbit\n\
16bit context\n\
AX:%04X,CX:%04X,DX:%04X,BX:%04X\n\
SP:%04X,BP:%04X,SI:%04X,DI:%04X\n\
ES:%04X,SS:%04X,CS:%04X,DS:%04X\n\
IP:%04X, address:%08X\n\
%s\n%s", rec->ExceptionAddress, (void*)rec->ExceptionInformation[1],
m_VM ? 16 : 32,
REG16(AX), REG16(CX), REG16(DX), REG16(BX), REG16(SP), REG16(BP), REG16(SI), REG16(DI),
SREG(ES), SREG(CS), SREG(SS), SREG(DS), m_eip, m_pc, buffer2, buffer);
		dump_stack_trace();
        printStack();
		/*
		
					context.Eax = REG16(AX);
					context.Ecx = REG16(CX);
					context.Edx = REG16(DX);
					context.Ebx = REG16(BX);
					context.Esp = REG16(SP);
					context.Ebp = REG16(BP);
					context.Esi = REG16(SI);
					context.Edi = REG16(DI);
					context.SegEs = SREG(ES);
					context.SegCs = SREG(CS);
					context.SegSs = SREG(SS);
					context.SegDs = SREG(DS);*/
        fprintf(stderr, "%s", buf);
#ifdef _DEBUG
        _getch();
#endif
		MessageBoxA(NULL, buf, "SEGV", MB_ICONERROR);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	void *wine_ldt_get_ptr(unsigned short sel, unsigned long offset);
	void wine_ldt_get_entry(unsigned short sel, LDT_ENTRY *entry);
	/*static inline unsigned char wine_ldt_get_flags(const LDT_ENTRY *ent)
	{
		unsigned char ret = ent->HighWord.Bits.Type;
		if (ent->HighWord.Bits.Default_Big) ret |= WINE_LDT_FLAGS_32BIT;
		return ret;
	}*/
}
BOOL is_32bit_segment(int sreg)
{
	LDT_ENTRY entry;
	wine_ldt_get_entry(SREG(sreg), &entry);
	if (wine_ldt_get_flags(&entry) & WINE_LDT_FLAGS_32BIT)
	{
		return TRUE;
	}
	return FALSE;
}
UINT get_segment_descriptor_wine(int sreg)
{
	LDT_ENTRY entry;
	wine_ldt_get_entry(SREG(sreg), &entry);
	if (wine_ldt_get_flags(&entry) & WINE_LDT_FLAGS_32BIT)
	{
		//printf("WINE_LDT_FLAGS_32BIT\n");
	}
	void *ptr = wine_ldt_get_ptr(SREG(sreg), 0);
	return (UINT)ptr;
	//m_base[sreg] = SegBase(sreg);
}
void load_segment_descriptor_wine(int sreg)
{
	LDT_ENTRY entry;
	wine_ldt_get_entry(SREG(sreg), &entry);
	if (wine_ldt_get_flags(&entry) & WINE_LDT_FLAGS_32BIT)
	{
		//printf("WINE_LDT_FLAGS_32BIT\n");
	}
	void *ptr = wine_ldt_get_ptr(SREG(sreg), 0);
	SREG_BASE(sreg) = (UINT)ptr;
	//m_base[sreg] = SegBase(sreg);
}
