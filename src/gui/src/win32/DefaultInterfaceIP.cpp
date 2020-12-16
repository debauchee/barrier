// REQUIRES: Ws2_32.lib, Iphlpapi.lib
// REQUIRES: WSAStartup(..)

// see comments regarding WSAAddressToStringA in address_string(..)
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <iphlpapi.h>
#include <string>
#include "HeapResource.h"

namespace Debauchee
{

static ULONG get_addresses(IP_ADAPTER_ADDRESSES * addresses, PULONG sz)
{
    return GetAdaptersAddresses(AF_INET,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME,
        NULL, addresses, sz);
}

static std::string address_string(const SOCKET_ADDRESS& address)
{
    // planning to use this with Qt but QString does not have a c'tor
    // for wide strings so we need to force the ANSI API here
    DWORD szStrAddress = 1023;
    char strAddress[1024];
    if (WSAAddressToStringA(address.lpSockaddr, address.iSockaddrLength, NULL, strAddress, &szStrAddress)) {
        return "";
    }
    return strAddress;
}

static std::string first_unicast_address(IP_ADAPTER_UNICAST_ADDRESS * unicast)
{
    while (unicast) {
        auto address = address_string(unicast->Address);
        if (!address.empty()) {
            return address;
        }
        unicast = unicast->Next;
    }
    return std::string();
}

static bool is_useable_interface(const IP_ADAPTER_ADDRESSES * ifa)
{
    return
        // is an ethernet or wireless interface AND
        (ifa->IfType == IF_TYPE_ETHERNET_CSMACD || ifa->IfType == IF_TYPE_IEEE80211) &&
        // is up
        ifa->OperStatus == IfOperStatusUp;
}

static std::string default_ip_address(const IP_ADAPTER_ADDRESSES * next)
{
    std::string wirelessAddress;
    for ( ; next; next = next->Next) {
        if (!is_useable_interface(next)) {
            continue;
        }
        std::string address = first_unicast_address(next->FirstUnicastAddress);
        if (address.empty()) {
            continue;
        }
        // take first wired address right away
        if (next->IfType == IF_TYPE_ETHERNET_CSMACD) {
            return address;
        }
        // save first wireless address to be used if we don't find a wired one
        if (wirelessAddress.empty()) {
            wirelessAddress = address;
        }
    }
    return wirelessAddress;
}

std::string default_interface_ip()
{
    const ULONG DefaultSzAddresses = 15 * 1024; // 15k recommended by msdn
    ULONG szAddresses = DefaultSzAddresses;
    HeapResource<IP_ADAPTER_ADDRESSES> addresses(GetProcessHeap(), 0, DefaultSzAddresses);
    if (addresses.is_valid()) {
        ULONG result;
        while (ERROR_BUFFER_OVERFLOW == (result = get_addresses(addresses, &szAddresses))) {
            // add more space in case more adapters have shown up
            szAddresses += DefaultSzAddresses;
            swap(addresses, HeapResource<IP_ADAPTER_ADDRESSES>(GetProcessHeap(), 0, szAddresses));
            if (!addresses.is_valid()) {
                break;
            }
        }
        if (result == ERROR_SUCCESS) {
            return default_ip_address(addresses);
        }
    }
    return std::string();
}

}
