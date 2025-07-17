#include <stddef.h> // for size_t
#ifndef SIM__HEADER
#define SIM__HEADER

#define RAM_SIZE 0x40000 // 256K RAM

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
#endif /* SIM__HEADER */
