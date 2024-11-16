#include "MasterCommsThread.h"
#include <unicodemultibyte.h>

void MasterCommsThread::ThreadProc()
{
	DWORD dwTimeout = INFINITE;
	int timeoutCount = 0;
	Event evNetworkEvent;

	bool quit = false;
	while (!quit) {
		m_logger.Log(info, L"Connecting to %s", LPCWSTR(UnicodeMultibyte(m_host)));
		if (ConnectNonBlocking(m_host.c_str(), m_port, evNetworkEvent, FD_CONNECT | FD_READ | FD_CLOSE) != ERROR_SUCCESS)
			return;

		HANDLE handles[] = { evNetworkEvent, m_evReadyToTX, m_evStop };
		bool bDisconnected = false;
		int odr_status = ODR_UNDEFINED;
		while (!quit && !bDisconnected) {
			switch (::WSAWaitForMultipleEvents(sizeof(handles) / sizeof(HANDLE), handles, FALSE, dwTimeout, FALSE)) {
			case WAIT_OBJECT_0: {	// connect, read, disconnect
				WSANETWORKEVENTS NetworkEvents;
				::WSAEnumNetworkEvents(m_s, evNetworkEvent, &NetworkEvents);
				if (NetworkEvents.lNetworkEvents & FD_CONNECT) {
					if (NetworkEvents.iErrorCode[FD_CONNECT_BIT] == 0) {
						m_logger.Log(info, L"Connected.");
						OnConnect(true);
						if (BuildNextPoll(ODR_FIRSTPOLL, m_packetTX))
							m_evReadyToTX.Set();
					}
					else {
						m_logger.Log(info, L"Connection Failed");
						// int x = NetworkEvents.iErrorCode[FD_CONNECT_BIT];
						// connection failed?
						bDisconnected = true;
						OnConnect(false);
					}
				}

				if (NetworkEvents.lNetworkEvents & FD_READ) {
					timeoutCount = 0;
					dwTimeout = INFINITE;
					odr_status = OnDataReady();
				}

				if (NetworkEvents.lNetworkEvents & FD_CLOSE) {
					int error = NetworkEvents.iErrorCode[FD_CLOSE_BIT];
					dwTimeout = INFINITE;
					Close();
					OnConnect(false);
					m_logger.Log(info, L"Remote disconnect (%d)", error);
					bDisconnected = true;
				}
				break;
			}

			case WAIT_OBJECT_0 + 1: {	// Ready to TX
				int size = (int)m_packetTX.Size();
				if (Send(m_packetTX, size) != size) {
					m_logger.Log(error, L"Write fail");
				}
				dwTimeout = m_dwReplyTimeout;
				m_packetTX.Clear();
				break;
			}

			case WAIT_OBJECT_0 + 2: {	// Quit
				quit = true;
				break;
			}

			default: {					// Timeout
				m_logger.Log(info, L"Timeout");
				odr_status = OnTimeout(++timeoutCount);
				break;
			}
			}

			if (odr_status != ODR_UNDEFINED) {
				switch (odr_status) {
				default:
				case ODR_OK:
					break;
				case ODR_DISCONNECT:
					dwTimeout = INFINITE;
					Close();
					OnConnect(false);
					m_logger.Log(info, L"ODR_DISCONNECT");
					bDisconnected = true;
					break;
				case ODR_FIRSTPOLL:
				case ODR_NEXTPOLL:
				case ODR_RETRY:
					Flush();
					if (BuildNextPoll(odr_status, m_packetTX))
						m_evReadyToTX.Set();
					break;
				}
				odr_status = ODR_UNDEFINED;
			}
		}
	}

	OnConnect(false);
	m_logger.Log(info, L"Terminating");
}