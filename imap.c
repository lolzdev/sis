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
#include <utils.h>
#include <imap.h>

static char buf[CMD_MAX_SIZE];
static trie_node *trie;

void imap_trie_encode(char *str, uint8_t cmd)
{
    trie_node *node;
    if (trie == NULL) {
        trie = (trie_node *) malloc(sizeof(trie_node));
        memset(trie, 0x0, sizeof(trie_node));
    }

    node = trie;
    do {
        node->children[*(str) - 'a'] = (trie_node *) malloc(sizeof(trie_node));
        node = node->children[*(str) - 'a'];
        memset(node, 0x0, sizeof(trie_node));
        node->id = 0xff;
        str++;
    } while (*str != '\0');

    node->id = cmd;
    printf("%d\n", node->id);
}

void imap_populate_trie(void)
{
    imap_trie_encode("capability", 0x0);
    imap_trie_encode("noop", 0x1);
    imap_trie_encode("logout", 0x2);
    imap_trie_encode("starttls", 0x3);
    imap_trie_encode("authenticate", 0x4);
    imap_trie_encode("login", 0x5);
}

#define CMD_MAP_LAST 0x5

void imap_trie_free(trie_node *node)
{
    trie_node *tmp = NULL;
    
    for (int i=0; i < 26; i++) {
        tmp = node->children[i];
        if (tmp != NULL) {
            imap_trie_free(tmp);
        }
    }

    free(node);
}

uint8_t imap_init(uint8_t daemon, imap_t *instance)
{
    imap_populate_trie();

    imap_t imap;
    imap.ssl_ctx = NULL;
    imap.ssl = 0;

    /* Create a new socket using IPv4 protocol */
    if ((imap.socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socker");
        return 1;
    }

    bzero(&imap.addr, sizeof(struct sockaddr_in));

    imap.state = IMAP_STATE_NO_AUTH;
    imap.addr.sin_family = AF_INET;
    /* From config.h */
    imap.addr.sin_port = htons(TLS_ENABLED ? IMAPS_PORT : IMAP_PORT);
    
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

    if (TLS_ENABLED) {
        imap_create_ssl_ctx(&imap);
        imap_starttls(&imap, NULL);
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

client_list *imap_add_client(imap_t *instance, client_list *list, int sock)
{
    client_list *node = (client_list *) malloc(sizeof(client_list));
    /* Check if TLS is active */
    if (instance->ssl) {
        node->ssl = SSL_new(instance->ssl_ctx);
        SSL_set_fd(node->ssl, sock);
        SSL_set_accept_state(node->ssl);
        SSL_do_handshake(node->ssl);
        node->fd = SSL_get_fd(node->ssl);
    } else {
        node->fd = sock;
    }

    node->socket = sock;
    node->next = list;
    node->prev = NULL;
    if (list != NULL) {
        list->prev = node;
    }

    return node; 
}

client_list *imap_remove_client(imap_t *instance, client_list *list, client_list *node)
{
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    }

    /* Check if TLS is active */
    if (instance->ssl) {
        SSL_shutdown(node->ssl);
        SSL_free(node->ssl);
    }

    close(node->socket);

    if (node == list) {
        return NULL;
    }

    return list;
}

client_list *imap_remove_sock(imap_t *instance, client_list *list, int sock)
{
    client_list *node = list;

    while (node != NULL && node->socket != sock) {
        node = node->next;
    }

    if (node != NULL) {
        imap_remove_client(instance, list, node);
    }

    return node;
}

void imap_starttls(imap_t *imap, client_list *list)
{
    imap->ssl = 1;

    while (list != NULL) {
        list->ssl = SSL_new(imap->ssl_ctx);
        list->fd = SSL_get_fd(list->ssl);
        SSL_set_fd(list->ssl, list->socket);
        list = list->next;
    }
}

void imap_start(imap_t *instance)
{
    int activity, max_fd, connection;
    size_t bytes_read;
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

            instance->clients = imap_add_client(instance, instance->clients, connection);
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
                 if ((bytes_read = imap_read(node, buf, CMD_MAX_SIZE, instance->ssl)) < 0) {
                    perror("recv");
                    instance->clients = imap_remove_client(instance, instance->clients, node);
                    free(node);
                    FD_CLR(connection, &fds);
                    syslog(LOG_ERR, "Failed to receive data.");
                /* Somebody disconnected */
                }  else if (bytes_read == 0) {
                    instance->clients = imap_remove_client(instance, instance->clients, node);
                    free(node);
                    FD_CLR(connection, &fds);
                    syslog(LOG_INFO, "Connection closed.");
                } else {
                    buf[bytes_read] = '\0';
                    imap_cmd cmd = imap_parse_cmd(buf);
                    uint8_t res = imap_cmd_exec(cmd, node, instance->ssl, instance->state);
                    if (res == IMAP_LOGOUT) {
                        instance->clients = imap_remove_client(instance, instance->clients, node);
                        free(node);
                        FD_CLR(connection, &fds);
                        syslog(LOG_INFO, "Client logout.");   
                    } else if (res == IMAP_STARTTLS) {
                        imap_starttls(instance, instance->clients);
                    }
                    if (cmd.params != NULL) {
                        free(cmd.params);
                    }
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
        if (instance->ssl) {
            SSL_shutdown(node->ssl);
            SSL_free(node->ssl);
        }
        close(node->socket);
        tmp = node->next;
        free(node);
        node = tmp;
    }

    imap_trie_free(trie);
    close(instance->socket);
    if (instance->ssl) {
        SSL_CTX_free(instance->ssl_ctx);
    }
    free(instance);
}

uint8_t imap_match_cmd(char *cmd, size_t len)
{
    strnlower(cmd, len);

    trie_node *node = trie;
    do {
        node = node->children[*cmd - 'a'];
        if (node->id == 0xff) {
            cmd++;
            continue;
        } else {
            return node->id;
        }

        cmd++;
    } while (*cmd != '\0');

    return 0xff;
}

imap_cmd imap_parse_cmd(char *s)
{
    imap_cmd cmd;
    strstrip(s);
    size_t params = 0, id_len = 0, i = 0;
    char *cpy;
    printf("%s\n", s);

    cmd.params = NULL;    
    /* Copy the first 4 characters of the command in the tag field */ 
    memcpy(cmd.tag, s, 4);
    s += 5; 

    while (*s != '\n' && *s != '\0' && *s != ' ' && *s > 0) {
        s++;
        id_len++;
    }

    if (id_len == 0) {
        cmd.id = 0xff;
        return cmd;
    }

    id_len -= 1;
    s -= id_len+1;
    cmd.id = imap_match_cmd(s, id_len);
    s += id_len+2;

    char *tok;
    cpy = (char *) calloc(strlen(s), sizeof(char));
    strcpy(cpy, s);
    for (tok = strtok(cpy, " "); tok; tok = strtok(NULL, " ")) {
        params++;
    }

    free(cpy);

    if (params > 0) {
        cmd.params = (char **) calloc(params, sizeof(char **));
        for (tok = strtok(s, " "); tok; tok = strtok(NULL, " ")) {
            cmd.params[i] = tok;
            i++;
        }
        cmd.p_count = params;
    }
    
    return cmd;
}

#include <imap.routines>

uint8_t imap_cmd_exec(imap_cmd cmd, client_list *node, uint8_t ssl, uint8_t state)
{
    if (cmd.id < 0x0 || cmd.id > CMD_MAP_LAST) {
        return IMAP_FAIL;
    }
    void *routines[] = {
        &&capability,
        &&noop,
        &&logout,
        &&starttls,
        &&auth,
        &&login
    };
    goto *routines[cmd.id];
    IMAP_ROUTINE(capability)
    IMAP_ROUTINE(noop)
    IMAP_ROUTINE(logout)
    IMAP_ROUTINE(starttls)
    IMAP_ROUTINE(auth)
    IMAP_ROUTINE(login)

    return IMAP_SUCCESS;
}

void imap_create_ssl_ctx(imap_t *imap)
{
    const SSL_METHOD *method;

    method = TLS_server_method();
    imap->ssl_ctx = SSL_CTX_new(method);
    if (!imap->ssl_ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(imap->ssl_ctx, "ca-cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(imap->ssl_ctx, "ca-key.pem", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

int imap_read(client_list *node, char *buffer, size_t len, uint8_t ssl)
{
    if (ssl) {
        return SSL_read(node->ssl, buffer, len);
    } else {
        return read(node->socket, buffer, len);
    }
}

void imap_write(client_list *node, uint8_t ssl, char *fmt, ...)
{
    va_list(args);
    va_start(args, fmt);
    vsprintf(buf, fmt, args);

    if (!ssl) {
        write(node->socket, buf, strlen(buf));
    } else {
        SSL_write(node->ssl, buf, strlen(buf));
    }
}

void imap_flush(client_list *node, uint8_t ssl)
{
    FILE *fd;

    if (!ssl) {
        fd = fdopen(node->socket, "w");
    } else {
        fd = fdopen(SSL_get_wfd(node->ssl), "w");    
    }

    fflush(fd);
}
