#define _GNU_SOURCE
#include <dlfcn.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

static int (*real_bind)(int, const struct sockaddr *, socklen_t) = NULL;
static int (*real_setsockopt)(int, int, int, const void *, socklen_t) = NULL;

static void logmsg(const char *msg) {
  write(2, msg, strlen(msg));
}

static void init_syms(void) {
    if (!real_bind)
        real_bind = dlsym(RTLD_NEXT, "bind");
    if (!real_setsockopt)
        real_setsockopt = dlsym(RTLD_NEXT, "setsockopt");
}

static int retry_bind(int sockfd,
                      struct sockaddr *addr,
                      socklen_t addrlen,
                      uint16_t port,
                      int is_ipv6)
{
    for (int i = 0; i < 16; i++) {  // allow up to 16 instances
        // update the port in the sockaddr structure each retry
        if (is_ipv6) {
            struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
            a6->sin6_port = htons(port);
        } else {
            struct sockaddr_in *a4 = (struct sockaddr_in *)addr;
            a4->sin_port = htons(port);
        }

        int ret = real_bind(sockfd, addr, addrlen);
        if (ret == 0)
            return 0;

        if (errno != EADDRINUSE && errno != EACCES)
            return ret;

        port++; // increment for next attempt
        fprintf(stderr, "bindshim: port in use, retrying on %u\n", port);
    }

    return -1;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    logmsg("bindshim: bind() called\n");
    init_syms();

    /* IPv6 */
    if (addr && addr->sa_family == AF_INET6) {
        struct sockaddr_in6 a6;
        memcpy(&a6, addr, sizeof(a6));

        uint16_t port = ntohs(a6.sin6_port);

        /* Only touch the problematic port range */
        if (port >= 51220 && port < 51240) {
            int one = 1;
            real_setsockopt(sockfd, IPPROTO_IPV6,
                            IPV6_V6ONLY, &one, sizeof(one));

            uint16_t newport = port;
            a6.sin6_port = htons(newport);

            return retry_bind(
                sockfd,
                (struct sockaddr *)&a6,
                sizeof(a6),
                port,
                1
            );
        }
    }

    /* IPv4 */
    if (addr && addr->sa_family == AF_INET) {
        struct sockaddr_in a4;
        memcpy(&a4, addr, sizeof(a4));

        uint16_t port = ntohs(a4.sin_port);

        if (port >= 51220 && port < 51240) {
            uint16_t newport = port;
            a4.sin_port = htons(newport);

            return retry_bind(
                sockfd,
                (struct sockaddr *)&a4,
                sizeof(a4),
                port,
                0
            );
        }
    }

    return real_bind(sockfd, addr, addrlen);
}

