#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

// This struct holds the title and artist of the currently playing media.
//
struct MediaInfo {
    std::string title;
    std::string artist;

    bool operator==(const MediaInfo &other) const {
        return title == other.title && artist == other.artist;
    }

    bool operator!=(const MediaInfo &other) const { return !(*this == other); }
};

// BT related code
class BluetoothSerial {
  private:
    int fd;
    std::string receiveBuffer;

  public:
    BluetoothSerial() : fd(-1) {}

    // desctuctor to close the connection when the object is destroyed
    ~BluetoothSerial() { close(); }

    // check connecition status
    bool connect(const std::string &device) {
        fd = open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);

        // did'tn connect boom!
        if (fd == -1) {
            std::cerr << "Failed to open " << device << ": "
                      << std::strerror(errno) << '\n';

            return false;
        }

        /*
        * terminos is a POSIX standard for terminal I/O interfaces.
        * It provides a way to configure and control the behavior of terminal
        devices, including serial ports.
        * The termios structure contains various fields that define the settings
        for input and output processing, baud rate, character size, parity, stop
        bits, and other options.
        */
        struct termios tty{};

        // check for the termios failure
        if (tcgetattr(fd, &tty) != 0) {
            std::cerr << "tcgetattr failed: " << std::strerror(errno) << '\n';

            close();
            return false;
        }

        cfsetispeed(&tty, B9600);
        cfsetospeed(&tty, B9600);

        // initializeing the c_cflag field of the termios structure to configure
        // the serial port settings.
        //  it's automatic

        tty.c_cflag &= ~CSIZE; // Clear the character size bits
        tty.c_cflag |= CS8;  // Set character size to 8 bits

        tty.c_cflag &= ~PARENB; // No parity
        tty.c_cflag &= ~CSTOPB; // One stop bit

        tty.c_cflag |= CREAD; // Enable receiver
        tty.c_cflag |= CLOCAL; // Ignore modem control lines

        tty.c_cflag &= ~CRTSCTS; // Disable hardware flow control

        tty.c_iflag = 0; // No input processing
        tty.c_oflag = 0;// No output processing
        tty.c_lflag = 0;// No local processing

        tty.c_cc[VMIN] = 0; // Minimum number of characters to read
        tty.c_cc[VTIME] = 0; // Timeout in deciseconds (0 = no timeout)

        // apply and check fro the failure
        if (tcsetattr(fd, TCSANOW, &tty) != 0) {
            std::cerr << "tcsetattr failed: " << std::strerror(errno) << '\n';

            close();
            return false;
        }

        std::cout << "Connected to " << device << '\n';

        return true;
    }

    // send the messge + `\n`
    bool sendLine(const std::string &message) {
        if (fd == -1)
            return false;

        std::string data = message + "\n";

        const char *ptr = data.c_str();
        size_t remaining = data.size();

        while (remaining > 0) {
            /*
            // write() ->
            /      fd, where to write
            /      ptr, what to write
            /      remaining, how many bytes to write
            */
            ssize_t written = write(fd, ptr, remaining);

            if (written < 0) {
                if (errno == EINTR)
                    continue;

                std::cerr << "Write failed: " << std::strerror(errno) << '\n';

                return false;
            }

            // upadte the scenario for the next write
            ptr += written;
            remaining -= written;
        }

        return true;
    }

    // read line over the bt com 
    bool readLine(std::string &line, int timeoutMs) {
        // find the newline char 
        size_t newline = receiveBuffer.find('\n');

        // if we find the newline char
        // we can extract the line from the buffer and return it
        
        if (newline != std::string::npos) {
            line = receiveBuffer.substr(0, newline);

            receiveBuffer.erase(0, newline + 1);

            // remove the carriage 
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            return true;
        }

        // no newline char:- then we read more data serial port
        struct pollfd pfd{};

        //initializeing
        pfd.fd = fd;
        pfd.events = POLLIN;

        int result = poll(&pfd, 1, timeoutMs);

        if (result <= 0)
            return false;

        char buffer[256];

        // store the number of bytes read from the serial port into the buffer
        ssize_t bytes = read(fd, buffer, sizeof(buffer));

        if (bytes <= 0)
            return false;

        // appen the new data 
        receiveBuffer.append(buffer, static_cast<size_t>(bytes));

        newline = receiveBuffer.find('\n');

        // if we don;t find the \n then return 
        if (newline == std::string::npos)
            return false;

        line = receiveBuffer.substr(0, newline);

        receiveBuffer.erase(0, newline + 1);

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        return true;
    }

    // close the connection
    void close() {
        if (fd != -1) {
            ::close(fd);
            fd = -1;
        }
    }
};

class MediaController {
    /**
     * media playback and retrieving
     */ 
  private:
      //cmd runner
    int runCommand(const std::string &command) {
        return std::system(command.c_str());
    }

    // read the output of the command and return it as a string
    std::string readCommand(const std::string &command) {
        //open teh pipeline to read from 
        FILE *pipe = popen(command.c_str(), "r");

        //check 
        if (!pipe)
            return "";

        char buffer[256];
        std::string result;

        //th og 
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }

        pclose(pipe);

        // remove trailing newline(s)
        while (!result.empty() &&
               (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }

        return result;
    }

    std::string cleanField(std::string value) {
        // Our protocol uses | as a separator,
        // so don't allow it inside fields.
        
        /*
         * input : John Doe | Artist
         * output: John Doe   Artist 
         */ 

        for (char &c : value) {
            if (c == '|' || c == '\n' || c == '\r') {
                c = ' ';
            }
        }

        return value;
    }

  public:
    void playPause() { runCommand("playerctl play-pause"); }

    void next() { runCommand("playerctl next"); }

    void previous() { runCommand("playerctl previous"); }

    MediaInfo getCurrentMedia() {
        MediaInfo info;

        // NOTE: this must be a real tab character (\t), not the two
        // literal characters '\' and 't'. A C++ string literal with
        // "\\t" produces a literal backslash-t, which playerctl prints
        // verbatim instead of substituting a tab -- breaking the
        // title/artist split below. A single "\t" is a real tab.
        std::string result = readCommand("playerctl metadata "
                                         "--format '{{title}}\t{{artist}}' "
                                         "2>/dev/null");

        // title<TAB>artist

        size_t separator = result.find('\t');

        // if we find the separator then we can split the title and artist
        if (separator != std::string::npos) {
            info.title = result.substr(0, separator);

            info.artist = result.substr(separator + 1);
        } else {
            info.title = result;
        }

        info.title = cleanField(info.title);

        info.artist = cleanField(info.artist);

        if (info.title.empty())
            info.title = "No media";

        if (info.artist.empty())
            info.artist = "Unknown";

        return info;
    }
};

// this classs is used to define the protocol for communication between the media controller and the bluetooth device
class Protocol {
  public:
      // check if the line is a hello message
    static bool isHello(const std::string &line) { return line == "HELLO"; }

    // check if the line is a command message
    static bool isCommand(const std::string &line, const std::string &command) {
        return line == "CMD|" + command;
    }


    static std::string makeMediaMessage(const MediaInfo &media) {
        return "MEDIA|" + media.title + "|" + media.artist;
    }
};

int main() {
    BluetoothSerial bluetooth;
    MediaController media;

    if (!bluetooth.connect("/dev/rfcomm0"))
        return 1;

    std::cout << "Media controller running...\n";

    MediaInfo lastMedia = media.getCurrentMedia();

    bluetooth.sendLine(Protocol::makeMediaMessage(lastMedia));

    std::cout << "Now playing: " << lastMedia.title << " - " << lastMedia.artist
              << '\n';

    auto lastMetadataCheck = std::chrono::steady_clock::now();

    //turn on a for-ever loop to keep the program running and checking for new messages from the bluetooth device
    while (true) {
        std::string line;

        // read a line from the bluetooth device with a timeout of 100ms
        if (bluetooth.readLine(line, 100)) {
            std::cout << "Received: " << line << '\n';

            // hello ->  then send the current media info back to the bluetooth device
            if (Protocol::isHello(line)) {
                MediaInfo current = media.getCurrentMedia();

                lastMedia = current;

                bluetooth.sendLine(Protocol::makeMediaMessage(current));
            }

            else if (Protocol::isCommand(line, "PLAY")) {
                media.playPause();
            }

            else if (Protocol::isCommand(line, "NEXT")) {
                media.next();

                // Give player time to update metadata
                usleep(100000);
            }

            else if (Protocol::isCommand(line, "PREV")) {
                media.previous();

                usleep(100000);
            }
        }

        auto now = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastMetadataCheck);

        if (elapsed.count() >= 1000) {
            lastMetadataCheck = now;
        
            MediaInfo current = media.getCurrentMedia();

            if (current != lastMedia) {
                lastMedia = current;

                std::cout << "Media changed: " << current.title << " - "
                          << current.artist << '\n';

                // just sen it 
                bluetooth.sendLine(Protocol::makeMediaMessage(current));
            }
        }
    }

    return 0;
}
