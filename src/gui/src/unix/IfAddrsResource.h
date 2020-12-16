#include <sys/types.h>
#include <ifaddrs.h>

namespace Debauchee
{

class IfAddrsResource
{
public:
    explicit IfAddrsResource() :
        _valid(getifaddrs(&_ifa) == 0)
    {
    }

    ~IfAddrsResource()
    {
        if (_valid) {
            freeifaddrs(_ifa);
        }
    }

    bool is_valid() const { return _valid; }

    operator struct ifaddrs * () const { return _ifa; }

private:
    bool _valid;
    struct ifaddrs * _ifa;
};

}

