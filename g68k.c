#include <stdio.h>
#include <string.h>

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
    int g_quit = 0;
    char args[GBUF_SIZE] = {0};
    char msg[GBUF_SIZE] = {0};
    while (!g_quit)
    {
        handle_fds();
        srv_msg_t cmd = get_client_msg(args);
        if (cmd == G68K_NOMSG)
            ;
        else
        {
            printf("Received message: %d\n", cmd);
            strcpy(msg, "ok");
            switch (cmd)
            {
                case G68K_QUIT:
                    printf("Exiting...\n");
                    g_quit = 1;
                    break;
                case G68K_NMI:
                    break;
                case G68K_STEP:
                    g68k_execute_cycles(100000);
                    break;
                case G68K_RREG:
                    // Handle read register request
                    {
                        char reg_name[10] = {0};
                        sscanf(args, "%9s", reg_name);
                        m68k_register_t reg = m68k_register_from_string(reg_name);
                        if (reg != 1337) // 1337 is the unknown register value
                        {
                            unsigned int value = m68k_get_reg(NULL, reg);
                            snprintf(msg, sizeof(msg), "%08x", value);
                        }
                        else
                        {
                            snprintf(msg, sizeof(msg), "wrongreg");
                        }
                    }
                    break;
                case G68K_WREG:
                    // Handle write register request
                    break;
                case G68K_STATE:
                    // Handle state request
                    // This is a placeholder, actual implementation needed
                    printf("State request received\n");
                    break;
                default:
                    printf("Unknown message type: %d\n", cmd);
            }
            send_msg(msg);
        }
    }
    return 0;
}
