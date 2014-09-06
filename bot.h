#include <stdint.h>

typedef struct bot {
    int socketfd;
    size_t packet_threshold;
} bot_t;

extern struct bot context;

int join_server(struct bot *, char *, char *, char *);
int send_str(struct bot *, char *);
int send_raw(struct bot *, void *, int);

