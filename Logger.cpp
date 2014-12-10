#include "Logger.h"
#include "Exceptions.h"
#include <ctime>
#include <iostream>

size_t Logger::logNumber = 0;

Logger::Logger()
{
	++logNumber;
	filename = std::string("");
}

Logger::Logger(const std::string& cmdLine)
{
	++logNumber;
	open(cmdLine);
}

Logger::Logger(DWORD pId)
{
	++logNumber;
	open(pId);
}

Logger::Logger(const std::string& nameOfFile, const std::string& cmdLine)
{
	++logNumber;
	filename = nameOfFile;
	out.open(filename);
	if (!out)
	{
		std::cout << "Can't open file" << std::endl;
		throw OpenFileError("Error opening file.");
	}
	out << "############################################################################" << std::endl;
	out << "============	The log file for a process with command line:	============" << std::endl <<
		cmdLine << std::endl;
	out << "############################################################################" << std::endl << std::endl;
	out.close();
}

Logger& Logger::operator<<(const std::string& message)
{
	time_t sec = time(NULL);
	tm timeInfo;
	std::string date, temp;
	errno_t err = localtime_s(&timeInfo, &sec);
	char buf[26];
	if (err || asctime_s(buf, 26, &timeInfo))
		date = "00\XXX\0000 00:00:00";
	else
	{
		temp = std::string(buf);
		date = temp.substr(8, 2) + "\\" + temp.substr(4, 3) + "\\" + temp.substr(20, 4) + " " + temp.substr(11, 8) + " : ";
	}
	out.open(filename, std::ios::out | std::ios::app);
	out << date << message << std::endl;
	out.close();

	return *this;
}

void Logger::open(const std::string& cmdLine)
{
	try
	{
		openService(logNumber, filename);
	}
	catch (std::exception&)
	{
		char buf[10];
		_ltoa_s(logNumber, buf, 10, 10);
		filename = std::string("log_XXX_00_00_00_" + std::string(buf));
	}
	out.open(filename);
	if (!out)
	{
		std::cout << "Can't open file" << std::endl;
		throw OpenFileError("Error opening file.");
	}
	out << "############################################################################" << std::endl;
	out << "============	The log file for a process with command line:	============" << std::endl <<
		cmdLine << std::endl;
	out << "############################################################################" << std::endl << std::endl;
	out.close();
}

void Logger::open(DWORD pId)
{
	try
	{
		openService(logNumber, filename);
	}
	catch (std::exception&)
	{
		char buf[10];
		_ltoa_s(logNumber, buf, 10, 10);
		filename = std::string("log_XXX_00_00_00_" + std::string(buf));
	}
	char buf[10];
	_ltoa_s(pId, buf, 10, 10);
	out.open(filename);
	if (!out)
	{
		std::cout << "Can't open file" << std::endl;
		throw OpenFileError("Error opening file.");
	}
	out << "############################################################################" << std::endl;
	out << "============	The log file for a process with initial id:		============" << std::endl <<
		buf << std::endl;
	out << "############################################################################" << std::endl << std::endl;
	out.close();
}

void Logger::openService(size_t logNum, std::string& fName)
{
	time_t sec = time(NULL);
	tm timeInfo;
	std::string date, temp;
	if (localtime_s(&timeInfo, &sec))
	{
		std::cout << "Can't open file" << std::endl;
		throw LocalTimeError("Error getting local time.");
	}
	char buf1[26];
	if (asctime_s(buf1, 26, &timeInfo))
	{
		std::cout << "Can't open file" << std::endl;
		throw AscTimeError("Error getting time string.");
	}

	char buf2[10];
	_ltoa_s(logNum, buf2, 10, 10);

	fName = ("log_" + std::string(buf1).substr(0, 3) + "_" + std::string(buf1).substr(11, 2) + "_" +
		std::string(buf1).substr(14, 2) + "_" + std::string(buf1).substr(17, 2) + "_" + std::string(buf2) + ".txt");
}