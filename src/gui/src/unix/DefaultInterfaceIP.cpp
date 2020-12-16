#include "IfAddrsResource.h"
#include "SocketResource.h"
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __linux__
#include <linux/wireless.h>
#else
#include <net/if.h>
#include <net/if_media.h>
#endif

#include <string>
#include <cstring>

namespace Debauchee
{

static bool is_wireless(const char * ifname, const SocketResource& sock)
{
#ifdef __linux__
    struct iwreq req;
    ::memset(&req, 0, sizeof(struct iwreq));
    ::strncpy(req.ifr_name, ifname, IFNAMSIZ - 1);
    return ioctl(sock, SIOCGIWMODE, &req) >= 0;
#else
    struct ifmediareq req;
    ::memset(&req, 0, sizeof(struct ifmediareq));
    ::strncpy(req.ifm_name, ifname, IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFMEDIA, &req) >= 0 &&
        (req.ifm_status & IFM_AVALID)
    ) {
        return IFM_TYPE(req.ifm_active) == IFM_IEEE80211;
    }
    return false;
#endif
}

static bool is_wireless(const char * ifname)
{
    if (ifname) {
        SocketResource sock(AF_INET, SOCK_DGRAM, 0);
        if (sock.is_valid()) {
            return is_wireless(ifname, sock);
        }
    }
    return false;
}

std::string default_interface_ip()
{
    std::string wirelessAddress;
    IfAddrsResource ifa;
    if (ifa.is_valid()) {
        for (struct ifaddrs * next = ifa; next; next = next->ifa_next) {
            auto sain = (struct sockaddr_in *)next->ifa_addr;
            if (!sain || sain->sin_family != AF_INET ||
                !(next->ifa_flags & IFF_RUNNING) ||
                (next->ifa_flags & IFF_LOOPBACK)
               ) {
                continue;
            }
            std::string address = inet_ntoa(sain->sin_addr);
            // take first wired address right away
            if (!is_wireless(next->ifa_name)) {
                return address;
            }
            // save first wireless address to be used if we don't find a wired one
            if (wirelessAddress.empty()) {
                wirelessAddress = address;
            }
        }
    }
    return wirelessAddress;
}

}

