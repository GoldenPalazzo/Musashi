#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "m68k.h"
#include "simulator.h"


// =============================================================
// Macros
// RW operations
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
// Other macros
#define IRQ_NMI 7
#define RAM_SIZE 0x40000 // 256K RAM
// =============================================================
// Global variables
unsigned int g_nmi = 0;
unsigned int g_irq_highest_level = 0;
unsigned int g_irq_pending = 0;
unsigned char g_ram[RAM_SIZE];
// =============================================================
// Prototypes
// Musashi functions
void exit_error(char* fmt, ...);

unsigned int cpu_read_byte(unsigned int address);
unsigned int cpu_read_word(unsigned int address);
unsigned int cpu_read_long(unsigned int address);
void cpu_write_byte(unsigned int address, unsigned int value);
void cpu_write_word(unsigned int address, unsigned int value);
void cpu_write_long(unsigned int address, unsigned int value);
void cpu_pulse_reset(void);
int cpu_irq_ack(int level);

void nmi_device_reset(void);
void nmi_device_update(void);
int nmi_device_ack(void);

// I/O will be added in a future update

/*
void input_device_reset(void);
void input_device_update(void);
int input_device_ack(void);
unsigned int input_device_read(void);
void input_device_write(unsigned int value);

void output_device_reset(void);
void output_device_update(void);
int output_device_ack(void);
unsigned int output_device_read(void);
void output_device_write(unsigned int value);
*/

void int_controller_set(unsigned int value);
void int_controller_clear(unsigned int value);

// Golden functions
void parse_srec(const char* filename, unsigned char* ram, unsigned int ram_size);
void cpu_instr_callback(unsigned int pc);
void cpu_pc_changed(unsigned int pc);
int m68k_register_from_string(const char* reg_name);
// =============================================================
// Definitions

// Hook functions
void cpu_pc_changed(unsigned int pc)
{
#ifdef DEBUG
    // This function can be used to handle PC changes, if needed.
    // For now, we will just print the new PC value.
    printf("PC changed to: %08x\n", pc);
#endif
}

void cpu_instr_callback(unsigned int pc)
{
#ifdef DEBUG
    printf("Executed instruction at: %08x\n", pc);
#endif
}

// Exit with an error message
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

// Read and write functions for the CPU memory
unsigned int cpu_read_byte(unsigned int address)
{
    if(address >= RAM_SIZE)
        exit_error("Read byte from invalid address %04x", address);
    return READ_BYTE(g_ram, address);
}

unsigned int cpu_read_word(unsigned int address)
{
    if(address >= RAM_SIZE || address & 1)
        exit_error("Read word from invalid address %04x", address);
    return READ_WORD(g_ram, address);
}

unsigned int cpu_read_long(unsigned int address)
{
    if(address >= RAM_SIZE || address & 3)
        exit_error("Read long from invalid address %04x", address);
    return READ_LONG(g_ram, address);
}

void cpu_write_byte(unsigned int address, unsigned int value)
{
    if(address >= RAM_SIZE)
        exit_error("Write byte to invalid address %04x", address);
    WRITE_BYTE(g_ram, address, value);
}

void cpu_write_word(unsigned int address, unsigned int value)
{
    if(address >= RAM_SIZE || address & 1)
        exit_error("Write word to invalid address %04x", address);
    WRITE_WORD(g_ram, address, value);
}

void cpu_write_long(unsigned int address, unsigned int value)
{
    if(address >= RAM_SIZE || address & 3)
        exit_error("Write long to invalid address %04x", address);
    WRITE_LONG(g_ram, address, value);
}

// Pulse reset function for the CPU
void cpu_pulse_reset(void)
{
    nmi_device_reset();
}

// Acknowledge IRQs
int cpu_irq_ack(int level)
{
    switch (level) {
        case IRQ_NMI:
            return nmi_device_ack();
        default:
            return M68K_INT_ACK_SPURIOUS;
    }
}

// NMI device functions
void nmi_device_reset(void)
{
    g_nmi = 0;
}

void nmi_device_update(void)
{
    if (g_nmi) {
        int_controller_set(IRQ_NMI);
        g_nmi = 0; // Clear NMI after handling
    }
}

int nmi_device_ack(void)
{
    int_controller_clear(IRQ_NMI);
    return M68K_INT_ACK_AUTOVECTOR;
}

// Interrupt controller functions
void int_controller_set(unsigned int value)
{
    unsigned int old_pending = g_irq_pending;
    g_irq_pending |= (1 << value);
    if (g_irq_pending && !old_pending && value > g_irq_highest_level) {
        g_irq_highest_level = value;
        m68k_set_irq(g_irq_highest_level);
    }
}

void int_controller_clear(unsigned int value)
{
    g_irq_pending &= ~(1 << value);
    for (g_irq_highest_level = 7; g_irq_highest_level > 0; g_irq_highest_level--) {
        if (g_irq_pending & (1 << g_irq_highest_level)) {
            break;
        }
    }
    m68k_set_irq(g_irq_highest_level);
}

// Parse SREC file and load it into RAM
void parse_srec(const char* filename, unsigned char* ram, unsigned int ram_size)
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
        if (line[0] != 'S') continue; // Skip non-S-Record lines
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
                if (address + data_byte_count > ram_size)
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
                    memset(ram + address + i, data, 1); // Write data to RAM
                }
            }
            else if (record_type > 6 && record_type < 10)
            {
                // Set PC (entrypoint)
                if (address >= ram_size)
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

// Convert register name to m68k_register_t enum
int m68k_register_from_string(const char* reg_name)
{
    if (strcmp(reg_name, "D0") == 0) return M68K_REG_D0;
    if (strcmp(reg_name, "D1") == 0) return M68K_REG_D1;
    if (strcmp(reg_name, "D2") == 0) return M68K_REG_D2;
    if (strcmp(reg_name, "D3") == 0) return M68K_REG_D3;
    if (strcmp(reg_name, "D4") == 0) return M68K_REG_D4;
    if (strcmp(reg_name, "D5") == 0) return M68K_REG_D5;
    if (strcmp(reg_name, "D6") == 0) return M68K_REG_D6;
    if (strcmp(reg_name, "D7") == 0) return M68K_REG_D7;
    if (strcmp(reg_name, "A0") == 0) return M68K_REG_A0;
    if (strcmp(reg_name, "A1") == 0) return M68K_REG_A1;
    if (strcmp(reg_name, "A2") == 0) return M68K_REG_A2;
    if (strcmp(reg_name, "A3") == 0) return M68K_REG_A3;
    if (strcmp(reg_name, "A4") == 0) return M68K_REG_A4;
    if (strcmp(reg_name, "A5") == 0) return M68K_REG_A5;
    if (strcmp(reg_name, "A6") == 0) return M68K_REG_A6;
    if (strcmp(reg_name, "A7") == 0) return M68K_REG_A7;
    if (strcmp(reg_name, "PC") == 0) return M68K_REG_PC;
    if (strcmp(reg_name, "USP") == 0) return M68K_REG_USP;
    if (strcmp(reg_name, "ISP") == 0) return M68K_REG_ISP;
    if (strcmp(reg_name, "MSP") == 0) return M68K_REG_MSP;
    if (strcmp(reg_name, "SR") == 0) return M68K_REG_SR;
    return 1337; // Unknown register
}
// =============================================================
