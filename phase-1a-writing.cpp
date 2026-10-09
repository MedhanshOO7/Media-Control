#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cstring>

int main()
{
    int fd = open("/dev/rfcomm0", O_WRONLY);

    if (fd == -1)
    {
        std::cerr << "Open failed: "
                  << strerror(errno) << '\n';
        return 1;
    }

    char c = 'A';

    ssize_t bytes = write(fd, &c, 1);

    std::cout << "Sent " << bytes << " byte\n";

    close(fd);
}
