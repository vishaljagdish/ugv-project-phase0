//Serial Port Interface. Configuration: 8N1

#ifndef UART_INTERFACE_HPP
#define UART_INTERFACE_HPP

#include <termios.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>

class UartDevice {
	public:
		UartDevice(char *filename, int baudrate);
		~UartDevice();
		UartDevice(const UartDevice& dev) = delete;
		UartDevice& operator=(const UartDevice& dev) = delete;
	private:
		int writeData(char *buf, size_t len);
		int writeString(char *string);
		int readData(char *buf, size_t len);
		char *filename_;
		int baudrate_;
		int fd_ = -1;
};

#endif
