#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <iostream>

int main()
{
    int fd = open("/dev/rfcomm0", O_RDONLY);

    if (fd == -1)
    {
        std::cerr << "Failed to open /dev/rfcomm0\n";
        return 1;
    }

    // Get current serial configuration
    termios tty{};

    if (tcgetattr(fd, &tty) != 0)
    {
        std::cerr << "Failed to get serial configuration\n";
        close(fd);
        return 1;
    }

    // Configure baud rate
    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);

    // 8 data bits
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    // No parity
    tty.c_cflag &= ~PARENB;

    // One stop bit
    tty.c_cflag &= ~CSTOPB;

    // Disable hardware flow control
    tty.c_cflag &= ~CRTSCTS;

    // Enable receiver
    tty.c_cflag |= CREAD;

    // Ignore modem control lines
    tty.c_cflag |= CLOCAL;

    // Raw input
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // Raw output
    tty.c_oflag &= ~OPOST;

    // Read returns when at least 1 byte arrives
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    // Apply configuration
    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        std::cerr << "Failed to configure serial device\n";
        close(fd);
        return 1;
    }

    std::cout << "Connected to HC-06!\n";

    char buffer[128];

    while (true)
    {
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            std::cout << buffer << std::flush;
        }
        else if (bytes_read == -1)
        {
            std::cerr << "Error reading from HC-06\n";
            break;
        }
    }

    close(fd);

    return 0;
}
