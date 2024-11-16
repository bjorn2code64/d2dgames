#pragma once

#include <wrap32lib.h>
#include <AppMessage.h>
#include <Window.h>
#include "Notifier.h"

#include <Ddeml.h>

class DDEServer
{
public:
	DDEServer();
	~DDEServer();

	void InitServerName(LPCWSTR szAppName);
	void SetTopicName(LPCWSTR szTopic);

	HDDEDATA DdeCallback(UINT uType, UINT uFmt, HCONV hConv, HSZ hszTopic, HSZ hszItem, HDDEDATA hdata, DWORD dwData1, DWORD dwData2);

	void OnValueChanged(LPCWSTR szItem);

	BOOL WMIsValidRequest(UINT message) { return message == m_msgRequestValue; }
	BOOL WMIsValidForget(UINT message) { return message == m_msgForgetValue; }
	BOOL WMIsValidPoke(UINT message) { return message == m_msgPokeValue; }
	BOOL WMIsValidDisconnect(UINT message) { return message == m_msgAllDisconnect; }

protected:
	virtual LPCWSTR OnRequestDataItemValue(LPCWSTR szItem) { return L"?"; }
	virtual void OnForgetDataItem(LPCWSTR szItem) {}
	virtual BOOL PokeItem(LPCWSTR szItem, LPCWSTR szValue) { return FALSE;  }
	virtual void OnAllClientsDisconnected() {}

protected:
	DWORD m_idInst;
	HSZ m_hszAppName;
	HSZ m_hszTopic;

	int m_nConnections;

	AppMessage m_msgAllDisconnect;
	AppMessage m_msgRequestValue;
	AppMessage m_msgForgetValue;
	AppMessage m_msgPokeValue;
};

