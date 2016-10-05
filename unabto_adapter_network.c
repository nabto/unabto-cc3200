#include <errno.h>
#include <stdlib.h>

// Nabto Includes
#include <unabto/unabto_common_main.h>
#include <unabto/unabto_context.h>
#include <unabto/unabto_external_environment.h>
#include <unabto/unabto_util.h>
#include <modules/list/utlist.h>

// Socket Includes
#include "socket.h"

typedef struct socketListElement {
    nabto_socket_t socket;
    struct socketListElement* prev;
    struct socketListElement* next;
} socketListElement;

static struct socketListElement* socketList = 0;

bool nabto_init_socket(uint32_t localAddr, uint16_t* localPort,
                       nabto_socket_t* sock) {
    NABTO_LOG_TRACE(("Open socket: ip=" PRIip ", port=%u",
                     MAKE_IP_PRINTABLE(localAddr), (int)*localPort));

    nabto_socket_t sd = sl_Socket(SL_PF_INET, SL_SOCK_DGRAM, 0);
    if (sd < 0) {
        NABTO_LOG_ERROR(
            ("Unable to create socket: (%i) '%s'.", errno, strerror(errno)));
        return false;
    }

    struct SlSockAddrIn_t sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = SL_AF_INET;
    sa.sin_addr.s_addr = sl_Htonl(localAddr);
    sa.sin_port = sl_Htons(*localPort);

    int status = sl_Bind(sd, (struct SlSockAddr_t*)&sa, sizeof(sa));
    if (status < 0) {
        NABTO_LOG_ERROR(("Unable to bind socket: (%i) '%s' localport %i", errno,
                         strerror(errno), *localPort));
        sl_Close(sd);
        return false;
    }

    SlSockNonblocking_t enableOption;
    enableOption.NonblockingEnabled = 1;
    sl_SetSockOpt(sd, SL_SOL_SOCKET, SL_SO_NONBLOCKING, (uint8_t*)&enableOption,
                  sizeof(enableOption));

    *sock = sd;

    NABTO_LOG_TRACE(("Socket opened: ip=" PRIip ", port=%u",
                     MAKE_IP_PRINTABLE(localAddr), (int)*localPort));

    socketListElement* se =
        (socketListElement*)malloc(sizeof(socketListElement));

    if (!se) {
        sl_Close(sd);
        NABTO_LOG_FATAL(
            ("Malloc of a single small list element should not fail!"));
    }

    se->socket = sd;
    DL_APPEND(socketList, se);

    return true;
}

void nabto_close_socket(nabto_socket_t* sock) {
    if (sock && *sock != NABTO_INVALID_SOCKET) {
        socketListElement* se;
        socketListElement* found = 0;
        DL_FOREACH(socketList, se) {
            if (se->socket == *sock) {
                found = se;
                break;
            }
        }
        if (!found) {
            NABTO_LOG_ERROR(("Socket %i not found in socket list", *sock));
        } else {
            DL_DELETE(socketList, se);
            free(se);
        }

        sl_Close(*sock);
        *sock = NABTO_INVALID_SOCKET;
    }
}

ssize_t nabto_read(nabto_socket_t sock, uint8_t* buf, size_t len,
                   uint32_t* addr, uint16_t* port) {
    struct SlSockAddrIn_t sa;
    SlSocklen_t addrlen = sizeof(sa);

    // read the header first
    ssize_t bytesRecvd =
        sl_RecvFrom(sock, buf, 16, 0, (struct SlSockAddr_t*)&sa, &addrlen);

    if (bytesRecvd < 16) {
        return 0;
    }

    // get actual message length from header
    uint16_t msgLen;
    READ_U16(msgLen , buf + 14);

    if(msgLen > len) {
        msgLen = len;
    }

    // read the remaining message
    bytesRecvd += sl_RecvFrom(sock, buf + 16, msgLen - 16, 0,
                              (struct SlSockAddr_t*)&sa, &addrlen);

    if (bytesRecvd < 16) {
        return 0;
    }

    *addr = sl_Htonl(sa.sin_addr.s_addr);
    *port = sl_Htons(sa.sin_port);

    return bytesRecvd;
}

ssize_t nabto_write(nabto_socket_t sock, const uint8_t* buf, size_t len,
                    uint32_t addr, uint16_t port) {
    struct SlSockAddrIn_t sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = SL_AF_INET;
    sa.sin_addr.s_addr = sl_Htonl(addr);
    sa.sin_port = sl_Htons(port);

    ssize_t bytesSent =
        sl_SendTo(sock, buf, len, 0, (struct SlSockAddr_t*)&sa, sizeof(sa));

    if (bytesSent < 0) {
        NABTO_LOG_ERROR(
            ("ERROR: %s (%i) in nabto_write()", strerror(errno), errno));
    }

    return bytesSent;
}

void wait_event() {
    nabto_stamp_t next_event;
    unabto_next_event(&next_event);

    nabto_stamp_t now = nabtoGetStamp();

    int timeout = nabtoStampDiff2ms(nabtoStampDiff(&next_event, &now));
    if (timeout < 0) {
        timeout = 0;
    }

    struct SlTimeval_t timeout_val;
    timeout_val.tv_sec = timeout / 1000;
    timeout_val.tv_usec = (timeout * 1000) % 1000000;

    SlFdSet_t read_fds;
    SL_FD_ZERO(&read_fds);
    unsigned int max_fd = 0;

    socketListElement* se;
    DL_FOREACH(socketList, se) {
        SL_FD_SET(se->socket, &read_fds);
        max_fd = MAX(max_fd, se->socket);
    }

    int nfds = sl_Select(max_fd + 1, &read_fds, NULL, NULL, &timeout_val);

    if (nfds < 0) {
        NABTO_LOG_ERROR(("Select returned error"));
    } else if (nfds > 0) {
        DL_FOREACH(socketList, se) {
            if (SL_FD_ISSET(se->socket, &read_fds)) {
                while(unabto_read_socket(se->socket));
            }
        }
    }

    unabto_time_event();
}
