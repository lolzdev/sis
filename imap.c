/*-
 * Copyright (c) 2024, Lorenzo Torres
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the <organization> nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <syslog.h>
#include <errno.h>
#include <config.h>
#include <imap.h>

uint8_t imap_init(uint8_t daemon, imap_t *instance)
{
    imap_t imap;

    /* Create a new socket using IPv4 protocol */
    if ((imap.socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socker");
        return 1;
    }

    bzero(&imap.addr, sizeof(struct sockaddr_in));

    imap.addr.sin_family = AF_INET;
    /* From config.h */
    imap.addr.sin_port = htons(IMAP_PORT);
    
    if (INADDR_ANY) {
        imap.addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    /* Bind the socket to the specified address */
    if ((bind(imap.socket, (struct sockaddr *)&imap.addr, sizeof(imap.addr)) < 0)) {
        perror("bind");
        return 2;
    }

    /* If daemon mode is activated, detach */
    if (daemon) {
        switch (fork()) {
            case -1:
                perror("fork");
                return 3;
                break;
            default:
                close(imap.socket);
                free(instance);
                exit(0);
                break;
            case 0:
                break;
        }
    }

    memcpy(instance, &imap, sizeof(imap));

    return 0;
}

int imap_get_max_fd(client_list *list, int master)
{
    int max_fd = 0;
    client_list *node = list;

    while (node != NULL) {
        if (max_fd < node->socket) {
            max_fd = node->socket;
        }

        node = node->next;
    }

    return max_fd > master ? max_fd : master;
}

client_list *imap_add_client(client_list *list, int sock)
{
    client_list *node = (client_list *) malloc(sizeof(client_list));
    node->socket = sock;
    node->next = list;
    node->prev = NULL;
    if (list != NULL) {
        list->prev = node;
    }

    return node; 
}

client_list *imap_remove_client(client_list *list, client_list *node)
{
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    }

    close(node->socket);

    if (node == list) {
        return NULL;
    }

    return list;
}

client_list *imap_remove_sock(client_list *list, int sock)
{
    client_list *node = list;

    while (node != NULL && node->socket != sock) {
        node = node->next;
    }

    if (node != NULL) {
        imap_remove_client(list, node);
    }

    return node;
}

void imap_start(imap_t *instance)
{
    int activity, max_fd, connection;
    size_t bytes_read;
    char buf[CMD_MAX_SIZE];
    /* List of all the file descriptors (sockets) being used. */
    fd_set fds;
    instance->clients = NULL;
    client_list *node = instance->clients;
    client_list *tmp = instance->clients;

    listen(instance->socket, BACKLOG);
    syslog(LOG_INFO, "Listening on %d.", IMAP_PORT);

    for (;;) {
        FD_ZERO(&fds);
        FD_SET(instance->socket, &fds);
        node = instance->clients;

        while (node != NULL) {
            FD_SET(node->socket, &fds);
            node = node->next;
        }

        max_fd = imap_get_max_fd(instance->clients, instance->socket);
        activity = select(max_fd + 1, &fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno!=EINTR)) {
            perror("select");
        }

        /* New connection. */
        if (FD_ISSET(instance->socket, &fds)) {
            if ((connection = accept(instance->socket, NULL, NULL)) < 0) {
                perror("accept");
                syslog(LOG_ERR, "Connection failed.");
                imap_close(instance);
                exit(EXIT_FAILURE);
            }

            instance->clients = imap_add_client(instance->clients, connection);
            syslog(LOG_INFO, "Connection enstablished.");
            FD_SET(connection, &fds);
        }

        node = instance->clients;
        tmp = node;
        while (node != NULL) {
            connection = node->socket;
            tmp = node->next;
            
            if (FD_ISSET(connection, &fds)) {
                /* Error occured. */
                if ((bytes_read = read(connection, buf, CMD_MAX_SIZE)) < 0) {
                    perror("recv");
                    instance->clients = imap_remove_client(instance->clients, node);
                    free(node);
                    FD_CLR(connection, &fds);
                    syslog(LOG_ERR, "Failed to receive data.");
                /* Somebody disconnected */
                }  else if (bytes_read == 0) {
                    instance->clients = imap_remove_client(instance->clients, node);
                    free(node);
                    FD_CLR(connection, &fds);
                    syslog(LOG_INFO, "Connection closed.");
                } else {
                    buf[bytes_read] = '\0';
                }
            }

            node = tmp;
        }
    }
}

void imap_close(imap_t *instance)
{
    client_list *node = instance->clients;
    client_list *tmp = node;
    while (node != NULL) {
        close(node->socket);
        tmp = node->next;
        free(node);
        node = tmp;
    }

    close(instance->socket);
    free(instance);
}
