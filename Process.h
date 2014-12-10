#ifndef PROCESS_H
#define PROCESS_H

#include "Logger.h"
#include "Exceptions.h"
#include <Windows.h>
#include <thread>
#include <mutex>
#include <string>

enum PROCESS_STATUS{ ACTIVE, RESTARTING, SUSPENDED, STOPPED };
const DWORD STOP_VIA_METHOD_CALL = 0x4D5A;

class Process
{
public:
	/**
	The consructor accepts 2 const unicode strings and a bool-value which determines stopping process when the instance
	is over (true by default). Also it accepts pointers to callback-functions(empty by default).
	Note: you can also pass args in the first argument.
	*/
	Process(const wchar_t* cmdLine,
		const wchar_t* argums = L"",
		bool stopWhenDestruct = true,
		void(*onProcStartFunc)() = [](void){},
		void(*onProcCrashFunc)() = [](void){},
		void(*onProcExitFunc)() = [](void){},
		void(*onProcManuallyStoppedFunc)() = [](void){},
		void(*onProcSuspendFunc)() = [](void){},
		void(*onProcResumeFunc)() = [](void){}
		);

	/**
	The consructor accepts 2 const multibyte strings () and a bool-value which determines stopping process when the instance
	is over (true by default). Also it accepts pointers to callback-functions(empty by default).
	*/
	Process(const char* cmdLine,
		const char* argums = "",
		bool stopWhenDestruct = true,
		void(*onProcStartFunc)() = [](void){},
		void(*onProcCrashFunc)() = [](void){},
		void(*onProcExitFunc)() = [](void){},
		void(*onProcManuallyStoppedFunc)() = [](void){},
		void(*onProcSuspendFunc)() = [](void){},
		void(*onProcResumeFunc)() = [](void){}
	);

	/**
	The constructor accepts process id and bool-value which determines stopping process when the instance
	is over (false by default). Also it accepts pointers to callback-functions(empty by default).
	*/
	Process(DWORD pId,
		bool stopWhenDestruct = true,
		void(*onProcStartFunc)() = [](void){},
		void(*onProcCrashFunc)() = [](void){},
		void(*onProcExitFunc)() = [](void){},
		void(*onProcManuallyStoppedFunc)() = [](void){},
		void(*onProcSuspendFunc)() = [](void){},
		void(*onProcResumeFunc)() = [](void){}
	);

	/**
	The destructor releases the resources, closes watchThread and stops the process if stopWhenDestruct is true.
	*/
	~Process();

	/**
	Next function tests the Process class. Call it, if you prefer to watch an example of using this class.
	*/
	static void testClassFunction();

	/**
	The function freezes execution of the process (if it is active) until it will be resumed.
	It returns true if succeed and false otherwise.
	*/
	bool suspendProcess();

	/**
	The function resumes the process if it is suspended.
	It returns true if succeed and false otherwise.
	*/
	bool resumeProcess();

	/**
	The function restarts the process if it is still active.
	It returns true if succeed and false otherwise.
	*/
	bool restartProcess();

	/**
	The function stops the process (if it is active) until it will be started again.
	It returns true if succeed and false otherwise.
	*/
	bool stopProcess();

	/**
	The function starts again the process if it was stopped by previous method.
	It returns true if succeed and false otherwise.
	*/
	bool startProcess();

	/**
	The function puts the process handle in pHandle if the process is active.
	It returns true if succeed and false otherwise.
	*/
	bool getProcessHandle(HANDLE pHandle);

	/**
	The function puts the process id in pId if the process is active.
	It returns true if succeed and false otherwise.
	*/
	bool getProcessId(DWORD& pId);

	/**
	The function puts the thread handle in tHandle if the process is active.
	It returns true if succeed and false otherwise.
	*/
	bool getThreadHandle(HANDLE tHandle);

	/**
	The function puts the thread id in tId if the process is active.
	It returns true if succeed and false otherwise.
	*/
	bool getThreadID(DWORD& tId);
	
	/**
	The function returns the status of the process:
	0 - ACTIVE
	1 - RESTARTING
	2 - SUSPENDED
	3 - STOPPED
	*/
	int getStatus();

	/**
	The function returns the command line of the process in the form of unicode.
	*/
	const wchar_t* getCommandLineW() const;

	/**
	The function returns the command line of the process in the form of multibyte.
	*/
	const char* getCommandLine() const;

	/**
	Those functions set the callbacks on main events.
	*/
	void setOnProcStart(void (*func)());
	void setOnProcCrash(void(*func)());
	void setOnProcExit(void(*func)());
	void setOnProcManuallyStopped(void(*func)());
	void setOnProcSuspend(void(*func)());
	void setOnProcResume(void(*func)());

	///static bool GetProcessList();

protected:
private:
	_STARTUPINFOW sInfo;
	_PROCESS_INFORMATION procInfo;
	std::mutex threadMutex;
	std::thread watchThread;
	bool exitThread;
	bool stopWhenDestr;
	int procStatus;
	std::string commandLine;
	std::function<void()> onProcStart;
	std::function<void()> onProcCrash;
	std::function<void()> onProcExit;
	std::function<void()> onProcManuallyStopped;
	std::function<void()> onProcSuspend;
	std::function<void()> onProcResume;
	Logger log;

	static wchar_t* multToUni(const char*);
	static char* uniToMult(const wchar_t*);
	bool createProc();
	void getCmd(DWORD pId);
	void initService(const wchar_t*, const wchar_t*, bool);
	static void watchThreadFunction(Process& proc);
};

#endif