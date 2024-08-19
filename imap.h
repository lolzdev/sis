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

#define BACKLOG 4

typedef struct _client_list {
    int32_t socket;
    struct _client_list *next;
    struct _client_list *prev;
} client_list;


typedef struct imap {
    int32_t socket;
    client_list *clients;
    struct sockaddr_in addr;
} imap_t;

/* Create a new imap_t instance and initialize the server. */
uint8_t imap_init(uint8_t daemon, imap_t *instance);
/* Start the IMAP server. */
void imap_start(imap_t *instance);
/* Close all connections and free the allocated memory. */
void imap_close(imap_t *instance);
/* Add client to client list */
client_list *imap_add_client(client_list *list, int sock);
/* Close connection with client */
client_list *imap_remove_client(client_list *list, client_list *node);
client_list *imap_remove_sock(client_list *list, int sock);
/* Get the higher file descriptor in the client list */
int imap_get_max_fd(client_list *list, int master);

#endif /* ifndef IMAP_H */
