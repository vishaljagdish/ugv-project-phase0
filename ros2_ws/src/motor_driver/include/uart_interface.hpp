//Serial Port Interface. Configuration: 8N1

#ifndef UART_INTERFACE_HPP
#define UART_INTERFACE_HPP

#include <termios.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <string>

class UartDevice {
	public:
		UartDevice(std::string filename, speed_t baudrate);
		~UartDevice();
		UartDevice(const UartDevice& dev) = delete;
		UartDevice& operator=(const UartDevice& dev) = delete;
	private:
		int writeData(const uint8_t *data, size_t len);
		int writeString(std::string_view s);
		int readData(uint8_t *buf, size_t len);
		std::string filename_;
		speed_t baudrate_;
		int fd_ = -1;
};

#endif
