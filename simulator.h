#ifndef SIM__HEADER
#define SIM__HEADER
#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif
#include <stddef.h> // for size_t

#define RAM_SIZE 0x40000 // 256K RAM
#define SCREEN_ADDR RAM_SIZE
#define SCREEN_SIZE 0xE100 // 320x180 pixels, 1 byte per pixel
#define SCREEN_PALETTE_ADDR SCREEN_ADDR + SCREEN_SIZE
#define SCREEN_PALETTE_SIZE 0x300 // 256 colors, 3 bytes per color (RGB)
#define SCREEN_STATUS_ADDR SCREEN_PALETTE_ADDR + SCREEN_PALETTE_SIZE // 1 byte for screen status
#define SCREEN_SIZE_TOTAL (SCREEN_SIZE + SCREEN_PALETTE_SIZE + 1) // total size of screen memory

unsigned int cpu_read_byte(unsigned int address);
unsigned int cpu_read_word(unsigned int address);
unsigned int cpu_read_long(unsigned int address);
void cpu_write_byte(unsigned int address, unsigned int value);
void cpu_write_word(unsigned int address, unsigned int value);
void cpu_write_long(unsigned int address, unsigned int value);
void cpu_pulse_reset(void);
void cpu_set_fc(unsigned int fc);
int  cpu_irq_ack(int level);
void cpu_instr_callback(unsigned int pc);
void cpu_pc_changed(unsigned int pc);

void parse_srec(const char *filename);
int m68k_register_from_string(const char *reg_name);

void g68k_setup(void);
void g68k_copy_to_ram(size_t dest, const unsigned char* src, size_t size);
void g68k_copy_from_ram(unsigned char* dest, const size_t src, size_t size);
void g68k_reset(void);
void g68k_execute_cycles(unsigned int cycles);
void g68k_clean_ram(void);


// symbols that need to be defined in javascript code
#ifdef WASM
extern void (*screen_write)(size_t address, unsigned int value);
#endif
#endif /* SIM__HEADER */
