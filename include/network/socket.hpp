#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    typedef SOCKET SocketHandle;
#else
#include <sys/socket.h>
#include <unistd.h>
typedef int SocketHandle;
const int INVALID_SOCKET = -1;
#endif

class Socket {
private:
    SocketHandle handle;

public:
    // Constructor: Takes ownership of an existing OS socket
    Socket(SocketHandle h) : handle(h) {}

    // Destructor (RAII): Automatically closes the socket when the object is destroyed
    ~Socket() {
        if (handle != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(handle);
#else
            close(handle);
#endif
        }
    }

    // Delete copy constructors to prevent two objects from trying to close the same socket
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    SocketHandle get() const { return handle; }
};