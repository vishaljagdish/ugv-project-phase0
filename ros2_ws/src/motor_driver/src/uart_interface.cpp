#include "uart_interface.hpp"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace {

[[noreturn]] void throwErrno(const std::string& what)
{
	throw std::system_error(errno, std::generic_category(), what);
}

}

UartDevice::UartDevice(std::string path, speed_t baud)
	: path_(std::move(path)), baud_(baud)
{
	fd_ = ::open(path_.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (fd_ < 0) {
		throwErrno("open " + path_);
	}

	//The destructor does not run if the constructor throws,
	//so close the fd here before propagating.
	try {
		configure();
	} catch (...) {
		::close(fd_);
		fd_ = -1;
		throw;
	}
}

UartDevice::~UartDevice()
{
	close();
}

bool UartDevice::isOpen() const noexcept
{
	return fd_ >= 0;
}

const std::string& UartDevice::path() const noexcept
{
	return path_;
}

void UartDevice::configure()
{
	struct termios tty;

	if (tcgetattr(fd_, &tty) != 0) {
		throwErrno("tcgetattr " + path_);
	}

	cfmakeraw(&tty);
	if (cfsetispeed(&tty, baud_) != 0 || cfsetospeed(&tty, baud_) != 0) {
		throwErrno("cfsetspeed " + path_);
	}

	tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
	tty.c_cflag |= CS8 | CREAD | CLOCAL;

	//read() returns as soon as any byte arrives, or after 0.5 s with nothing.
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 5;

	if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
		throwErrno("tcsetattr " + path_);
	}

	//Drop anything that arrived before the port was configured.
	tcflush(fd_, TCIFLUSH);
}

void UartDevice::requireOpen() const
{
	if (fd_ < 0) {
		throw std::logic_error("UartDevice: " + path_ + " is closed");
	}
}

void UartDevice::writeAll(std::string_view data)
{
	requireOpen();

	const char *p = data.data();
	size_t remaining = data.size();

	while (remaining > 0) {
		ssize_t n = ::write(fd_, p, remaining);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			throwErrno("write " + path_);
		}
		p += n;
		remaining -= static_cast<size_t>(n);
	}
}

ssize_t UartDevice::read(uint8_t *buf, size_t len)
{
	requireOpen();

	for (;;) {
		ssize_t n = ::read(fd_, buf, len);
		if (n >= 0) {
			return n;
		}
		if (errno != EINTR) {
			throwErrno("read " + path_);
		}
	}
}

void UartDevice::drain()
{
	requireOpen();

	while (tcdrain(fd_) != 0) {
		if (errno != EINTR) {
			throwErrno("tcdrain " + path_);
		}
	}
}

void UartDevice::flushInput()
{
	requireOpen();

	if (tcflush(fd_, TCIFLUSH) != 0) {
		throwErrno("tcflush " + path_);
	}
}

void UartDevice::close() noexcept
{
	if (fd_ < 0) {
		return;
	}

	//Best effort: make sure a final stop command actually leaves the port.
	while (tcdrain(fd_) != 0 && errno == EINTR) {
	}

	::close(fd_);
	fd_ = -1;
}
