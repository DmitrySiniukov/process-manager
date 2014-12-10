#include <stdexcept>

class OpenProcessError : public std::exception
{
public:
	OpenProcessError(const char* errStr) :exception(errStr)
	{
	}
};

class QueryInfoProcError : public std::exception
{
public:
	QueryInfoProcError(const char* errStr) :exception(errStr)
	{
	}

};

class ReadMemoryError : public std::exception
{
public:
	ReadMemoryError(const char* errStr) :exception(errStr)
	{
	}
};

class DebuggerError : public std::exception
{
public:
	DebuggerError(const char* errStr) :exception(errStr)
	{
	}
};

class CreateProcessError : public std::exception
{
public:
	CreateProcessError(const char* errStr) :exception(errStr)
	{
	}
};

class OpenFileError : public std::exception
{
public:
	OpenFileError(const char* errStr) :exception(errStr)
	{
	}
};

class LocalTimeError : public std::exception
{
public:
	LocalTimeError(const char* errStr) :exception(errStr)
	{
	}
};

class AscTimeError : public std::exception
{
public:
	AscTimeError(const char* errStr) :exception(errStr)
	{
	}
};