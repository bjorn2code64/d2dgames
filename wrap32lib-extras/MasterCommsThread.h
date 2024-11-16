#pragma once

#include <wrap32lib.h>
#include <Thread.h>
#include <Logger.h>

#include "Socket.h"
#include "packet.h"

#define ODR_UNDEFINED	-1	// not used
#define ODR_OK			0
#define ODR_DISCONNECT	1
#define ODR_NEXTPOLL	2
#define ODR_RETRY		3
#define ODR_FIRSTPOLL	4

class MasterCommsThread : public SocketClient, public Thread
{
public:
	MasterCommsThread(Logger& logger) :
		m_port(0),
		m_logger(logger),
		m_dwReplyTimeout(INFINITE)
	{}

	LPCSTR GetHost()	{	return m_host.c_str();	}
	uint16_t GetPort()	{	return m_port;	}

	bool SetHost(LPCSTR sz)
	{
		if (!m_host.compare(sz))
			return false;
		m_host = sz;
		return true;
	}

	bool SetPort(uint16_t port)
	{
		if (m_port == port)
			return false;
		m_port = port;
		return true;
	}

protected:
	virtual int OnDataReady() { return ODR_OK;  }
	// Called when a packet is received. Should return an ODR_ code.
	
	virtual void OnConnect(bool connected) {}
	// Called when the connection is made, a connection attempt fails and the existing connection drops.

	virtual int OnTimeout(int count) { return ODR_RETRY;  }
	// Called when an expected reply is not received in time. Should return an ODR_ code.

	virtual bool BuildNextPoll(int odr_ret, Packet& packet) { return false;  }
	// Should build the next poll in packet. Return false if no poll to build.

	void ThreadProc();

	void TxReady()		{	m_evReadyToTX.Set();	}

protected:
	std::string m_host;
	uint16_t m_port;
	Event m_evReadyToTX;
	Logger& m_logger;
	Packet m_packetTX;
	DWORD m_dwReplyTimeout;
};
