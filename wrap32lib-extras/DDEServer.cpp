#include "DDEServer.h"

static DDEServer* l_pDDE = NULL;

#ifdef _DEBUG
#define GDDEFULLDEBUG
#endif

static int l_idInst = 0;

static HDDEDATA CALLBACK l_DdeCallback(UINT uType, UINT uFmt, HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA hdata, DWORD dwData1, DWORD dwData2)
{
#ifdef GDDEFULLDEBUG
	const wchar_t* buffType = L"unknown";
	if (uType == XTYP_REGISTER)
		buffType = L"XTYP_REGISTER";
	else if (uType == XTYP_UNREGISTER)
		buffType = L"XTYP_UNREGISTER";
	else if (uType == XTYP_CONNECT_CONFIRM)
		buffType = L"XTYP_CONNECT_CONFIRM";
	else if (uType == XTYP_DISCONNECT)
		buffType = L"XTYP_DISCONNECT";
	else if (uType == XTYP_CONNECT)
		buffType = L"XTYP_CONNECT";
	else if (uType == XTYP_ADVSTART)
		buffType = L"XTYP_ADVSTART";
	else if (uType == XTYP_ADVREQ)
		buffType = L"XTYP_ADVREQ";
	else if (uType == XTYP_REQUEST)
		buffType = L"XTYP_REQUEST";
	else if (uType == XTYP_ADVREQ)
		buffType = L"XTYP_ADVREQ";
	else if (uType == XTYP_ADVSTOP)
		buffType = L"XTYP_ADVSTOP";
	else if (uType == XTYP_POKE)
		buffType = L"XTYP_POKE";
	else if (uType == XTYP_XACT_COMPLETE)
		buffType = L"XTYP_XACT_COMPLETE";

	const wchar_t* szFormat = (uFmt == CF_TEXT) ? L"TEXT" : L"unknown";

	wchar_t buff1[256];	// Get The Item Name
	DdeQueryString(l_idInst, hsz1, buff1, 255, CP_WINNEUTRAL);

	wchar_t buff2[256];	// Get The Item Name
	DdeQueryString(l_idInst, hsz2, buff2, 255, CP_WINNEUTRAL);

	wchar_t buff[1024];
	_snwprintf_s(buff, 1024, L"DDECallback %s %s %s %s\n", buffType, szFormat, buff1, buff2);
	buff[1023] = '\0';
	OutputDebugString(buff);
#endif
	return l_pDDE ? l_pDDE->DdeCallback(uType, uFmt, hconv, hsz1, hsz2, hdata, dwData1, dwData2) : (HDDEDATA)NULL;
}

DDEServer::DDEServer() :
	m_idInst(0),
	m_nConnections(0),
	m_hszAppName(NULL),
	m_hszTopic(NULL)
{
	l_pDDE = this;	// for the callback
	DdeInitialize(&m_idInst,		 // receives instance identifier 
		(PFNCALLBACK)::l_DdeCallback, // pointer to callback function 
		APPCMD_FILTERINITS,
		0);
	l_idInst = m_idInst;
}

DDEServer::~DDEServer()
{
	DdeNameService(m_idInst, 0, 0, DNS_UNREGISTER);
	DdeFreeStringHandle(m_idInst, m_hszAppName);
	DdeFreeStringHandle(m_idInst, m_hszTopic);
	DdeUninitialize(m_idInst);
}

void DDEServer::InitServerName(LPCWSTR szAppName)
{
	// Register this application name
	m_hszAppName = DdeCreateStringHandle(m_idInst, szAppName, CP_WINNEUTRAL);
	DdeNameService(m_idInst, m_hszAppName, 0, DNS_REGISTER);
}

void DDEServer::SetTopicName(LPCWSTR szTopic)
{
	// Register this application name
	if (m_hszTopic)		DdeFreeStringHandle(m_idInst, m_hszTopic);
	m_hszTopic = DdeCreateStringHandle(m_idInst, szTopic, CP_WINNEUTRAL);
}

HDDEDATA DDEServer::DdeCallback(UINT uType, UINT uFmt, HCONV hConv, HSZ hszTopic, HSZ hszItem, HDDEDATA hData, DWORD /*dwData1*/, DWORD /*dwData2*/)
{
	switch (uType)
	{
	case XTYP_REGISTER:
	case XTYP_UNREGISTER:
		return (HDDEDATA)NULL;

	case XTYP_CONNECT_CONFIRM:
		m_nConnections++;
		return (HDDEDATA)NULL;

	case XTYP_DISCONNECT:
		if (m_nConnections > 0)
		{
			m_nConnections--;
			if (!m_nConnections)
				OnAllClientsDisconnected();
		}

		return (HDDEDATA)NULL;

	default:
		break;
	}

#ifdef _UNICODE
	if (uFmt && (uFmt != CF_UNICODETEXT))
#else
	if (uFmt && (uFmt != CF_TEXT))
#endif
		return (HDDEDATA)NULL;						// Text only at the moment

	if (DdeCmpStringHandles(hszTopic, m_hszTopic))	// is it our topic?
	{
		// Support "system" in here
		return (HDDEDATA)NULL;						// not for us
	}

	if (uType == XTYP_CONNECT)						// Approve the connection
		return (HDDEDATA)TRUE;

	if (!DdeImpersonateClient(hConv))
		return (HDDEDATA)NULL;		// fail

	TCHAR buffItem[256];	// Get The Item Name
	DdeQueryString(m_idInst, hszItem, buffItem, 255, CP_WINNEUTRAL);

	switch (uType)
	{
	case XTYP_ADVSTART:
		return (OnRequestDataItemValue(buffItem) != NULL) ? (HDDEDATA)TRUE : (HDDEDATA)FALSE;

	case XTYP_ADVREQ:	// Somewhere else in the server has said that this data has changed
	case XTYP_REQUEST:	// A client request. Only supported if an advise loop is already running.
	{
		LPCTSTR pData = OnRequestDataItemValue(buffItem);

		DWORD dwSize = 0;
		if (pData)
			dwSize = ((DWORD)wcslen(pData) + 1) * sizeof(wchar_t);

		return DdeCreateDataHandle(m_idInst, (unsigned char*)pData, dwSize, 0, hszItem, uFmt, 0);
	}

	case XTYP_ADVSTOP:
		OnForgetDataItem(buffItem);
		break;

	case XTYP_POKE:
	{
		DWORD dwSize = DdeGetData(hData, NULL, 0, 0);
		if (dwSize)
		{
			BYTE* p = new BYTE[dwSize];
			DdeGetData(hData, p, dwSize, 0);

			if (PokeItem(buffItem, (LPCTSTR)p))
			{
				delete[] p;
				OnValueChanged(buffItem);
				return (HDDEDATA)DDE_FACK;	// we dealt with it
			}
			delete[] p;
		}

		return DDE_FNOTPROCESSED;	// we didn't deal with it
	}
	break;

	case XTYP_REGISTER:
	case XTYP_UNREGISTER:
		return (HDDEDATA)NULL;

	case XTYP_ADVDATA:
		return (HDDEDATA)DDE_FACK;

	case XTYP_XACT_COMPLETE:
		return (HDDEDATA)NULL;
	}

	RevertToSelf();
	return (HDDEDATA)NULL;
}

void DDEServer::OnValueChanged(LPCTSTR szItem)
{
	HSZ hszItem = DdeCreateStringHandle(m_idInst, szItem, CP_WINNEUTRAL);
	DdePostAdvise(m_idInst, m_hszTopic, hszItem);
	DdeFreeStringHandle(m_idInst, hszItem);
}
