#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Debauchee
{

class SocketResource
{
public:
    explicit SocketResource(int domain, int type, int protocol) :
        _fd(socket(domain, type, protocol))
    {
    }

    ~SocketResource()
    {
        if (is_valid()) {
            close(_fd);
        }
    }

    bool is_valid() const { return _fd >= 0; }

    operator int() const { return _fd; }

private:
    int _fd;
};

}

