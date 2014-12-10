#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <string>
#include <Windows.h>

const std::string noCmdLine("-1");

class Logger
{
public:
	Logger();
	Logger(const std::string& cmdLine);
	Logger(const std::string& nameOfFile, const std::string& cmdLine);
	Logger(DWORD pId);
	Logger& operator<<(const std::string& message);
	void open(const std::string& cmdLine);
	void open(DWORD pId);

protected:
private:
	std::ofstream out;
	std::string filename;
	static size_t logNumber;

	static void openService(size_t logNum, std::string& fName);
};

#endif