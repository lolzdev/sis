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

#ifndef IMAP_H
#define IMAP_H

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BACKLOG 4
#define IMAP_SUCCESS 0x0
#define IMAP_FAIL 0x1
#define IMAP_LOGOUT 0x2
#define IMAP_STARTTLS 0x3

#define IMAP_STATE_NO_AUTH 0x0
#define IMAP_STATE_AUTH 0x1
#define IMAP_STATE_SELECTED 0x2

typedef struct _client_list {
    int32_t socket, fd;
    SSL *ssl;
    struct _client_list *next;
    struct _client_list *prev;
} client_list;

typedef struct _trie_node {
    struct _trie_node *children[26];
    uint8_t id;
} trie_node;

typedef struct imap {
    int32_t socket;
    client_list *clients;
    struct sockaddr_in addr;
    uint8_t state, ssl;
    SSL_CTX *ssl_ctx;
} imap_t;

typedef struct {
    char tag[4];
    uint8_t id;
    size_t p_count;
    char **params;
} imap_cmd;

/* Create a new imap_t instance and initialize the server. */
uint8_t imap_init(uint8_t daemon, imap_t *instance);
/* Start the IMAP server. */
void imap_start(imap_t *instance);
/* Close all connections and free the allocated memory. */
void imap_close(imap_t *instance);
/* Add client to client list */
client_list *imap_add_client(imap_t *instance, client_list *list, int sock);
/* Close connection with client */
client_list *imap_remove_client(imap_t *instance, client_list *list, client_list *node);
client_list *imap_remove_sock(imap_t *instance, client_list *list, int sock);
/* Get the higher file descriptor in the client list */
int imap_get_max_fd(client_list *list, int master);
imap_cmd imap_parse_cmd(char *s);
uint8_t imap_match_cmd(char *cmd, size_t len);
void imap_create_ssl_ctx(imap_t *imap);
void imap_starttls(imap_t *imap, client_list *list);
int imap_read(client_list *node, char *buf, size_t len, uint8_t ssl);
void imap_write(client_list *node, uint8_t ssl, char *fmt, ...);
void imap_flush(client_list *node, uint8_t ssl);
uint8_t imap_cmd_exec(imap_cmd cmd, client_list *node, uint8_t ssl, uint8_t state);
void imap_trie_populate(void);
void imap_trie_encode(char *str, uint8_t cmd);

#endif /* ifndef IMAP_H */
