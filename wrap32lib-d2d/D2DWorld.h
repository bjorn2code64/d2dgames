#pragma once

#include <d2dWindow.h>
#define _USE_MATH_DEFINES	// for M_PI
#include <math.h>

class Position
{
public:
	enum class moveResult {
		ok,
		hitboundsright,
		hitboundsleft,
		hitboundstop,
		hitboundsbottom
	};

	Position(const Point2F& pos, float speed, int direction) : m_pos(pos), m_fSpeed(speed) {
		SetDirection(direction);
	}

	const Point2F& GetPos() const { return m_pos; }
	void SetPos(const Point2F& pos) { m_pos = pos; }
	void OffsetPos(const Point2F& pos) { m_pos += pos;  }

	void SetDirection(int directionInDeg) {
		m_direction = ((double)directionInDeg * M_PI) / 180.0;
		UpdateCache();
	}

	void SetSpeed(float speed) {
		m_fSpeed = speed;
		UpdateCache();
	}

	virtual moveResult WillHit(D2D1_RECT_U* pBounds = NULL) { return moveResult::ok; }
	virtual void MovePos(Point2F& pos) { pos += m_cacheStep; }
	void Move() { m_pos += m_cacheStep; }

	void BounceX() {
		if ((m_direction >= M_PI / 2.0) && (m_direction < 3.0 * M_PI / 2.0)) {
			m_direction = M_PI / 2.0 + (3.0 * M_PI / 2.0) - m_direction;
		}
		else {
			m_direction = 2.0 * M_PI - m_direction;
		}
		UpdateCache();
	}

	void BounceY() {
		if (m_direction <= M_PI) {
			m_direction = M_PI - m_direction;
		}
		else {
			m_direction = M_PI + (2.0 * M_PI) - m_direction;
		}
		UpdateCache();
	}

	void Offset(const Point2F& pos) {
		m_pos += pos;
	}

protected:
	void UpdateCache() {
		m_cacheStep = Point2F((FLOAT)sin(m_direction), (FLOAT)-cos(m_direction)) * m_fSpeed;
	}

protected:
	Point2F m_pos;			// Current position
	double m_direction;		// Held in radians
	FLOAT m_fSpeed;			// Movement speed
	Point2F m_cacheStep;	// Cache the move vector so we're not doing constant Trig
};

class Shape : public Position
{
public:
	Shape(const Point2F& pos, float speed, int dir, UINT32 rgb, LPARAM userdata = 0) : Position(pos, speed, dir), m_pBrush(NULL), m_rgb(rgb), m_userdata(userdata), m_active(false)  {
	}

	~Shape() {
		D2DDiscardResources();
	}

	virtual void D2DOnCreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRS) {
		UINT32 rgb =
			(m_rgb & 0x000000ff) << 16 |
			(m_rgb & 0x0000ff00) |
			(m_rgb & 0x0000ff0000) >> 16 ;

		pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(rgb), &m_pBrush);
		m_active = true;
	}

	virtual void D2DDiscardResources() {
		SafeRelease(&m_pBrush);
	}

	void SetActive(bool b) { m_active = b;  }
	bool IsActive() { return m_active;  }

	virtual moveResult WillHit(D2D1_RECT_U* pBounds = NULL) { return __super::WillHit(pBounds); }
//	virtual void MovePos(Point2F& pos) { return __super::MovePos(pos); }

	ID2D1SolidColorBrush* GetBrush() { return m_pBrush; }

	virtual void Draw(ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRS = NULL) {}
	virtual bool HitTest(Point2F pos) { return false; }
	virtual void GetBoundingBox(RectF*) const {}

	bool HitTestShape(const Shape& rhs) {
		RectF rThis;
		GetBoundingBox(&rThis);

		RectF rThat;
		rhs.GetBoundingBox(&rThat);
		return rThis.hitTest(rThat);
	}

	void SetColor(UINT32 rgb) { m_rgb = rgb; }

	void SetUserData(LPARAM l) { m_userdata = l;  }
	LPARAM GetUserData() { return m_userdata;  }

protected:
	ID2D1SolidColorBrush* m_pBrush;
	UINT32 m_rgb;
	LPARAM m_userdata;
	bool m_active;
};

class MovingEllipse : public Shape
{
public:
	MovingEllipse(const Point2F& pos, float radius, float speed, int dir, UINT32 rgb, LPARAM userdata = 0) : Shape(pos, speed, dir, rgb, userdata), m_fRadius(radius) {}

	bool HitTest(Point2F pos) override {
		float distSq = (pos.x - m_pos.x) * (pos.x - m_pos.x) +
			(pos.y - m_pos.y) * (pos.y - m_pos.y);
		return distSq <= m_fRadius * m_fRadius;
	}

	virtual moveResult WillHit(D2D1_RECT_U* pBounds) override {
		auto currPos = GetPos();
		__super::MovePos(currPos);

		if (pBounds) {
			// Check if it hit an edge
			if (currPos.x < pBounds->left + m_fRadius) {
				return moveResult::hitboundsleft;
			}
			if (currPos.x > pBounds->right - m_fRadius) {
				return moveResult::hitboundsright;
			}
			if (currPos.y < pBounds->top + m_fRadius) {
				return moveResult::hitboundstop;
			}
			if (currPos.y > pBounds->bottom - m_fRadius) {
				return moveResult::hitboundsbottom;
			}
		}

		return moveResult::ok;
	}

	void Draw(ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRS = NULL) override {
		D2D1_ELLIPSE e;
		e.point = GetPos();
		if (pRS)	pRS->Scale(&e.point);

		FLOAT r = m_fRadius;
		if (pRS)	pRS->ScaleNoOffset(&r);
		e.radiusX = e.radiusY = r;

		pRenderTarget->FillEllipse(&e, GetBrush());
	}

	void GetBoundingBox(RectF* p) const {
		p->left = GetPos().x - m_fRadius;
		p->right = GetPos().x + m_fRadius;
		p->top = GetPos().y - m_fRadius;
		p->bottom = GetPos().y + m_fRadius;
	}

protected:
	FLOAT m_fRadius;
};

class MovingRectangle : public Shape
{
public:
	MovingRectangle(const Point2F& pos, float width, float height, float speed, int dir, UINT32 rgb, LPARAM userdata = 0) : Shape(pos, speed, dir, rgb, userdata), m_fWidth(width), m_fHeight(height) {}

	bool HitTest(Point2F pos) override {
		return false;
	}

	moveResult WillHit(D2D1_RECT_U* pBounds = NULL) {
		Point2F currPos = GetPos();
		__super::MovePos(currPos);

		if (pBounds) {
			// Check if it hit an edge
			if (currPos.x < pBounds->left) {
				return moveResult::hitboundsleft;
			}
			if (currPos.x > pBounds->right - m_fWidth) {
				return moveResult::hitboundsright;
			}
			if (currPos.y < pBounds->top) {
				return moveResult::hitboundstop;
			}
			if (currPos.y > pBounds->bottom - m_fHeight) {
				return moveResult::hitboundsbottom;
			}
		}

		return moveResult::ok;
	}

	void Draw(ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRS = NULL) override {
		Point2F pos = GetPos();
		FLOAT fWidth = m_fWidth;
		FLOAT fHeight = m_fHeight;
		if (pRS) {
			pRS->Scale(&pos);
			pRS->ScaleNoOffset(&fWidth);
			pRS->ScaleNoOffset(&fHeight);
		}

		D2D1_RECT_F r;
		r.left = pos.x;
		r.right = pos.x + fWidth;
		r.top = pos.y;
		r.bottom = pos.y + fHeight;
		pRenderTarget->FillRectangle(&r, GetBrush());
	}

	void GetBoundingBox(RectF* p) const {
		p->left = GetPos().x;
		p->right = GetPos().x + m_fWidth;
		p->top = GetPos().y;
		p->bottom = GetPos().y + m_fHeight;
	}

protected:
	FLOAT m_fWidth;
	FLOAT m_fHeight;
};

class MovingText : public Shape {
public:
	MovingText(LPCWSTR wsz, const Point2F& pos, float width, float height, float speed, int dir, UINT32 rgb, DWRITE_TEXT_ALIGNMENT ta, LPARAM userdata = 0) :
		m_text(wsz),
		Shape(pos, speed, dir, rgb, userdata),
		m_fWidth(width), m_fHeight(height),
		m_pWTF(NULL),
		m_ta(ta)
	{
	}

	void D2DOnCreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRS) override {
		FLOAT fHeight = m_fHeight;
		pRS->ScaleNoOffset(&fHeight);
		pDWriteFactory->CreateTextFormat(
			L"Arial",
			NULL,
			DWRITE_FONT_WEIGHT_REGULAR,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			fHeight,
			L"en-us",
			&m_pWTF
		);

		m_pWTF->SetTextAlignment(m_ta);

		__super::D2DOnCreateResources(pDWriteFactory, pRenderTarget, pRS);
	}

	void D2DDiscardResources() override {
		SafeRelease(&m_pWTF);
		__super::D2DDiscardResources();
	}

	void SetText(LPCWSTR wsz) {
		m_text = wsz;
	}

	void Draw(ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRS = NULL) override {
		Point2F pos = GetPos();
		FLOAT fWidth = m_fWidth;
		FLOAT fHeight = m_fHeight;
		if (pRS) {
			pRS->Scale(&pos);
			pRS->ScaleNoOffset(&fWidth);
			pRS->ScaleNoOffset(&fHeight);
		}

		D2D1_RECT_F r;
		r.left = pos.x;
		r.right = pos.x + fWidth;
		r.top = pos.y;
		r.bottom = pos.y + fHeight;
		pRenderTarget->DrawTextW(m_text.c_str(), (UINT32)m_text.length(), m_pWTF, r, m_pBrush);
	}

protected:
	FLOAT m_fWidth;
	FLOAT m_fHeight;
	IDWriteTextFormat* m_pWTF;
	std::wstring m_text;
	DWRITE_TEXT_ALIGNMENT m_ta;
};

class TickDelta {
public:
	TickDelta(ULONGLONG periodMS, bool active = true) : m_periodMS(periodMS), m_active(active) {
		m_ullLast = GetTickCount64();
	}

	bool Elapsed(ULONGLONG tick) {
		if (!m_active)
			return false;

		if (tick - m_ullLast >= m_periodMS) {
			m_ullLast = tick;
			return true;
		}
		return false;
	}

	void SetActive(bool b) {
		m_ullLast = GetTickCount64();
		m_active = b;
	}

	void AddTicks(int ticks) {
		m_periodMS += ticks;
	}

protected:
	ULONGLONG m_ullLast;
	ULONGLONG m_periodMS;
	bool m_active;
};

class D2DWorld
{
public:
	virtual bool CreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, D2DRectScaler* pRS) {
		return true;
	}

	bool DiscardResources() {
		for (auto p : m_shapes) {
			p->D2DDiscardResources();
		}
		return true;
	}

	void AddShape(Shape* p, IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, D2DRectScaler* pRS, bool active = true) {
		p->D2DOnCreateResources(pDWriteFactory, pRenderTarget, pRS);
		p->SetActive(active);
		m_shapes.push_back(p);
	}

	void RemoveShape(Shape* p) {
		for (auto it = m_shapes.begin(); it != m_shapes.end(); ) {
			if (*it == p)
				it = m_shapes.erase(it);
			else
				++it;
		}
		p->D2DDiscardResources();
	}

	void ProcessQueue(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, D2DRectScaler* pRS) {
		for (auto p : m_shapesQueue) {
			p->D2DOnCreateResources(pDWriteFactory, pRenderTarget, pRS);
			m_shapes.push_back(p);
		}
		m_shapesQueue.clear();
	}

	void QueueShape(Shape* p) {
		m_shapesQueue.push_back(p);
	}

	bool Init() {
		return true;
	}

	virtual bool Update(ULONGLONG tick, const Point2F& mouse) {
		return true;
	}

	bool Render(ID2D1HwndRenderTarget* pRenderTarget, D2DRectScaler* pRsFAR) {
		for (auto p : m_shapes)
			if (p->IsActive())
				p->Draw(pRenderTarget, pRsFAR);

		return true;
	}

	virtual w32Size GetScreenSize() {
		return w32Size(1920, 1080);
	}

protected:
	int KeyDown(int keycode) {
		return ::GetAsyncKeyState(keycode) & 0x8000;
	}

	int KeyPressed(int keycode) {
		return ::GetAsyncKeyState(keycode) & 0x0001;
	}

protected:
	std::vector<Shape*> m_shapes;
	std::vector<Shape*> m_shapesQueue;
};