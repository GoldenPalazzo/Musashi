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
