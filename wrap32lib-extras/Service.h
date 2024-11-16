#pragma once

#include "wrap32lib.h"
#include "event.h"
#include "Logger.h"

#include <string>
#include <functional>

//////////////////////////////////////////////////////////////////////

	class SCManager
	{
	public:
		SCManager() : m_h(NULL)	{}
		~SCManager()	{	Close();	}

		bool Open(std::wstring& strError) {
			Close();
			m_h = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (!m_h) {
				if (ERROR_ACCESS_DENIED == GetLastError())
					strError = L"OpenSCManager failed. Please run as administrator";
				else
					strError = L"OpenSCManager failed";

				return false;
			}

			return true;
		}

		void Close() {
			if (m_h) {
				::CloseServiceHandle(m_h);
				m_h = NULL;
			}
		}

		operator SC_HANDLE()			{	return m_h;	}

	private:
		SC_HANDLE m_h;
	};

class SCService
{
public:
	SCService(SCManager& scm) : m_scm(scm), m_h(NULL) {}
	~SCService() { Close(); }

	bool Connect(LPCWSTR name, std::wstring& strError) {
		Close();
		m_h = OpenService(m_scm, (LPWSTR)name, SERVICE_ALL_ACCESS);

		if (!m_h) {
			if (ERROR_ACCESS_DENIED == GetLastError())
				strError = L"OpenSCManager failed. Please run as administrator";
			else
				strError = L"OpenSCManager failed";

			return false;
		}

		return true;
	}

	bool Running() {
		if (!m_h) {
			return false;
		}

		SERVICE_STATUS_PROCESS ssp;
		DWORD dwBytesNeeded;

		if (!QueryServiceStatusEx(
			m_h,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded)) {
			return false;
		}

		return ssp.dwCurrentState != SERVICE_STOPPED;
	}

	bool Start(std::wstring& strError) {
		if (!m_h) {
			strError = L"Not connected";
			return false;
		}

		if (!StartService(
			m_h,  // handle to service 
			0,    // number of arguments 
			NULL)) {
			strError = L"Start failed";
			return false;
		}
		return true;
	}

	bool Stop() {
		if (!m_h) {
			return false;
		}

		SERVICE_STATUS_PROCESS ssp;
		return ControlService(
			m_h,
			SERVICE_CONTROL_STOP,
			(LPSERVICE_STATUS)&ssp);
	}

	void Close() {
		if (m_h) {
			::CloseServiceHandle(m_h);
			m_h = NULL;
		}
	}
private:
	SCManager& m_scm;
	SC_HANDLE m_h;
};

class Service
{
public:
	static bool Install(const std::wstring& strName, const std::wstring& strDesc, std::wstring& strError);
	static bool Uninstall(const std::wstring& strName, std::wstring& strError);
	DWORD Run(const std::wstring& strName, DWORD dwMaxWaitTime);
	DWORD RunAsApp(const std::wstring& strName, DWORD dwMaxWaitTime);

	static void ReportStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

	static void StopService() { m_evStop.Set(); }
public:
	std::wstring m_strError;
	static Logger* m_logger;

protected:
	static std::function<void(void)> m_init;
	static std::function<void(Event&)> m_main;

	Service() {}
	~Service()	{ StopService();	}

private:
	// The service callback functions
	static void CALLBACK SvcMain(DWORD, LPTSTR *);
	static void CALLBACK SvcCtrlHandler(DWORD);

private:
	// Data for the callback functions - must be static
	static std::wstring m_strName;
	static SERVICE_STATUS m_SvcStatus;
	static SERVICE_STATUS_HANDLE m_SvcStatusHandle;
	static Event m_evStop;
//	static Event m_evQuit;
	static int m_dwMaxWaitTimeMS;
};
