#pragma once

#include <wrap32lib.h>
#include <Thread.h>
#include <Logger.h>
#include <SerialCom.h>
#include <packet.h>

#define ODR_UNDEFINED	-1	// not used
#define ODR_OK			0
#define ODR_DISCONNECT	1
#define ODR_NEXTPOLL	2
#define ODR_RETRY		3
#define ODR_FIRSTPOLL	4

class MasterCommsThreadSer : public Serial, public Thread
{
public:
	MasterCommsThreadSer(Logger& logger) :
		m_logger(logger),
		m_expected(-1)	// disabled if -1, 0 unsupported for now
	{
		ZeroMemory(&m_ov, sizeof(m_ov));
	}

protected:
	virtual int OnDataReady(Packet& packet) { return ODR_OK;  }
	// Called when a packet is received. Should return an ODR_ code.
	
	virtual int OnTimeout(int count) { return (count == 3) ? ODR_DISCONNECT : ODR_RETRY;  }
	// Called when an expected reply is not received in time. Should return an ODR_ code.

	virtual void OnError(bool error) {}
	// Called when an expected reply is not received in time.

	virtual bool BuildNextPoll(int odr_ret, Packet& packet) { return false;  }
	// Should build the next poll in packet. Return false if no poll to build.

	virtual int GetExpectedReplyLength() { return -1;  }
	// Get the number of chars in a valid reply - set to "intermessage gap"

	void ThreadProc();

	bool SendPacket() {
		m_ov.hEvent = m_evTX;
		DWORD written;
		::WriteFile(m_hComm, m_packetTX, (DWORD)m_packetTX.Size(), &written, &m_ov);
		// don't clear packet until write is complete. This is asynch
		return true;
	}

protected:
	OVERLAPPED m_ov;
	Event m_evCheck;
	Event m_evTX;
	Event m_evRX;
	Logger& m_logger;
	Packet m_packetTX;
	Packet m_packetRX;
	int m_expected;
	DWORD m_baudrate;
	BYTE m_databits;
};
