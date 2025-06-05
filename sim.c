#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "sim.h"

#include <netinet/in.h>
#include <fcntl.h>

#include "m68k.h"

#define PORT 2601
#define BUF_SIZE 256


// IRQ
#define IRQ_NMI 7

// 256K memory
#define RAM_SIZE 0x40000

// Read/Write macros
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

void nmi_device_reset(void);
void nmi_device_update(void);
int nmi_device_ack(void);

/*
 * I/O will be added in a future update


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

// void get_user_input(void);

void parse_srec(const char* filename, unsigned char* ram, unsigned int ram_size);
void your_pc_changed_handler_function(unsigned int pc);
void your_instruction_hook_function(unsigned int pc);

void your_pc_changed_handler_function(unsigned int pc)
{
#ifdef DEBUG
    // This function can be used to handle PC changes, if needed.
    // For now, we will just print the new PC value.
    printf("PC changed to: %08x\n", pc);
#endif
}

void your_instruction_hook_function(unsigned int pc)
{
#ifdef DEBUG
    printf("Executed instruction at: %08x\n", pc);
#endif
}

// State
unsigned int g_quit = 0;
unsigned int g_nmi = 0;
unsigned int g_irq_highest_level = 0;
unsigned int g_irq_pending = 0; // list of pending IRQs

unsigned char g_ram[RAM_SIZE] = {0}; // 256K RAM

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

/* Read data from memory */
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

/* Pulse reset the CPU */
void cpu_pulse_reset(void)
{
    nmi_device_reset();
}

/* CPU acknowledging an IRQ */

int cpu_irq_ack(int level)
{
    switch (level) {
        case IRQ_NMI:
            return nmi_device_ack();
        default:
            return M68K_INT_ACK_SPURIOUS;
    }
}

/* NMI device implementation */
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

/* Interrupt controller functions */
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


/* Convert register name to M68K register enum */
m68k_register_t m68k_register_from_string(const char* reg_name)
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

/* Parsing S-Rec binary format */
void parse_srec(const char* filename, unsigned char* ram, unsigned int ram_size)
{
    FILE* file = fopen(filename, "rb");
    if (!file)
    {
        exit_error("Failed to open S-Record file: %s", filename);
    }

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        if (line[0] != 'S') continue; // Skip non-S-Record lines
        short record_type = line[1] - '0'; // Get record type (S0, S1, S2, etc.)
        char byte_count_char[3] = { line[2], line[3], '\0' };
        long byte_count = strtol(byte_count_char, NULL, 16); // Convert byte count from hex to int
        if (byte_count < 3 || byte_count > 255)
        {
            exit_error("Invalid byte count in S-Record: %d", byte_count);
        }
        if (record_type > 0)
        {

            size_t address = 0;

            size_t address_bytes = 2 + (record_type<4 ? record_type-1 : 2-record_type%7);
            // S1, S2, S3 records contain address
            char* address_str = malloc(sizeof(char) * (address_bytes * 2 + 1));
            if (!address_str)
            {
                exit_error("Memory allocation failed for address string");
            }
            strncpy(address_str, line + 4, address_bytes * 2);
            address_str[address_bytes * 2] = '\0'; // Null-terminate the string
            address = (unsigned int)strtol(address_str, NULL, 16); // Convert address from hex to int
            free(address_str);

            if (record_type < 4)
            {

                size_t data_byte_count = byte_count - address_bytes - 1; // Data length
                if (address + data_byte_count > ram_size)
                {
                    exit_error("S-Record data exceeds RAM size at address %04x", address);
                }
                for (size_t i = 0; i < data_byte_count; i++)
                {
                    char* data_byte = malloc(sizeof(unsigned char) * 3);
                    if (!data_byte)
                    {
                        exit_error("Memory allocation failed for data byte");
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
                    exit_error("S-Record PC exceeds RAM size at address %04x", address);
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

void setup_g68k() {
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();
    nmi_device_reset();
}


/* Main */
int main(int argc, char* argv[])
{
    // Initialize the CPU
    setup_g68k();
    // Load initial state or program into RAM (not implemented here)
    if (argc < 2) {
        exit_error("Usage: %s <srec_file> [n_cycles]", argv[0]);
    }
    parse_srec(argv[1], g_ram, RAM_SIZE);
#ifdef DEBUG
    printf("Loaded S-Record file: %s\n", argv[1]);
    printf("Program Counter set to: %08x\n", m68k_get_reg(NULL, M68K_REG_PC));
#endif

    // Setup TCP server
    int server_fd, client_fd, max_fd, activity;
    struct timeval timeout = {0, 100000}; // 100ms timeout
    fd_set read_fds;
    int addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
    address.sin_port = htons(PORT); // Port 12345
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(server_fd, F_SETFL, O_NONBLOCK); // Set non-blocking mode
    if (server_fd < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
    int bind_result = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    if (bind_result < 0) {
        perror("Failed to bind socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("Failed to listen on socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", PORT);

    // Main loop
    g_quit = 0;
    unsigned int n_cycles = 100000; // Default number of cycles
    if (argc == 3)
    {
        n_cycles = strtoul(argv[2], NULL, 10);
        if (n_cycles <= 0) {
            exit_error("Invalid number of cycles: %s", argv[2]);
        }
    }
    while (!g_quit)
    {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;
        if (client_fd > 0) {
            FD_SET(client_fd, &read_fds);
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
        }
        activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (FD_ISSET(server_fd, &read_fds)) {
            // New connection
            client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (client_fd >= 0) {
                printf("New connection accepted\n");
            }
            else if (client_fd < 0) {
                perror("Accept failed");
            }
        }


        char buffer[BUF_SIZE] = {0};
        if (FD_ISSET(client_fd, &read_fds)) {
            // Handle client input
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read < 0) {
                perror("Read failed");
                close(client_fd);
                client_fd = 0;
                continue;
            } else if (bytes_read == 0) {
                // Client disconnected
                printf("Client disconnected\n");
                close(client_fd);
                client_fd = 0;
                continue;
            }
            buffer[bytes_read] = '\0'; // Null-terminate the string
            printf("Received command: %s\n", buffer);
        }
        char msg[BUF_SIZE];
        if (strcasecmp(buffer, "quit") == 0) {
            g_quit = 1;
            snprintf(msg, sizeof(msg), "quit");
            send(client_fd, msg, strlen(msg), 0);
            break;
        } else if (strcasecmp(buffer, "nmi") == 0) {
            g_nmi = 1; // Trigger NMI
            nmi_device_update();
            snprintf(msg, sizeof(msg), "nmi");
        } else if (strcasecmp(buffer, "step") == 0){
#ifdef DEBUG
            printf("Executing %u cycles...\n", n_cycles);
#endif
            m68k_execute(n_cycles);

            // Update devices
            nmi_device_update();
            /*input_device_update();*/
            /*output_device_update();*/

            // Check for user input or other events
            // get_user_input();
            unsigned int cycles_executed = m68k_cycles_run();
            snprintf(msg, sizeof(msg), "%u", cycles_executed);
        } else if (strncasecmp(buffer, "rreg ", 5) == 0) {
            // Read register
            char reg_name[10];
            sscanf(buffer + 5, "%s", reg_name);
            m68k_register_t reg = m68k_register_from_string(reg_name);
            if (reg == 1337) {
                printf("Unknown register: %s\n", reg_name);
                snprintf(msg, sizeof(msg), "wrongreg");
            } else {
                unsigned int reg_value = m68k_get_reg(NULL, reg);
                char msg[BUF_SIZE];
                snprintf(msg, sizeof(msg), "%08x", reg_value);
            }
        } else if (strncasecmp(buffer, "wreg ", 5) == 0) {
            // Write register
            char reg_name[10];
            unsigned int value;
            sscanf(buffer + 5, "%s %x", reg_name, &value);
            m68k_register_t reg = m68k_register_from_string(reg_name);
            if (reg == 1337) {
                snprintf(msg, sizeof(msg), "wrongreg");
            } else {
                m68k_set_reg(reg, value);
                int val = m68k_get_reg(NULL, reg);
                snprintf(msg, sizeof(msg), "%08x", val);
            }
        } else if (strcasecmp(buffer, "state") == 0) {
            unsigned int pc = m68k_get_reg(NULL, M68K_REG_PC);
            unsigned int sp = m68k_get_reg(NULL, M68K_REG_A7);
            unsigned int sr = m68k_get_reg(NULL, M68K_REG_SR);
            char msg[BUF_SIZE];
            snprintf(msg, sizeof(msg), "%08x %08x %04x", pc, sp, sr);
        } else if (strcasecmp(buffer, "reset") == 0) {
            cpu_pulse_reset();
            nmi_device_reset();
            m68k_pulse_reset();
            snprintf(msg, sizeof(msg), "reset");
        }


        else if (strcmp(buffer, "\0") == 0) {
            continue;
        }
        else {
            printf("Unknown command: %s\n", buffer);
            snprintf(msg, sizeof(msg), "unknown");
        }
        send(client_fd, msg, strlen(msg), 0);
    }
    return 0;
}
