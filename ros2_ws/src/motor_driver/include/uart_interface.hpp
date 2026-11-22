//Serial Port Interface. Configuration: 8N1, raw mode, no flow control.
//
//RAII: a constructed UartDevice always has an open, configured port.
//Moves bytes only; knows nothing about the board protocol.
//Errors are reported by throwing std::system_error (errno attached).

#ifndef UART_INTERFACE_HPP
#define UART_INTERFACE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <termios.h>

class UartDevice {
	public:
		//Opens and configures the port. Throws std::system_error on failure.
		//baud is a termios constant, e.g. B115200.
		UartDevice(std::string path, speed_t baud);

		//Drains pending output, then closes. Never throws.
		~UartDevice();

		UartDevice(const UartDevice& dev) = delete;
		UartDevice& operator=(const UartDevice& dev) = delete;

		bool isOpen() const noexcept;
		const std::string& path() const noexcept;

		//Writes every byte of data (retries short writes and EINTR).
		void writeAll(std::string_view data);

		//Reads up to len bytes into buf. Blocks for at most ~0.5 s (VTIME=5).
		//Returns the number of bytes read; 0 means timeout, no data.
		ssize_t read(uint8_t *buf, size_t len);

		//Blocks until all written bytes have been transmitted.
		void drain();

		//Discards received but unread bytes.
		void flushInput();

		//Drains and closes. Safe to call more than once. Never throws.
		void close() noexcept;

	private:
		void configure();
		void requireOpen() const;

		std::string path_;
		speed_t baud_;
		int fd_ = -1;
};

#endif
