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
#include <stddef.h>
#include <signal.h>
#include <syslog.h>
#include <imap.h>

static imap_t *instance;

void int_handler(int sig)
{
    if (instance != NULL) {
        imap_close(instance);
    }

    exit(0);
}

int main(void)
{
    signal(SIGINT, int_handler);
    signal(SIGPIPE, SIG_IGN);

    openlog("sis", LOG_PID, LOG_MAIL);
    syslog(LOG_INFO, "Starting sis %s.", VERSION);

    int status = 0;
    instance = (imap_t *) malloc(sizeof(imap_t));    

    if ((status = imap_init(0, instance)) != 0) {
        perror("imap_init");
        goto ret;
    }

    imap_start(instance);

ret:
    if (instance != NULL) {
        imap_close(instance);
    }

    closelog();
    return status;
}
