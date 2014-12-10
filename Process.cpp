#include "Process.h"
#include <iostream>
#include <TlHelp32.h>

Process::Process(const wchar_t* cmdLine,
	const wchar_t* argums,
	bool stopWhenDestruct,
	void(*onProcStartFunc)(),
	void(*onProcCrashFunc)(),
	void(*onProcExitFunc)(),
	void(*onProcManuallyStoppedFunc)(),
	void(*onProcSuspendFunc)(),
	void(*onProcResumeFunc)()
	)
{
	onProcStart = onProcStartFunc;
	onProcCrash = onProcCrashFunc;
	onProcExit = onProcExitFunc;
	onProcManuallyStopped = onProcManuallyStoppedFunc;
	onProcSuspend = onProcSuspendFunc;
	onProcResume = onProcResumeFunc;
	initService(cmdLine, argums, stopWhenDestruct);
	log.open(commandLine);
	createProc();
	watchThread = std::thread(watchThreadFunction, std::ref(*this));
}

Process::Process(const char* cmdLine,
	const char* argums,
	bool stopWhenDestruct,
	void(*onProcStartFunc)(),
	void(*onProcCrashFunc)(),
	void(*onProcExitFunc)(),
	void(*onProcManuallyStoppedFunc)(),
	void(*onProcSuspendFunc)(),
	void(*onProcResumeFunc)()
	)
{
	onProcStart = onProcStartFunc;
	onProcCrash = onProcCrashFunc;
	onProcExit = onProcExitFunc;
	onProcManuallyStopped = onProcManuallyStoppedFunc;
	onProcSuspend = onProcSuspendFunc;
	onProcResume = onProcResumeFunc;
	initService(multToUni(cmdLine), multToUni(argums), stopWhenDestruct);
	log.open(commandLine);
	createProc();
	watchThread = std::thread(watchThreadFunction, std::ref(*this));
}

Process::Process(DWORD pId,
	bool stopWhenDestruct,
	void(*onProcStartFunc)(),
	void(*onProcCrashFunc)(),
	void(*onProcExitFunc)(),
	void(*onProcManuallyStoppedFunc)(),
	void(*onProcSuspendFunc)(),
	void(*onProcResumeFunc)()
	)
{
	onProcStart = onProcStartFunc;
	onProcCrash = onProcCrashFunc;
	onProcExit = onProcExitFunc;
	onProcManuallyStopped = onProcManuallyStoppedFunc;
	onProcSuspend = onProcSuspendFunc;
	onProcResume = onProcResumeFunc;
	initService(L"", L"", stopWhenDestruct);
	log.open(pId);
	getCmd(pId);
	watchThread = std::thread(watchThreadFunction, std::ref(*this));
}

Process::~Process()
{
	threadMutex.lock();
	exitThread = true;
	threadMutex.unlock();
	if (watchThread.joinable())
		watchThread.join();
	if (stopWhenDestr && procStatus == ACTIVE)
		stopProcess();
	log << "Deleting the process instance...";
	CloseHandle(procInfo.hProcess);
	CloseHandle(procInfo.hThread);
}

void Process::initService(const wchar_t* cmdLine, const wchar_t* args, bool stopWhenDestuct)
{
	commandLine = (!wcslen(args)) ? std::string(uniToMult(cmdLine)) :
		(("\"" + std::string(uniToMult(cmdLine)) + "\" " + std::string(uniToMult(args))).c_str());
	procInfo.hProcess = NULL;
	procInfo.hThread = NULL;
	stopWhenDestr = stopWhenDestuct;
	exitThread = false;
	procStatus = ACTIVE;
}

void Process::watchThreadFunction(Process& proc)
{
	DWORD exitCode;
	BOOL isSuspended = FALSE;
	
	proc.threadMutex.lock();
	proc.log << "The watchThread is created";
	proc.threadMutex.unlock();

	while (1)
	{
		proc.threadMutex.lock();
		if (proc.exitThread)
		{
			proc.log << "Exiting from thread...";
			proc.threadMutex.unlock();
			return;
		}
		GetExitCodeProcess(proc.procInfo.hProcess, &exitCode);
		proc.threadMutex.unlock();

		if (exitCode == STILL_ACTIVE)
		{
			proc.threadMutex.lock();
			CheckRemoteDebuggerPresent(proc.procInfo.hProcess, &isSuspended);
			if (isSuspended)
				proc.procStatus = SUSPENDED;
			else
				proc.procStatus = ACTIVE;
			proc.threadMutex.unlock();
		}
		else if (exitCode == STOP_VIA_METHOD_CALL)
		{
			proc.threadMutex.lock();
			proc.procStatus = STOPPED;
			proc.threadMutex.unlock();
		}
		else
		{
			proc.threadMutex.lock();
			if (!exitCode)
			{
				proc.log << "Unexpected process closing.";
				proc.onProcExit();
			}
			else
			{
				proc.log << "The process was closed because of a crash.";
				proc.onProcCrash();
			}
			proc.procStatus = RESTARTING;
			proc.log << "The process is restarting...";
			proc.threadMutex.unlock();
			Sleep(300); // In order to clarity
			proc.threadMutex.lock();
			proc.createProc();
			proc.threadMutex.unlock();
		}
	}
}

//
// Typedefs for getCmd function
//
typedef NTSTATUS(NTAPI* queryInfoProc)(IN HANDLE procHandle, ULONG procInfoClass, OUT PVOID procInfo,
	IN ULONG procInfoLength, OUT PULONG returnLength OPTIONAL);

typedef NTSTATUS(NTAPI* readMemory64)(IN HANDLE procHandle, IN PVOID64 baseAddress, OUT PVOID buffer,
	IN ULONG64 size, OUT PULONG64 numOfBytes);

typedef class
{
public:
	PVOID reserved1;
	PVOID pebAddress;
	PVOID reserved2[2];
	ULONG_PTR uniqueProcessId;
	PVOID reserved3;
} ProcBasicInfo;

typedef class
{
public:
	PVOID reserved1[2];
	PVOID64 pebAddress;
	PVOID reserved2[4];
	ULONG_PTR uniqueProcId[2];
	PVOID reserved3[2];
} ProcBasicInfo64;

typedef class
{
public:
	USHORT length;
	USHORT maxLength;
	PWSTR  buffer;
} uniStr;

typedef struct uniStr64 {
	USHORT length;
	USHORT maxLength;
	PVOID64 buffer;
} uniStr64;

/*
	I attempted to write a multi-platform function. It works both on 32 and 64 bit processes.
	But errors may occur if you try to handle system processes.
*/
void Process::getCmd(DWORD pId)
{
	PBYTE* params;
	SYSTEM_INFO sysInfo;
	wchar_t* tempCmdLine;

	ZeroMemory(&sInfo, sizeof(sInfo));
	sInfo.cb = sizeof(sInfo);
	ZeroMemory(&procInfo, sizeof(procInfo));
	procInfo.dwProcessId = pId;

	// An error may occur if you try to handle a system process
	if (!(procInfo.hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procInfo.dwProcessId)))
	{
		DWORD err = GetLastError();

		if (err == ERROR_ACCESS_DENIED)
		{
			log << "Can't open the process: access denied.";
			throw OpenProcessError("Can't open the process: access denied.");
		}
		else
		{
			log << "Error opening the process";
			throw OpenProcessError("Error opening the process.");
		}
	}

	GetNativeSystemInfo(&sysInfo);
	DWORD paramOffset = sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? 0x20 : 0x10;
	DWORD CommandLineOffset = sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? 0x70 : 0x40;

	BOOL is64;
	IsWow64Process(GetCurrentProcess(), &is64);

	DWORD pebSize = paramOffset + 8;
	PBYTE peb = new BYTE[pebSize];
	ZeroMemory(peb, pebSize);

	DWORD ppSize = CommandLineOffset + 16;
	PBYTE pp = new BYTE[ppSize];
	ZeroMemory(peb, pebSize);

	if (is64)
	{
		ProcBasicInfo64 procBasicInfo;
		ZeroMemory(&procBasicInfo, sizeof(procBasicInfo));
		queryInfoProc query = (queryInfoProc)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64QueryInformationProcess64");

		if (query(procInfo.hProcess, 0, &procBasicInfo, sizeof(procBasicInfo), NULL))
		{//exception
			log << "Getting process information error.";
			throw QueryInfoProcError("Getting process information error.");
		}
		readMemory64 read = (readMemory64)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64ReadVirtualMemory64");
		if (read(procInfo.hProcess, procBasicInfo.pebAddress, peb, pebSize, NULL))
		{//exception
			log << "Error reading memory.";
			throw ReadMemoryError("Error reading memory.");
		}
		params = (PBYTE*)*(LPVOID*)(peb + paramOffset); // address in remote process adress space
		if (read(procInfo.hProcess, params, pp, ppSize, NULL))
		{//exception
			log << "Error reading memory.";
			throw ReadMemoryError("Error reading memory.");
		}
		uniStr64* pCommandLine = (uniStr64*)(pp + CommandLineOffset);
		tempCmdLine = new wchar_t[pCommandLine->maxLength / 2];
		if (read(procInfo.hProcess, pCommandLine->buffer, tempCmdLine, pCommandLine->maxLength, NULL))
		{//exception
			log << "Error reading memory.";
			throw ReadMemoryError("Error reading memory.");
		}
	}
	else
	{
		ProcBasicInfo procBasicInfo;
		ZeroMemory(&procBasicInfo, sizeof(procBasicInfo));
		queryInfoProc query = (queryInfoProc)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");

		if (query(procInfo.hProcess, 0, &procBasicInfo, sizeof(procBasicInfo), NULL))
		{//exception
			log << "Getting process information error.";
			throw QueryInfoProcError("Getting process information error.");
		}
		if (!ReadProcessMemory(procInfo.hProcess, procBasicInfo.pebAddress, peb, pebSize, NULL))
		{//exception
			log << "Error reading memory.";
			throw ReadMemoryError("Error reading memory.");
		}
		PBYTE* params = (PBYTE*)*(LPVOID*)(peb + paramOffset);
		if (!ReadProcessMemory(procInfo.hProcess, params, pp, ppSize, NULL))
		{//exception
			log << "Error reading memory.";
			throw ReadMemoryError("Error reading memory.");
		}
		uniStr* pCommandLine = (uniStr*)(pp + CommandLineOffset);
		tempCmdLine = new wchar_t[pCommandLine->maxLength / 2];
		if (!ReadProcessMemory(procInfo.hProcess, pCommandLine->buffer, tempCmdLine, pCommandLine->maxLength, NULL))
		{//exception
			log << "Error reading memory.";
			throw ReadMemoryError("Error reading memory.");
		}
	}
	commandLine = std::string(uniToMult(tempCmdLine));
	log << (std::string("Connected to the process with command line: ") + commandLine);
}

// The function creates a new process
bool Process::createProc()
{
	CloseHandle(procInfo.hProcess);
	CloseHandle(procInfo.hThread);
	ZeroMemory(&sInfo, sizeof(sInfo));
	sInfo.cb = sizeof(sInfo);
	ZeroMemory(&procInfo, sizeof(procInfo));

	if (!CreateProcessW(NULL, multToUni(commandLine.c_str()),
		NULL, NULL, FALSE, 0, NULL, NULL, &sInfo, &procInfo))
	{//exception
		log << "Creating process error.";
		throw CreateProcessError("Creating process error.");
	}
	if (procStatus == STOPPED)
		log << "The process started after manual stop.";
	else
		log << "The process started.";
	onProcStart();
	return true;
}

void Process::testClassFunction()
{
	// Create a test file
	std::ofstream out("./test.txt");
	out << "Hello, Kodisoft!" << std::endl;
	out.close();


	const wchar_t* path = L"notepad.exe";
	const wchar_t* args = L"./test.txt";
	char* statusStrs[] = { "ACTIVE", "RESTARTING", "SUSPENDED", "STOPPED" };
	DWORD p1Id; //
	Process* p1; // The first process

	try
	{
		p1 = new Process(path,
			args,
			false,
			[](){std::cout << "The process p1 started." << std::endl; },
			[](){std::cout << "The process p1 crashed" << std::endl; },
			[](){std::cout << "The process p1 unexpectedly closed." << std::endl; },
			[](){std::cout << "The process p1 manually stopped." << std::endl; },
			[](){std::cout << "The process p1 suspended." << std::endl; },
			[](){std::cout << "The process p1 resumed" << std::endl; });
		while (!p1->getProcessId(p1Id)) // Get the p1 id
		{
			std::cerr << "Can't get p1 id." << std::endl;
		}
		delete p1;

	}
	catch (CreateProcessError& err)
	{
		std::cerr << "Error creating process." << std::endl;
		return;
	}
	catch (OpenFileError& err)
	{
		std::cerr << "Error opening file." << std::endl;
		return;
	}
	Process p2(p1Id,
		true,
		[](){std::cout << "The process p2 started." << std::endl; },
		[](){std::cout << "The process p2 crashed" << std::endl; },
		[](){std::cout << "The process p2 unexpectedly closed." << std::endl; },
		[](){std::cout << "The process p2 manually stopped." << std::endl; },
		[](){std::cout << "The process p2 suspended." << std::endl; },
		[](){std::cout << "The process p2 resumed" << std::endl; });

	std::cout << "p2 status: " << statusStrs[p2.getStatus()] << std::endl;
	Sleep(4000);
	p2.suspendProcess();
	Sleep(4000); // You can try to move the process window in order to check it.
	p2.suspendProcess(); // Attempt to suspend it again.
	p2.resumeProcess();
	Sleep(4000); // The process is active.
	p2.stopProcess();
	Sleep(1000); // The process is closed.
	p2.stopProcess(); // Attempt to stop it again.
	p2.startProcess();
	Sleep(4000); // The process is active again.
	p2.startProcess(); // Try to start it again.
	p2.restartProcess(); // Restart process

	// The cycle shows current process status
	// you can close process in order to check the restarting
	for (size_t i = 0; i < 500; i++)
	{
		// Outputs process status
		std::cout << statusStrs[p2.getStatus()] << std::endl;
		Sleep(10);
	}
	std::cout << "Deleting p2 instance, the process is closing..." << std::endl;
}

bool Process::suspendProcess()
{
	threadMutex.lock();
	bool status = DebugActiveProcess(procInfo.dwProcessId) ? true : false;
	if (status)
	{
		log << "The process is suspended.";
		onProcSuspend();
	}
	else
		log << "An attempt to suspend the process. Failure.";
	threadMutex.unlock();
	return status;
}

bool Process::resumeProcess()
{
	threadMutex.lock();
	bool status = DebugActiveProcessStop(procInfo.dwProcessId) ? true : false;
	if (status)
	{
		log << "The process is resumed.";
		onProcResume();
	}
	else
		log << "An attempt to resume the process. Failure.";
	threadMutex.unlock();
	return status;
}

bool Process::restartProcess()
{
	threadMutex.lock();
	log << "Restarting process...";
	threadMutex.unlock();
	return (stopProcess() && createProc());
}

bool Process::stopProcess()
{
	threadMutex.lock();
	bool status = TerminateProcess(procInfo.hProcess, STOP_VIA_METHOD_CALL) ? true : false;
	if (status)
	{
		log << "The process is stopped.";
		onProcManuallyStopped();
	}
	else
		log << "An attempt to stop the process. Failure.";
	threadMutex.unlock();
	return status;
}

bool Process::startProcess()
{
	bool status = false;
	threadMutex.lock();
	if (procStatus == STOPPED)
		status = createProc();
	threadMutex.unlock();
	return status;
}

bool Process::getProcessHandle(HANDLE pHandle)
{
	threadMutex.lock();
	if (procStatus == ACTIVE)
		pHandle = procInfo.hProcess;
	else
	{
		threadMutex.unlock();
		return false;
	}
	threadMutex.unlock();
	return true;
}

bool Process::getProcessId(DWORD& pId)
{
	threadMutex.lock();
	if (procStatus == ACTIVE)
		pId = procInfo.dwProcessId;
	else
	{
		threadMutex.unlock();
		return false;
	}
	threadMutex.unlock();
	return true;
}

bool Process::getThreadHandle(HANDLE tHandle)
{
	threadMutex.lock();
	if (procStatus == ACTIVE)
		tHandle = procInfo.hThread;
	else
	{
		threadMutex.unlock();
		return false;
	}
	threadMutex.unlock();
	return true;
}

bool Process::getThreadID(DWORD& tId)
{
	threadMutex.lock();
	if (procStatus == ACTIVE)
		tId = procInfo.dwThreadId;
	else
	{
		threadMutex.unlock();
		return false;
	}
	threadMutex.unlock();
	return true;
}

int Process::getStatus()
{
	threadMutex.lock();
	int status = procStatus;
	threadMutex.unlock();

	return status;
}

const wchar_t* Process::getCommandLineW() const
{
	return multToUni(commandLine.c_str());
}

const char* Process::getCommandLine() const
{
	return commandLine.c_str();
}

void Process::setOnProcStart(void(*func)())
{
	onProcStart = std::function<void()>(func);
}

void Process::setOnProcCrash(void(*func)())
{
	onProcCrash = std::function<void()>(func);
}

void Process::setOnProcExit(void(*func)())
{
	onProcExit = std::function<void()>(func);
}

void Process::setOnProcManuallyStopped(void(*func)())
{
	onProcManuallyStopped = std::function<void()>(func);
}

void Process::setOnProcSuspend(void(*func)())
{
	onProcSuspend = std::function<void()>(func);
}

void Process::setOnProcResume(void(*func)())
{
	onProcResume = std::function<void()>(func);
}

// The function interpretates a const multibyte string to unicode one
wchar_t* Process::multToUni(const char* cStr)
{
	size_t length = strlen(cStr) + 1;
	wchar_t* wStr = new wchar_t[length];
	size_t convertedChars = 0;
	mbstowcs_s(&convertedChars, wStr, length, cStr, _TRUNCATE);

	return wStr;
}

// The function interpretates a const unicode string to multibyte one
char* Process::uniToMult(const wchar_t* wStr)
{
	size_t length = wcslen(wStr) + 1;
	char* cStr = new char[length];
	size_t convertedChars = 0;
	wcstombs_s(&convertedChars, cStr, length, wStr, _TRUNCATE);

	return cStr;
}

// The function shows a list of current open processes.
//
//bool Process::GetProcessList()
//{
//	HANDLE hProcessSnap;
//	HANDLE hProcess;
//	PROCESSENTRY32 pe32;
//
//	// Take a snapshot of all processes in the system.
//	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
//
//	if (hProcessSnap == INVALID_HANDLE_VALUE)
//	{
//		return false;
//	}
//
//	// Set the size of the structure before using it.
//	pe32.dwSize = sizeof(PROCESSENTRY32);
//
//	// Retrieve information about the first process,
//	// and exit if unsuccessful
//	if (!Process32First(hProcessSnap, &pe32))
//	{
//		CloseHandle(hProcessSnap);          // clean the snapshot object
//	}
//
//	do
//	{
//		hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
//		//_tprintf(TEXT("\nPROCESS NAME:  %i	%s"), pe32.th32ProcessID, pe32.szExeFile);
//		CloseHandle(hProcess);
//	} while (Process32Next(hProcessSnap, &pe32));
//
//	std::cout << GetCurrentProcessId() << std::endl;
//	CloseHandle(hProcessSnap);
//	return(TRUE);
//}