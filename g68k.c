#include <stdio.h>

#include "simulator.h"
#include "server.h"

int main(int argc, char *argv[])
{
    server_setup();
    while (1)
    {
        handle_fds();
        srv_msg_t msg = get_client_msg(NULL);
        if (msg == G68K_NOMSG)
            ;
        else
        {
            printf("Received message: %d\n", msg);
            send_msg("Message received");
        }
    }
    return 0;
}
