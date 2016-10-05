#include <unabto/unabto_external_environment.h>
#include "socket.h"

uint32_t resolved_v4addr;
nabto_dns_status_t resolve_dns_status = NABTO_DNS_NOT_FINISHED;

void nabto_dns_resolve(const char* id) {
    unsigned long ip;
    int ret =
        sl_NetAppDnsGetHostByName((int8_t*)id, strlen(id), &ip, SL_AF_INET);
    resolve_dns_status = (ret == 0) ? NABTO_DNS_OK : NABTO_DNS_ERROR;
    resolved_v4addr = ip;
}

nabto_dns_status_t nabto_dns_is_resolved(const char* id, uint32_t* v4addr) {
    *v4addr = resolved_v4addr;
    NABTO_LOG_TRACE(("Dns resolved: %03d.%03d.%03d.%03d",
                     MAKE_IP_PRINTABLE(*v4addr)));
    return resolve_dns_status;
}
