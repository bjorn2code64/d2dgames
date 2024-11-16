#include "MasterCommsThreadSer.h"
#include <unicodemultibyte.h>

void MasterCommsThreadSer::ThreadProc()
{
	DWORD dwTimeout = INFINITE;
	int timeoutCount = 0;

	bool quit = false;
	while (!quit) {
		if (!IsOpen()) {
			m_logger.Log(info, L"Connecting to com port %d", m_nPort);
			if (!Open(true)) {	// open overlapped
				m_logger.Log(warning, L"Failed to open Com port %d", m_nPort);
				Sleep(5000);
				continue;
			}

			Configure(m_baudrate, m_databits, NOPARITY, ONESTOPBIT);
			SetReadTimeout(0, 500);
		}

		// Try and send the first poll
		if (BuildNextPoll(ODR_FIRSTPOLL, m_packetTX)) {
			m_logger.Log(info, L"Sending first poll");
			SendPacket();
		}
		else {
			m_logger.Log(info, L"No polls to send.");
			continue;
		}

		HANDLE handles[] = { m_evTX, m_evRX, m_evStop };
		bool restart = false;
		int odr_status = ODR_UNDEFINED;
		while (!quit && !restart) {
			switch (::WSAWaitForMultipleEvents(sizeof(handles) / sizeof(HANDLE), handles, FALSE, dwTimeout, FALSE)) {
			case WAIT_OBJECT_0: {	// TX complete
				// Did the write work?
				DWORD bytes;
				if (!::GetOverlappedResult(m_hComm, &m_ov, &bytes, TRUE)) {
					OnError(true);
					m_logger.Log(error, L"GetOverlappedResult (TX) failed");
					restart = true;
					break;
				}

				if (bytes != (DWORD)m_packetTX.Size()) {
					OnError(true);
					m_logger.Log(error, L"Expected to write %d. Wrote %d", m_packetTX.Size(), bytes);
					restart = true;
					break;
				}

				// Clear out the write data
				m_packetTX.Clear();

				// Expect a reply
				m_ov.hEvent = m_evRX;
				m_expected = GetExpectedReplyLength();
				if (m_expected > 0) {
					// If we expect a fixed length reply, allocate and wait for it
					m_packetRX.Allocate(m_expected);
					Receive(m_packetRX, m_expected, &m_ov);
				}
				else {
					// We'll keep getting chars until there's a gap
					m_packetRX.Allocate(1024);
					SetReadTimeout(0, 50);			// inter char gap means message ends
					Receive(m_packetRX, 1, &m_ov);	// Wait for the first char
				}
				break;
			}

			case WAIT_OBJECT_0 + 1: {	// RX something
				// Did we read ok
				DWORD bytes;
				if (!::GetOverlappedResult(m_hComm, &m_ov, &bytes, TRUE)) {
					m_logger.Log(error, L"GetOverlappedResult (RX) failed");
					restart = true;
					break;
				}

				odr_status = ODR_UNDEFINED;
				if (m_expected > 0) {
					// Were we expecting a fixed length reply? If so, did we get it all?
					if (bytes != (DWORD)m_expected) {
						m_logger.Log(error, L"Expected %d bytes. Got %d", m_expected, bytes);
						if (bytes == 0) {// we didn't get anything
							odr_status = OnTimeout(++timeoutCount);
						}
					}
				}
				else {
					// We're read chars one at a time until we timeout
					if (bytes == 0) {		// We didn't get anything so we're either done or we timed out
						if (m_packetRX.Size() == 0) {
							// Nothing back at all :( We can call this a reply timeout
							odr_status = OnTimeout(++timeoutCount);
						}
					}
					else {
						Receive(m_packetRX.End(), 1, &m_ov);	// Wait for the next char
						break;
					}
				}

				if (odr_status == ODR_UNDEFINED)
					odr_status = OnDataReady(m_packetRX);

				switch (odr_status) {
				case ODR_FIRSTPOLL:
				case ODR_NEXTPOLL:
					timeoutCount = 0;
					OnError(false);
				case ODR_RETRY:
					Flush();
					if (BuildNextPoll(odr_status, m_packetTX)) {
						m_logger.Log(info, L"Sending next poll");
						SendPacket();
					}
					break;

				case ODR_DISCONNECT:
				default:
					restart = true;
					break;
				}
				break;
			}
			case WAIT_OBJECT_0 + 2: {// quit
				quit = true;
				break;
			}
			}
		}
	}

	m_logger.Log(info, L"Terminating");
}
