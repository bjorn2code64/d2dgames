
#include "Service.h"

#include <winsvc.h>

///////////////////////////////////////////////////////////////////////

Event Service::m_evStop(true);				// Manual reset event to stop the service
//Event Service::m_evQuit(true);				// Manual reset event indicating the console service has quit
std::wstring Service::m_strName;						// The name of the service
SERVICE_STATUS Service::m_SvcStatus;					// The status of the service
SERVICE_STATUS_HANDLE Service::m_SvcStatusHandle;		// A handle to the service control handler
std::function<void(void)> Service::m_init;
std::function<void(Event&)> Service::m_main;
int Service::m_dwMaxWaitTimeMS = 5000;		// FuncMain should report running status in intervals less than this time (in ms)
Logger* Service::m_logger = NULL;

///////////////////////////////////////////////////////////////////////

bool Service::Install(const std::wstring& strName, const std::wstring& strDesc, std::wstring& strError)
{
	m_strName = strName;

	TCHAR szPath[MAX_PATH + 1];
	if (!GetModuleFileName(NULL, szPath, MAX_PATH))
	{
		strError = L"Cannot install service (GetModuleFileName)";
		return false;
	}

	SCManager scm;
	if (!scm.Open(strError)) {
		return false;
	}

	// Create the service
	SC_HANDLE schService = CreateService(
				scm,						// SCM database 
				m_strName.c_str(),			// name of service 
				m_strName.c_str(),			// service name to display 
				SERVICE_ALL_ACCESS,			// desired access 
				SERVICE_WIN32_OWN_PROCESS,	// service type 
				SERVICE_AUTO_START,			// start type 
				SERVICE_ERROR_NORMAL,		// error control type 
				szPath,						// path to service's binary 
				NULL,						// no load ordering group 
				NULL,						// no tag identifier 
				NULL,						// no dependencies 
				NULL,						// LocalSystem account 
				NULL);						// no password 

	if (!schService)
	{
		strError = L"CreateService failed";
		return false;
	}

	// Set the description
	SERVICE_DESCRIPTION sd = {	(LPWSTR)strDesc.c_str()	};
	if (!ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, (LPVOID)&sd)) {
		strError = L"ChangeServiceConfig2 failed";
		return false;
	}

	CloseServiceHandle(schService);
	return true;
}

bool Service::Uninstall(const std::wstring& strName, std::wstring& strError)
{
	SCManager scm;
	if (!scm.Open(strError))
		return false;

	// Get a handle to the service.
	SC_HANDLE schService = OpenService(
		scm,				// SCM database 
		strName.c_str(),    // name of service 
		DELETE);            // need delete access 

	if (!schService)
	{
		strError = L"OpenService failed";
		return false;
	}

	// Delete the service.
	if (!DeleteService(schService))
	{
		strError = L"DeleteService failed";
		CloseServiceHandle(schService);
		return false;
	}

	CloseServiceHandle(schService);
	return true;
}

DWORD Service::Run(const std::wstring& strName, DWORD dwMaxWaitTime)
{
	if (m_logger)
		m_logger->Log(debug, L"Service.Run");

	m_strName = strName;
	m_dwMaxWaitTimeMS = dwMaxWaitTime;

	// A list of all the services we provide. We only support
	// a single service in this class.
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{	(LPTSTR)m_strName.c_str(), (LPSERVICE_MAIN_FUNCTION)SvcMain},
		{	NULL,				NULL}
	};

	// This call returns when the service has stopped. 
	// The process should simply terminate when the call returns.
	if (!StartServiceCtrlDispatcher(DispatchTable))
		return GetLastError();

	return ERROR_SUCCESS;
}

DWORD Service::RunAsApp(const std::wstring& strName, DWORD dwMaxWaitTime)
{
	if (m_logger)
		m_logger->Log(critical, L"Service.Init");
	m_init();
	if (m_logger)
		m_logger->Log(critical, L"Service.Main");
	m_main(m_evStop);

	if (m_logger)
		m_logger->Log(critical, L"Main completed");

	return ERROR_SUCCESS;
}

void Service::SvcMain(DWORD, LPTSTR*)
{
	if (m_logger)
		m_logger->Log(debug, L"Service.SvcMain");

	// Register the handler function for the service
	m_SvcStatusHandle = RegisterServiceCtrlHandler(m_strName.c_str(), (LPHANDLER_FUNCTION)SvcCtrlHandler);

	if (!m_SvcStatusHandle) {
		if (m_logger)
			m_logger->Log(critical, L"RegisterServiceCtrlHandler failed");
		return;
	}

	// These SERVICE_STATUS members remain as set here
	m_SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	m_SvcStatus.dwServiceSpecificExitCode = 0;
	m_SvcStatus.dwCheckPoint = 0;	// init this to start with

	// Report initial status to the SCM
	ReportStatus(SERVICE_START_PENDING, NO_ERROR, m_dwMaxWaitTimeMS);	// 3 second max update/loop time

	if (m_logger)
		m_logger->Log(critical, L"Service.Init");
	m_init();

	// Report running status when initialization is complete.
	ReportStatus(SERVICE_RUNNING, NO_ERROR, 0);

	// Call main which should only return on error or when m_evStop is set
	// and should periodically call Service::ReportStatus(SERVICE_RUNNING...
	if (m_logger)
		m_logger->Log(critical, L"Service.Main");
	m_main(m_evStop);

	if (m_logger)
		m_logger->Log(critical, L"Main completed");
	ReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

void Service::ReportStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	// Fill in the SERVICE_STATUS structure.
	m_SvcStatus.dwCurrentState = dwCurrentState;
	m_SvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	m_SvcStatus.dwWaitHint = dwWaitHint;
	m_SvcStatus.dwControlsAccepted = (dwCurrentState == SERVICE_START_PENDING) ? 0 : SERVICE_ACCEPT_STOP;

	if ((dwCurrentState == SERVICE_PAUSED) || (dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
		m_SvcStatus.dwCheckPoint = 0;
	else
		m_SvcStatus.dwCheckPoint++;

	// Report the status of the service to the SCM.
	::SetServiceStatus(m_SvcStatusHandle, &m_SvcStatus);
}

void Service::SvcCtrlHandler(DWORD dwCtrl)
{
	if (dwCtrl == SERVICE_CONTROL_STOP)
	{
		ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
		m_evStop.Set();		// Signal the service to stop.
	}
}
