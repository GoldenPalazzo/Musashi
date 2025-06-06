#include <stdio.h>

#include "m68k.h"
#include "simulator.h"
#include "server.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <srec_file>\n", argv[0]);
        return 1;
    }
    g68k_setup(argv[1]);
    server_setup();
    while (1)
    {
        handle_fds();
        char args[GBUF_SIZE] = {0};
        srv_msg_t cmd = get_client_msg(args);
        if (cmd == G68K_NOMSG)
            ;
        else
        {
            printf("Received message: %d\n", cmd);
            char msg[GBUF_SIZE] = {0};
            switch (cmd)
            {
                case G68K_QUIT:
                    printf("Exiting...\n");
                    return 0;
                case G68K_NMI:
                    break;
                case G68K_STEP:
                    g68k_execute_cycles(100000);
                    break;
                case G68K_RREG:
                    // Handle read register request
                    // For now, we will just print the PC value
                    {
                        char reg_name[10] = {0};
                        sscanf(args, "%9s", reg_name);
                        m68k_register_t reg = m68k_register_from_string(reg_name);
                        if (reg != 1337) // 1337 is the unknown register value
                        {
                            unsigned int value = m68k_get_reg(NULL, reg);
                            snprintf(msg, sizeof(msg), "%08x", value);
                            printf("%s\n", msg);
                        }
                        else
                        {
                            snprintf(msg, sizeof(msg), "wrongreg");
                            printf("%s\n", msg);
                        }
                    }
                    break;
                case G68K_WREG:
                    // Handle write register request
                    // This is a placeholder, actual implementation needed
                    printf("Write register request received\n");
                    break;
                case G68K_STATE:
                    // Handle state request
                    // This is a placeholder, actual implementation needed
                    printf("State request received\n");
                    break;
                default:
                    printf("Unknown message type: %d\n", msg);
            }
            send_msg("Message received");
        }
    }
    return 0;
}
