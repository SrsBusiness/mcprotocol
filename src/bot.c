#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <jansson.h>
#include "bot.h"
#include "protocol.h"
#include "marshal.h"

#define pos(x, y, z) (((x & 0x3FFFFFF) << 38) | ((y & 0xFFF) << 26) | (z & 0x3FFFFFF))

void hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

// initializes a bot structure with a socket. The socket is bound to the local address on
// some port and is connected to the server specified by the server_host and server_port
// the socket descriptor is returned by the function. If -1 is returned, then an error
// occured, and a message will have been printed out.

int join_server(bot_t *your_bot, char *local_port, char* server_host,
                char* server_port){
    int status;
    struct addrinfo hints, *res;
    int sockfd;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if(status = getaddrinfo(NULL, local_port, &hints, &res)){
        fprintf(stderr, "Your computer is literally haunted: %s\n",
                gai_strerror(status));
        return -1;
    }
    if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1){
        fprintf(stderr, "Could not create socket for unknown reason.\n");
        return -1;
    }
    freeaddrinfo(res);
    // socket bound to local address/port

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if(status = getaddrinfo(server_host, server_port, &hints, &res)){
        fprintf(stderr, "Server could not be resolved: %s\n",
                gai_strerror(status));
        return -1;
    }
    connect(sockfd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    // connected to server
    your_bot -> socketfd = sockfd;
    return sockfd;
}

int disconnect(bot_t *your_bot){
    return close(your_bot -> socketfd);
}

int send_str(bot_t *your_bot, char *str){
    //TODO: send is not guaranteed to send all the data. Need to make loop
    size_t len = strlen(str) + 1; // to include null character
    return send(your_bot -> socketfd, str, len, 0);
}

int send_raw(bot_t *your_bot, void *data, size_t len){
    // hexDump("sender", data, len);
    return send(your_bot -> socketfd, data, len, 0);
}

int receive_raw(bot_t *your_bot, void *data, size_t len){
    return recv(your_bot -> socketfd, data, len, 0);
}

int main() {
    char* buf = calloc(sizeof(char), DEFAULT_THRESHOLD);
    vint32_t packet_size;
    uint32_t received;
    uint32_t len;
    uint32_t tick = 0;
    uint32_t count = 0;
    int ret, i;

    bot_t bot;
    bot.state = HANDSHAKING;
    bot.packet_threshold = DEFAULT_THRESHOLD;

    join_server(&bot, "25567", "lf.lc", "25565");

    send_handshaking_serverbound_handshake(&bot, 47, "localhost", 25565, 2);
    send_login_serverbound_login(&bot, "an_guy");

    bot.packet_threshold = 256;

    struct timeval tv = {2, 0};
    fd_set readfds;

    FD_ZERO(&readfds);
    FD_SET(bot.socketfd, &readfds);

    while (1) {
        usleep(30000);
        
        select(bot.socketfd + 1, &readfds, NULL, NULL, &tv);

        memset(buf, 0, DEFAULT_THRESHOLD);
        for (i = 0; i < 5; i++) {
            ret = receive_raw(&bot, buf + i, 1);
            if (ret <= 0) return 1;
            if ((buf[i] & 0x80) == 0) break;
        }

        len = varint32(buf, &packet_size);
        if (packet_size == 0) continue;

        packet_size += len;
        received = i + 1;
        if (packet_size <= DEFAULT_THRESHOLD) {
            while (received < packet_size) {
                ret = receive_raw(&bot, buf + received, packet_size - received);
                if (ret <= 0) return 1;
                received += ret;
            }

            ret = peek_packet(&bot, buf);
            // printf("%02x\n", ret);
            // hexDump("buffer", buf, packet_size);
            if (bot.state == PLAY) {
                switch (ret) {
                    case 0x00:{
                        play_clientbound_keepalive_t *p;
                        p = recv_play_clientbound_keepalive(&bot, buf);
                        send_play_serverbound_keepalive(&bot, p->keepalive_id);
                        free_packet(p);
                        break;
                    }
                    case 0x02:{
                        play_clientbound_chat_t *p;
                        p = recv_play_clientbound_chat(&bot, buf);

                        json_t *root;
                        json_error_t error;
                        root = json_loads(p->json, 0, &error);
                        free_packet(p);

                        if(!root) {
                            fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
                            break;
                        }
                        json_t *translate = json_object_get(root, "translate");
                        if(!json_is_string(translate)) {
                            fprintf(stderr, "error: translate is not a string\n");
                            json_decref(root);
                            break;
                        }
                        const char* translate_text = json_string_value(translate);
                        if (strcmp(translate_text, "chat.type.text") == 0) {
                            json_t *with_array = json_object_get(root, "with");
                            if (!json_is_array(with_array)) {
                                fprintf(stderr, "error: with is not an array\n");
                                json_decref(root);
                                break;
                            }

                            for(i = 0; i < json_array_size(with_array); i++) {
                                json_t *data;
                                const char *msg;

                                data = json_array_get(with_array, i);
                                if (json_is_string(data)) {
                                    msg = json_string_value(data);
                                    printf("%s\n", msg);
                                }
                            }
                        }

                        json_decref(root);
                        break;
                    }
                    case 0x3f:{
                        send_play_serverbound_plugin_message(&bot, "MC|Brand", "vanilla");
                        break;
                    }
                    default:{
                        break;
                    }
                }
            }
        } else {
            // hexDump("bigbuf", buf, received);
            while (received < packet_size) {
                len = packet_size - received;
                if (len > DEFAULT_THRESHOLD) len = DEFAULT_THRESHOLD;
                ret = receive_raw(&bot, buf, len);
                if (ret <= 0) return 1;
                received += ret;
            }
            // printf("%d vs %d\n", received, packet_size);
        }

        if (bot.state == PLAY) {
            if ((tick % 50) == 1) {
                send_play_serverbound_chat(&bot, "hello world!");
            }

        }
        tick++;
    }

    free(buf);
}
