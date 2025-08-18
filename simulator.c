#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "simulator.h"
#include "m68k.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EM_EXPORT
#endif

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
unsigned char* g_ram; /* RAM */
size_t g_ram_size = 0;
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
    if(address > g_ram_size)
        exit_error("Attempted to read byte from RAM address %08x", address);
    return READ_BYTE(g_ram, address);
}

unsigned int cpu_read_word(unsigned int address)
{
    if(address > g_ram_size)
        exit_error("Attempted to read word from RAM address %08x", address);
    return READ_WORD(g_ram, address);
}

unsigned int cpu_read_long(unsigned int address)
{
    if(address > g_ram_size)
        exit_error("Attempted to read long from RAM address %08x", address);
    return READ_LONG(g_ram, address);
}

/* All memory writes */

void cpu_write_byte(unsigned int address, unsigned int value)
{
    if(address > g_ram_size)
        exit_error("Attempted to write %02x to RAM address %08x", value&0xff, address);
    WRITE_BYTE(g_ram, address, value);
}

void cpu_write_word(unsigned int address, unsigned int value)
{
    if(address > g_ram_size)
        exit_error("Attempted to write %04x to RAM address %08x", value&0xffff, address);
    WRITE_WORD(g_ram, address, value);
}

void cpu_write_long(unsigned int address, unsigned int value)
{
    if(address > g_ram_size)
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
    int_controller_clear(level);
    return M68K_INT_ACK_AUTOVECTOR;
}

/* Interrupt controller implementation */
EM_EXPORT
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

EM_EXPORT
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

EM_EXPORT
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

/* Some exports */

EM_EXPORT
void reset()
{
    // Reset the CPU and RAM
    m68k_pulse_reset();
    g_int_controller_pending = 0; // Clear pending interrupts
    g_int_controller_highest_int = 0; // Reset highest pending interrupt
}

EM_EXPORT
void setup(unsigned char* rambuf, size_t rambuf_size)
{
    if (rambuf == NULL || rambuf_size == 0) {
        fprintf(stderr, "Invalid RAM buffer or size\n");
        return;
    }

    g_ram = rambuf;
    g_ram_size = rambuf_size;

    // Initialize the CPU and RAM
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    reset();
}

EM_EXPORT
unsigned int step()
{
    // Execute one instruction
    return m68k_execute_step();
}

EM_EXPORT
unsigned int execute(unsigned int cycles)
{
    // Execute a number of cycles
    return m68k_execute(cycles);
}

EM_EXPORT
unsigned int get_reg(m68k_register_t reg)
{
    return m68k_get_reg(NULL, reg);
}

EM_EXPORT
void set_reg(m68k_register_t reg, unsigned int value)
{
    m68k_set_reg(reg, value);
}

EM_EXPORT
void cp_to_ram(size_t dest, const unsigned char* src, size_t size)
{
    if (src == NULL || size == 0) {
        fprintf(stderr, "Invalid memory copy parameters\n");
        return;
    }
    if (dest + size > RAM_SIZE)
    {
        fprintf(stderr, "Memory copy out of bounds (dest: %lx, size: %lu))\n", dest, size);
        return;
    }
    memcpy(g_ram + dest, src, size);
}

EM_EXPORT
void cp_from_ram(unsigned char* dest, const size_t src, size_t size)
{
    if (dest == NULL || size == 0)
    {
        fprintf(stderr, "Invalid memory copy parameters\n");
        return;
    }
    if ((src + size > RAM_SIZE) || (src < 0)) {
        fprintf(stderr, "Memory copy out of bounds (src: %lx, size: %lu)\n", src, size);
        return;
    }
    memcpy(dest, g_ram + src, size);
}

#ifndef __EMSCRIPTEN__


void parse_srec(const char* filename)
{
#ifdef DEBUG
    printf("Parsing S-Record file: %s\n", filename);
#endif
    FILE* file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open S-Record file: %s", filename);
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
#ifdef DEBUG
        printf("Processing line: %s", line);
#endif
        if (line[0] != 'S') exit_error("Invalid S-Record line: %s", line);
        short record_type = line[1] - '0'; // Get record type (S0, S1, S2, etc.)
        char byte_count_char[3] = { line[2], line[3], '\0' };
        long byte_count = strtol(byte_count_char, NULL, 16); // Convert byte count from hex to int
        if (byte_count < 3 || byte_count > 255)
        {
            fprintf(stderr, "Invalid byte count in S-Record: %s", line);
            fclose(file);
        }
#ifdef DEBUG
        printf("Record Type: %d, Byte Count: %ld (0x%lx)\n", record_type, byte_count, byte_count);
#endif
        if (record_type > 0)
        {

            size_t address = 0;

            size_t address_bytes = 2 + (record_type<4 ? record_type-1 : 2-record_type%7);
            // S1, S2, S3 records contain address
            char* address_str = malloc(sizeof(char) * (address_bytes * 2 + 1));
            if (!address_str)
            {
                fprintf(stderr, "Memory allocation failed for address string");
                fclose(file);
                exit(EXIT_FAILURE);
            }
            strncpy(address_str, line + 4, address_bytes * 2);
            address_str[address_bytes * 2] = '\0'; // Null-terminate the string
            address = (unsigned int)strtol(address_str, NULL, 16); // Convert address from hex to int
            free(address_str);
#ifdef DEBUG
            printf("Address: %04lx\n", address);
#endif
            if (record_type < 4)
            {

                size_t data_byte_count = byte_count - address_bytes - 1; // Data length
                if (address + data_byte_count > RAM_SIZE)
                {
                    fprintf(stderr, "S-Record data exceeds RAM size at address %04lx", address);
                    fclose(file);
                    exit(EXIT_FAILURE);
                }
                for (size_t i = 0; i < data_byte_count; i++)
                {
                    char* data_byte = malloc(sizeof(unsigned char) * 3);
                    if (!data_byte)
                    {
                        fprintf(stderr, "Memory allocation failed for data byte");
                        fclose(file);
                        exit(EXIT_FAILURE);
                    }
                    strncpy(data_byte, line + 4 + address_bytes * 2 + i * 2, 2);
                    data_byte[2] = '\0'; // Null-terminate the string
                    unsigned char data = (unsigned char)strtol(data_byte, NULL, 16);
                    free(data_byte);
                    cp_to_ram(address + i, &data, sizeof(unsigned char));
                }
            }
            else if (record_type > 6 && record_type < 10)
            {
                // Set PC (entrypoint)
                if (address >= RAM_SIZE)
                {
                    fprintf(stderr, "S-Record PC exceeds RAM size at address %04lx", address);
                    fclose(file);
                    exit(EXIT_FAILURE);
                }
                m68k_set_reg(M68K_REG_PC, address);
            }
        }
        else if (record_type == 0)
        {
            // S0 record, usually contains header information
            // We can ignore it for now
        }

        else
        {
            exit_error("Unsupported S-Record type: %d", record_type);
        }
    }

    fclose(file);
}


void instruction_hook(unsigned int pc)
{
    /*return;*/
    char buff[100];
    m68k_disassemble(buff, pc, M68K_CPU_TYPE_68000);
    // Print instruction and hex code
    printf("%04x: %s (%02x %02x %02x %02x)\n",
           pc, buff,
           cpu_read_byte(pc), cpu_read_byte(pc + 1),
           cpu_read_byte(pc + 2), cpu_read_byte(pc + 3));
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <srec_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned char ram[RAM_SIZE];
    memset(ram, 0, RAM_SIZE);
    setup(ram, RAM_SIZE);

    parse_srec(argv[1]);

    m68k_pulse_reset();

    printf("starting sr: %016b\n", m68k_get_reg(NULL, M68K_REG_SR));
    printf("starting pc: %04x\n", m68k_get_reg(NULL, M68K_REG_PC));
    while (1)
    {
        execute(1000); // Execute 1000 cycles
        /*int_controller_set(1);*/
    }

    return EXIT_SUCCESS;
}

#endif
