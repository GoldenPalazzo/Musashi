#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#include "server.h"

#define PORT 2601
#define BUFSIZE 128

int g_server_fd, g_client_fd, g_max_fd;
struct sockaddr_in g_server_addr, g_client_addr;
struct timeval g_timeout = {0, 100000}; // 100 ms
fd_set g_read_fds;
int g_addrlen = sizeof(struct sockaddr_in);

void server_setup()
{
    g_server_addr.sin_family = AF_INET;
    g_server_addr.sin_addr.s_addr = INADDR_ANY;
    g_server_addr.sin_port = htons(PORT);
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(g_server_fd, F_SETFL, O_NONBLOCK);
    if (g_server_fd < 0)
    {
        perror("socket");
        exit(1);
    }
    int bind_res = bind(g_server_fd, (struct sockaddr *)&g_server_addr, sizeof(g_server_addr));
    if (bind_res < 0)
    {
        perror("bind");
        exit(1);
    }
    int listen_res = listen(g_server_fd, 5);
    if (listen_res < 0)
    {
        perror("listen");
        exit(1);
    }
    printf("Server setup complete, listening on port %d\n", PORT);
}

void handle_fds()
{
    FD_ZERO(&g_read_fds);
    FD_SET(g_server_fd, &g_read_fds);
    g_max_fd = g_server_fd;
    if (g_client_fd > 0)
    {
        FD_SET(g_client_fd, &g_read_fds);
        if (g_client_fd > g_max_fd)
            g_max_fd = g_client_fd;
    }
    int activity = select(g_max_fd + 1, &g_read_fds, NULL, NULL, &g_timeout);
    if (activity < 0)
    {
        perror("select");
        exit(1);
    }
    if (FD_ISSET(g_server_fd, &g_read_fds))
    {
        g_client_fd = accept(g_server_fd, (struct sockaddr *)&g_client_addr, (socklen_t *)&g_addrlen);
        if (g_client_fd < 0)
        {
            perror("accept");
            /*exit(1);*/
        }
        else
        {
            printf("New client connected\n");
        }
        /*fcntl(g_client_fd, F_SETFL, O_NONBLOCK);*/
    }
}

srv_msg_t get_client_msg(const char* args)
{
    char buffer[BUFSIZE] = {0};
    if (FD_ISSET(g_client_fd, &g_read_fds))
    {
        int bytes_read = recv(g_client_fd, buffer, BUFSIZE - 1, 0);
        if (bytes_read < 0)
        {
            perror("recv");
            close(g_client_fd);
            g_client_fd = -1;
        }
        else if (bytes_read == 0)
        {
            printf("Client disconnected\n");
            close(g_client_fd);
            g_client_fd = -1;
        }
        else
        {
            if (strcasecmp(buffer, "quit") == 0)
            {
                return QUIT;
            }
            else if (strcasecmp(buffer, "nmi") == 0)
            {
                return NMI;
            }
            else if (strcasecmp(buffer, "step") == 0)
            {
                return STEP;
            }
            else if (strncasecmp(buffer, "rreg", 4) == 0)
            {
                return RREG;
            }
            else if (strncasecmp(buffer, "wreg", 4) == 0)
            {
                return WREG;
            }
            else if (strcasecmp(buffer, "state") == 0)
            {
                return STATE;
            }
            else
            {
                return UNKNOWN;
            }
        }
    }
    return NOMSG;
}

void send_msg(const char* msg)
{
    if (g_client_fd > 0)
    {

        int r = send(g_client_fd, msg, strlen(msg), 0);
        if (r < 0)
        {
            perror("send");
            close(g_client_fd);
            g_client_fd = -1;
        }
        else
        {
            printf("Sent message: %s\n", msg);
        }
    }
    else
    {
        fprintf(stderr, "No client connected to send message\n");
    }
}
