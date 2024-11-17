#include <string>

#include <d2dWindow.h>
#include <time.h>

#include <WindowSaverExt.h>

#include "BouncyWorld.h"
#include "InvaderWorld.h"
#include "BreakoutWorld.h"

#define APPNAME L"w32ld2d"

////////////////////////////////////////////////////////////////////////////////

class MainWindow : public D2DWindow
{
public:
	MainWindow() :
		D2DWindow(WINDOW_FLAGS_QUITONCLOSE),
		m_pBrush(NULL),
		m_windowSaver(APPNAME)
	{
		srand((unsigned int)time(NULL));	// Stop random numbers being the same every time
		w32seed();

		AddExt(&m_windowSaver);
	}

	~MainWindow() {
	}

	// Custom create function specifying we want our OnPaint() called automatically
	DWORD CreateAndShow(int nCmdShow) {
		RETURN_IF_ERROR(CreateOverlapped(APPNAME));

		m_world.Init();
		D2DWindow::Init(m_world.GetScreenSize());

		Show(nCmdShow);
		return ERROR_SUCCESS;
	}

protected:
	void D2DOnCreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* pIWICFactory) override  {
		pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_pBrush);
		m_world.CreateResources(pDWriteFactory, pRenderTarget, pIWICFactory, &m_rsFAR);
	}

	bool D2DUpdate() {
		ULONGLONG tick = GetTickCount64();
		return m_world.Update(tick, m_ptMouse);
	}

	void D2DPreRender(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* pIWICFactory) override {
		m_world.ProcessQueue(pDWriteFactory, pRenderTarget, pIWICFactory, &m_rsFAR);
	}

	void D2DRender(ID2D1HwndRenderTarget* pRenderTarget) override {
		D2DClearScreen(D2D1::ColorF::Black);

		RectF rectBounds;
		D2DGetFARRect(&rectBounds);
		pRenderTarget->DrawRectangle(rectBounds, m_pBrush);

		m_world.Render(pRenderTarget, &m_rsFAR);
	}

	void D2DOnDiscardResources() override {
		m_world.DiscardResources();

		SafeRelease(&m_pBrush);
	}

	LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		if (uMsg == WM_MOUSEMOVE) {
			w32Point mouse = lParam;
			m_rsFAR.ReverseScaleAndOffset(&mouse);
			m_ptMouse = mouse;
		}

		return __super::WndProc(hWnd, uMsg, wParam, lParam);
	}

protected:
//	BouncyWorld m_world;
	BreakoutWorld m_world;
//	InvaderWorld m_world;

	ID2D1SolidColorBrush* m_pBrush;	// Outline brush - white
	Point2F m_ptMouse;				// Track the mouse position
	WindowSaverExt m_windowSaver;	// Save the window position between runs
};

////////////////////////////////////////////////////////////////////////////////
// Main routine. Init the library, create a window and run the program.
////////////////////////////////////////////////////////////////////////////////

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	int ret = 0;
	if (SUCCEEDED(CoInitialize(NULL))) {
		Window::LibInit(hInstance);	// Initialise the library

		MainWindow w;	// Our custom Window (defined above)

		// Create and show it
		DWORD dwError = w.CreateAndShow(nCmdShow);
		if (dwError == ERROR_SUCCESS) {
			ret = w.Run();	// Run the standard Windows message loop
		}
		else {
			Window::ReportError(dwError);	// Show the error
		}

		CoUninitialize();
	}

	return ret;
}

////////////////////////////////////////////////////////////////////////////////
