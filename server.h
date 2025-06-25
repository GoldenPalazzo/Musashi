#ifndef SRV__HEADER
#define SRV__HEADER

#define GBUF_SIZE 1024

typedef enum
{
    G68K_NOMSG = 0,
    G68K_UNKNOWN,
    G68K_QUIT,
    G68K_NMI,
    G68K_STEP,
    G68K_RREG,
    G68K_WREG,
    G68K_RMEM,
    G68K_WMEM,
    G68K_RESET,
    G68K_SETBP,
    G68K_RMBP,
    G68K_STATE,
} srv_msg_t;

void server_setup(void);
void handle_fds(void);
srv_msg_t get_client_msg(char* args);
void send_msg(const char* msg);

#endif
