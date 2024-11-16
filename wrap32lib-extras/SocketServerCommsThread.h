#pragma once

#include <wrap32lib.h>
#include <Thread.h>
#include <NotifyTarget.h>
#include <Logger.h>
#include <packet.h>
#include <Socket.h>

#include "Comms.h"

#include <sstream>

class CommsParser
{
public:
	virtual bool OnReceive(BYTE* p, int len)	{	return false;	}
	virtual int GetReply(BYTE* p)				{	return 0;		}
};

class SocketCommsThread : public Thread
{
public:
	SocketCommsThread(Socket* s, CommsParser* pParser, Event& evQuit, NotifyTarget* pNotifier, Logger& logger) :
		m_s(s), m_pParser(pParser), m_evQuit(evQuit), m_pNotifier(pNotifier), m_logger(logger)
	{
	}

private:
	void Log(logPriority prio, LPCWSTR fmt, ...) {
		std::wostringstream wss;
		wss << std::hex << (__int64)this << L" DST";
		va_list args;
		va_start(args, fmt);
		m_logger.LogVA(wss.str().c_str(), prio, fmt, args);
		va_end(args);
	}

	void ThreadProc() {
		Log(info, L"Session Starting");

		Event evRead;
		//		Event evTimer;
		HANDLE handles[] = { evRead, m_evStop };
		m_s->AssignEvent(evRead, FD_READ | FD_CLOSE);

		//		TimerQueue tq;
		//		auto timer = tq.CreateTimer([&evTimer](LPVOID, BOOLEAN) { evTimer.Set(); return false;  }, NULL);
		//		timer.Start(SERVER_UPDATE_TIMER_MS, SERVER_UPDATE_TIMER_MS);

		BYTE buff[1024];

		WSANETWORKEVENTS NetworkEvents;
		BOOL bQuit = FALSE;
		while (!bQuit) {
			switch (::WaitForMultipleObjects(sizeof(handles) / sizeof(HANDLE), handles, FALSE, INFINITE)) {
			case WAIT_OBJECT_0:	// read cmd
				::WSAEnumNetworkEvents(m_s->GetSocket(), evRead, &NetworkEvents);
				if (NetworkEvents.lNetworkEvents & FD_READ) {
					int len = m_s->Receive(buff, 1024, false);
					if (len > 0) {
						if (m_pParser->OnReceive(buff, len)) {
							len = m_pParser->GetReply(buff);
							if (len > 0) {
								m_s->Send(buff, len);
							}
							else {
								Log(error, L"Bad message");
							}
						}
						else {
							Log(error, L"Bad message");
						}
					}
				}

				if (NetworkEvents.lNetworkEvents & FD_CLOSE)
					bQuit = TRUE;
				break;

			case WAIT_OBJECT_0 + 1: {	// evQuit
				bQuit = TRUE;
				break;
			}

			default: {	// timeout
				break;
			}
			}
		}

		m_evQuit.Set();
		Log(info, L"Session ending");
	}

protected:
	CommsParser* m_pParser;

	Event& m_evQuit;
	Socket* m_s;

	NotifyTarget* m_pNotifier;
	Logger& m_logger;
};

class SocketServerCommsThread : public SocketServer, public Thread
{
public:
	SocketServerCommsThread(int connections, CommsParser* pParser, Logger& logger) : m_logger(logger), m_pParser(pParser), m_pNotifier(NULL), SocketServer(connections) {}
	~SocketServerCommsThread() {
		Stop();
	}

	DWORD Start(NotifyTarget* pNotifier = NULL) {
		m_pNotifier = pNotifier;
		DWORD dw = SocketServer::Start();
		if (dw != ERROR_SUCCESS)
			return dw;

		Thread::Start();
		return ERROR_SUCCESS;
	}

	void Stop() {
		Thread::Stop();
	}

private:
	void ThreadProc() {
		Event evAccept;
		Event evThreadHasQuit;
		AssignEvent(evAccept, FD_ACCEPT);
		HANDLE handles[] = { evAccept, evThreadHasQuit, m_evStop };

		BOOL bQuit = FALSE;
		while (!bQuit) {
			switch (::WaitForMultipleObjects(sizeof(handles) / sizeof(HANDLE), handles, FALSE, INFINITE)) {
			case WAIT_OBJECT_0: {
				Socket* pSocket;
				if (Accept(&pSocket)) {
					SocketCommsThread* pSCT = new SocketCommsThread(pSocket, m_pParser, evThreadHasQuit, m_pNotifier, m_logger);
					m_threads.push_back(pSCT);
					pSCT->Start();
				}
				break;
			}

			case WAIT_OBJECT_0 + 1: {
				// Clean up the terminated thread
				m_threads.erase(std::remove_if(m_threads.begin(), m_threads.end(), [](SocketCommsThread* p) {
					if (p->Running())
						return false;
					delete p;
					return true;
					}), m_threads.end());
				break;
			}
			default: {
				for (auto thread : m_threads)
					thread->Stop();
				bQuit = TRUE;
				break;
			}
			}
		}
	}

protected:
	CommsParser* m_pParser;
	std::vector<SocketCommsThread*> m_threads;
	NotifyTarget* m_pNotifier;
	Logger& m_logger;
};
