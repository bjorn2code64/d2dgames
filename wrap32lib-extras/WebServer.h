#pragma once

#include <Event.h>
#include <Thread.h>

#include <algorithm>

#include "Socket.h"
#include "packet.h"
#include "Notifier.h"

typedef std::function<void(LPCWSTR, std::string& contentType, std::wstringstream&)> WebServerGetReplyFunc;

class WebServerThread : public Thread
{
public:
	WebServerThread(Socket* pSocket, WebServerGetReplyFunc fGetReply, Event& evQuit) :
		m_s(pSocket), m_fGetReply(fGetReply), m_evQuit(evQuit)
	{}
private:
	void ThreadProc() {
		Event evRead;
		HANDLE handles[] = { evRead, m_evStop };
		m_s->AssignEvent(evRead, FD_READ | FD_CLOSE);

		char buff[4096];

		WSANETWORKEVENTS NetworkEvents;
		BOOL bQuit = FALSE;
		while (!bQuit) {
			switch (::WaitForMultipleObjects(2, handles, FALSE, INFINITE)) {
			case WAIT_OBJECT_0:	// read cmd
				::WSAEnumNetworkEvents(m_s->GetSocket(), evRead, &NetworkEvents);
				if (NetworkEvents.lNetworkEvents & FD_READ) {
					int nLen = m_s->Receive((BYTE*)buff, 4096, true);
					if (nLen > 0) {
						buff[nLen] = '\0';
						char path[256];
						sscanf_s(buff, "GET %[^ ]", path, 256);

						// Map to unicode
						UnicodeMultibyte umPath(path);
						std::wstringstream wss;
						std::string contentType;
						m_fGetReply(umPath, contentType, wss);

						std::stringstream ss;
						ss << "HTTP/1.1 200 OK\r\n";
						ss << "Access-Control-Allow-Origin: *\r\n";
						ss << "Content-Type:" << contentType << "\r\n";
						ss << "\r\n";
						m_s->Send(ss.str().c_str());

						// Map back to MBCS
						std::wstring str = wss.str();
						UnicodeMultibyte umRet(str);
						m_s->Send(umRet);
						m_s->Close();
						bQuit = TRUE;
					}
				}

				if (NetworkEvents.lNetworkEvents & FD_CLOSE)
					bQuit = TRUE;
				break;

			default:
				bQuit = TRUE;
				break;
			}
		}

		delete m_s;
		m_evQuit.Set();
	}

	Event& m_evQuit;
	Socket* m_s;
	WebServerGetReplyFunc m_fGetReply;
};

class WebServer : public SocketServer, public Thread
{
public:
	WebServer(int nConnections, Notifier& notifier) : SocketServer(nConnections), m_notifier(notifier) {
//		m_notifier.AddNotifyTarget(this, &m_evNotify); 
	}
	~WebServer() { Stop();  }

	DWORD Start(uint16_t port, WebServerGetReplyFunc fGetReply) {
		m_fGetReply = fGetReply;
		SetPort(port);
		DWORD dw = SocketServer::Start();
		if (dw != ERROR_SUCCESS)
			return dw;

		Thread::Start();
		return ERROR_SUCCESS;
	}

	DWORD Restart(uint16_t port) {
		Stop();
		SetPort(port);
		DWORD dw = SocketServer::Start();
		if (dw != ERROR_SUCCESS)
			return dw;

		Thread::Start();
		return ERROR_SUCCESS;
	}

protected:
	void ThreadProc() {
		Event evAccept;
		Event evThreadQuit;
		AssignEvent(evAccept, FD_ACCEPT);
		HANDLE handles[] = { evAccept, evThreadQuit, m_evStop };

		BOOL bQuit = FALSE;
		while (!bQuit) {
			switch (::WaitForMultipleObjects(3, handles, FALSE, INFINITE)) {
			case WAIT_OBJECT_0: {
				Socket* pSocket;
				if (Accept(&pSocket)) {
					WebServerThread* pWST = new WebServerThread(pSocket, m_fGetReply, evThreadQuit);
					m_threads.push_back(pWST);
					pWST->Start();
				}
			}
			break;

			case WAIT_OBJECT_0 + 1:
				m_threads.erase(std::remove_if(m_threads.begin(), m_threads.end(), [](WebServerThread* p) {
					if (p->Running())
						return false;
					delete p;
					return true;
					}), m_threads.end());
				break;

			default:
				bQuit = TRUE;
				break;
			}
		}
		Close();
	}

protected:
	Notifier& m_notifier;
//	Event m_evNotify;
	std::vector<WebServerThread*> m_threads;
	WebServerGetReplyFunc m_fGetReply;
};