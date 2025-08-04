#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "simulator.h"
#include "m68k.h"

#define RAM_SIZE 0x1f400

/* Read/write macros */
#define READ_BYTE(BASE, ADDR) (BASE)[ADDR]
#define READ_WORD(BASE, ADDR) (((BASE)[ADDR]<<8) |			\
							  (BASE)[(ADDR)+1])
#define READ_LONG(BASE, ADDR) (((BASE)[ADDR]<<24) |			\
							  ((BASE)[(ADDR)+1]<<16) |		\
							  ((BASE)[(ADDR)+2]<<8) |		\
							  (BASE)[(ADDR)+3])

#define WRITE_BYTE(BASE, ADDR, VAL) (BASE)[ADDR] = (VAL)&0xff
#define WRITE_WORD(BASE, ADDR, VAL) (BASE)[ADDR] = ((VAL)>>8) & 0xff;		\
									(BASE)[(ADDR)+1] = (VAL)&0xff
#define WRITE_LONG(BASE, ADDR, VAL) (BASE)[ADDR] = ((VAL)>>24) & 0xff;		\
									(BASE)[(ADDR)+1] = ((VAL)>>16)&0xff;	\
									(BASE)[(ADDR)+2] = ((VAL)>>8)&0xff;		\
									(BASE)[(ADDR)+3] = (VAL)&0xff


/* Prototypes */
void exit_error(char* fmt, ...);

unsigned int cpu_read_byte(unsigned int address);
unsigned int cpu_read_word(unsigned int address);
unsigned int cpu_read_long(unsigned int address);
void cpu_write_byte(unsigned int address, unsigned int value);
void cpu_write_word(unsigned int address, unsigned int value);
void cpu_write_long(unsigned int address, unsigned int value);
void cpu_pulse_reset(void);
int cpu_irq_ack(int level);

void int_controller_set(unsigned int value);
void int_controller_clear(unsigned int value);

/* Data */
unsigned char g_ram[RAM_SIZE]; /* RAM */
unsigned int g_int_controller_pending = 0; /* List of pending interrupts */
unsigned int g_int_controller_highest_int = 0; /* Highest pending */

/* Exit with an error message.  Use printf syntax. */
void exit_error(char* fmt, ...)
{
	static int guard_val = 0;
	char buff[100];
	unsigned int pc;
	va_list args;

	if(guard_val)
		return;
	else
		guard_val = 1;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	pc = m68k_get_reg(NULL, M68K_REG_PPC);
	m68k_disassemble(buff, pc, M68K_CPU_TYPE_68000);
	fprintf(stderr, "At %04x: %s\n", pc, buff);

	exit(EXIT_FAILURE);
}

/* All memory reads */
unsigned int cpu_read_byte(unsigned int address)
{
    if(address > RAM_SIZE)
        exit_error("Attempted to read byte from RAM address %08x", address);
    return READ_BYTE(g_ram, address);
}

unsigned int cpu_read_word(unsigned int address)
{
    if(address > RAM_SIZE)
        exit_error("Attempted to read word from RAM address %08x", address);
    return READ_WORD(g_ram, address);
}

unsigned int cpu_read_long(unsigned int address)
{
    if(address > RAM_SIZE)
        exit_error("Attempted to read long from RAM address %08x", address);
    return READ_LONG(g_ram, address);
}

/* All memory writes */

void cpu_write_byte(unsigned int address, unsigned int value)
{
    if(address > RAM_SIZE)
        exit_error("Attempted to write %02x to RAM address %08x", value&0xff, address);
    WRITE_BYTE(g_ram, address, value);
}

void cpu_write_word(unsigned int address, unsigned int value)
{
    if(address > RAM_SIZE)
        exit_error("Attempted to write %04x to RAM address %08x", value&0xffff, address);
    WRITE_WORD(g_ram, address, value);
}

void cpu_write_long(unsigned int address, unsigned int value)
{
    if(address > RAM_SIZE)
        exit_error("Attempted to write %08x to RAM address %08x", value, address);
    WRITE_LONG(g_ram, address, value);
}

/* CPU pulses the RESET line */
void cpu_pulse_reset(void)
{
    // Should reset all external devices
}

/* CPU acknowledges an IRQ */
int cpu_irq_ack(int level)
{
    if(level < 0 || level > 7)
        return M68K_INT_ACK_SPURIOUS;

    // Acknowledge the interrupt and return the vector number
    // For simplicity, we just return the level as the vector
    g_int_controller_pending &= ~(1 << level);
    g_int_controller_highest_int = 0; // Reset highest pending interrupt
    return M68K_INT_ACK_AUTOVECTOR;
}

/* Interrupt controller implementation */
void int_controller_set(unsigned int value)
{
    if(value > 7)
        exit_error("Attempted to set invalid interrupt level %d", value);

    g_int_controller_pending |= (1 << value);

    // Update the highest pending interrupt
    if(value > g_int_controller_highest_int)
    {
        g_int_controller_highest_int = value;
        m68k_set_irq(g_int_controller_highest_int);
    }
}

void int_controller_clear(unsigned int value)
{
    if(value > 7)
        exit_error("Attempted to clear invalid interrupt level %d", value);

    g_int_controller_pending &= ~(1 << value);

    // Recalculate the highest pending interrupt
    for(g_int_controller_highest_int = 7;
            g_int_controller_highest_int > 0;
            g_int_controller_highest_int--)
        if(g_int_controller_pending & (1 << g_int_controller_highest_int))
            break;

    m68k_set_irq(g_int_controller_highest_int);
}


unsigned int get_instruction_info(unsigned int pc, char* buff)
{
    if (buff == NULL)
    {
        fprintf(stderr, "Buffer for instruction info is NULL\n");
        return 0;
    }
    if (pc >= RAM_SIZE)
    {
        fprintf(stderr, "PC out of bounds: %04x\n", pc);
        return 0;
    }

    // Get instruction info at the specified PC
    return m68k_disassemble(buff, pc, M68K_CPU_TYPE_68000);
}


int main()
{
    return 0;
}
