/* See LICENSE file for copyright and license details. */

/* network */
#define IMAP_PORT       143
#define IMAPS_PORT      993
#define TLS_ENABLED     1
/*-
 * Maximum number of connected clients,
 * NOTE: each one of these is a currently
 * connected client! Unless your server
 * has to manage a lot of accounts you
 * should be good with the default value.
 */
#define MAX_CLIENTS     30
/*-
 * Maximum size for the command buffer.
 * IMAP sends plain text commands, so
 * the default value can receive up to
 * 8000 characters long commands.
 * It should be enough but feel free to
 * modify this.
 */
#define CMD_MAX_SIZE    8000

static char *imap_capabilities[] = {
    "IMAP4rev1",
    "STARTTLS",
    "AUTH=GSSAPI",
    "LOGINDISABLED",
    NULL
};

static char *imaps_capabilities[] = {
    "IMAP4rev1",
    "STARTTLS",
    "AUTH=GSSAPI",
    "AUTH=PLAIN",
    NULL
};
