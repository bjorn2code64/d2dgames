#pragma once

#include <d2dWindow.h>
#define _USE_MATH_DEFINES	// for M_PI
#include <math.h>

class Vector2F {
public:
	FLOAT x, y;

	Vector2F() : x(0.0F), y(0.0F) {}
	Vector2F(FLOAT x, FLOAT y) : x(x), y(y) {}

	Vector2F operator-(const Vector2F& other) const {
		return { x - other.x, y - other.y };
	}

	Vector2F operator+(const Vector2F& other) const {
		return { x + other.x, y + other.y };
	}

	Vector2F operator*(float scalar) const {
		return { x * scalar, y * scalar };
	}

	float dot(const Vector2F& other) const {
		return x * other.x + y * other.y;
	}

	Vector2F normalize() const {
		float length = std::sqrt(x * x + y * y);
		return { x / length, y / length };
	}

	float lengthsq() const {
		return (x * x) + (y * y);
	}

	double anglerad() const {
		if (y == 0) {
			return (x > 0) ? M_PI / 2 : 3 * M_PI / 2;
		}
		return M_PI / 2 + atan2(y, x);
	}
};

Vector2F reflect(const Vector2F& direction, const Vector2F& normal) {
	return direction - normal * (2 * direction.dot(normal));
}

class Position
{
public:
	static double RadToDeg(double rad) {
		return rad / M_PI * 180;
	}

	static double DegToRad(double deg) {
		return deg / 180 * M_PI;
	}

	enum class moveResult {
		ok,
		hitboundsright,
		hitboundsleft,
		hitboundstop,
		hitboundsbottom
	};

	Position(const Point2F& pos, float speed, int direction) : m_pos(pos), m_fSpeed(speed)
	{
		SetDirectionInDeg(direction);
	}

	const Point2F& GetPos() const { return m_pos; }
	void SetPos(const Point2F& pos) { m_pos = pos; }
	void OffsetPos(const Point2F& pos) { m_pos += pos;  }

	void SetDirectionInDeg(int directionInDeg) {
		m_direction = ((double)directionInDeg * M_PI) / 180.0;
		UpdateCache();
	}

	void SetDirection(double direction) {
		m_direction = direction;
		UpdateCache();
	}

	void SetSpeed(float speed) {
		m_fSpeed = speed;
		UpdateCache();
	}

	virtual moveResult WillHitBounds(const D2D1_RECT_U& bounds) { return moveResult::ok; }
	virtual void MovePos(Point2F& pos, float len = 0.0)
	{
		if (len > 0.0) {
			float movelen = sqrt(m_cacheStep.lengthsq());
			float fraction = len / movelen;
			pos.x += m_cacheStep.x * fraction;
			pos.y += m_cacheStep.y * fraction;
		}
		else {
			pos.x += m_cacheStep.x;
			pos.y += m_cacheStep.y;
		}
	}

	void Move() { m_pos.x += m_cacheStep.x; m_pos.y += m_cacheStep.y;  }

	static double GetBounceX(double dir) {
		if ((dir >= M_PI / 2.0) && (dir < 3.0 * M_PI / 2.0)) {
			return M_PI / 2.0 + (3.0 * M_PI / 2.0) - dir;
		}
		return 2.0 * M_PI - dir;
	}

	static double GetBounceY(double dir) {
		if (dir <= M_PI) {
			return M_PI - dir;
		}
		return M_PI + (2.0 * M_PI) - dir;
	}

	void BounceX() {
		m_direction = GetBounceX(m_direction);
		UpdateCache();
	}

	void BounceY() {
		m_direction = GetBounceY(m_direction);
		UpdateCache();
	}

	void Offset(const Point2F& pos) {
		m_pos += pos;
	}

	double GetDirection() {
		return m_direction;
	}

	void AddDirection(double f) {
		m_direction += f;
		UpdateCache();
	}

protected:
	void UpdateCache() {
//		char buff[256];
//		snprintf(buff, 256, "deg %f rad %f\n", RadToDeg(m_direction), m_direction);
//		OutputDebugStringA(buff);
		m_cacheStep = Vector2F((FLOAT)sin(m_direction), (FLOAT)-cos(m_direction)) * m_fSpeed;
	}

protected:
	Point2F m_pos;			// Current position
	double m_direction;		// Held in radians
	FLOAT m_fSpeed;			// Movement speed
	Vector2F m_cacheStep;	// Cache the move vector so we're not doing constant Trig
};

class Shape : public Position
{
public:
	Shape(const Point2F& pos, float speed, int dir, UINT32 rgb, LPARAM userdata = 0) : Position(pos, speed, dir), m_pBrush(NULL), m_rgb(rgb), m_userdata(userdata), m_active(false)  {
	}

	~Shape() {
		D2DDiscardResources();
	}

	virtual void D2DOnCreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* m_pIWICFactory, const D2DRectScaler* pRS) {
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

	virtual moveResult WillHitBounds(const D2D1_RECT_U& bounds) { return __super::WillHitBounds(bounds); }

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

class MovingCircle : public Shape
{
public:
	MovingCircle(const Point2F& pos, float radius, float speed, int dir, UINT32 rgb, LPARAM userdata = 0) : Shape(pos, speed, dir, rgb, userdata), m_fRadius(radius) {}

	bool HitTest(Point2F pos) override {
		float distSq = (pos.x - m_pos.x) * (pos.x - m_pos.x) +
			(pos.y - m_pos.y) * (pos.y - m_pos.y);
		return distSq <= m_fRadius * m_fRadius;
	}

	virtual moveResult WillHitBounds(const D2D1_RECT_U& bounds) override {
		auto currPos = GetPos();
		__super::MovePos(currPos);

		// Check if it hit an edge
		if (currPos.x < bounds.left + m_fRadius) {
			return moveResult::hitboundsleft;
		}
		if (currPos.x > bounds.right - m_fRadius) {
			return moveResult::hitboundsright;
		}
		if (currPos.y < bounds.top + m_fRadius) {
			return moveResult::hitboundstop;
		}
		if (currPos.y > bounds.bottom - m_fRadius) {
			return moveResult::hitboundsbottom;
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

	bool BounceOffRectSides(const Shape& shape) {
		RectF r;
		shape.GetBoundingBox(&r);

		Point2F pos = GetPos();	// where we are
		MovePos(pos);	// where we will be

		// Check we're anywhere near it first
		if ((pos.x + m_fRadius < r.left) || (pos.x - m_fRadius > r.right) ||
			(pos.y + m_fRadius < r.top) || (pos.y - m_fRadius > r.bottom)) {
			return false;
		}

		float endlen = sqrt(m_cacheStep.lengthsq());
		float len = 0.0;
		while (len < endlen) {	// Check in radius/2 steps
			len += 1.0;

			pos = GetPos();		// where we are
			MovePos(pos, len);	// where we will be

			// Check left and right bounce zone
			if (WillHitRectX(r, pos)) {
				SetPos(pos);
				BounceX();
				return true;
			}

			// Check top and bottom bounce zone
			if (WillHitRectY(r, pos)) {
				SetPos(pos);
				BounceY();
				return true;
			}
		}

		return false;
	}

	bool BounceOffRectCorners(const Shape& shape) {
		RectF r;
		shape.GetBoundingBox(&r);

		Point2F pos = GetPos();	// where we are
		MovePos(pos);	// where we will be

		// Check we're anywhere near it first
		if ((pos.x + m_fRadius < r.left) || (pos.x - m_fRadius > r.right) ||
			(pos.y + m_fRadius < r.top) || (pos.y - m_fRadius > r.bottom)) {
			return false;
		}

		double radsq = m_fRadius * m_fRadius;
		std::vector<Point2F> corners;
		r.GetCorners(corners);

		float endlensq = m_cacheStep.lengthsq();
		float lensq = 0.0;
		while (lensq < endlensq) {	// Check in radius/2 steps
			lensq += 1.0;
			MovePos(pos, lensq);	// where we will be

			// Check all 4 corners
			for (auto& corner : corners) {
				if (pos.DistanceToSq(corner) < radsq - 0.1F) {
					SetPos(pos);
					BounceOffPoint(corner);
					return true;
				}
			}
		}

		return false;
	}

protected:
	void BounceOffPoint(const Point2F& pt) {
		// Gradient between pt and direction
		double direction = m_cacheStep.anglerad();
		double touch = pt.angleradTo(m_pos);
		double deflection = M_PI - (direction - touch) * 2;
		m_direction += deflection;
		UpdateCache();
	}

	bool WillHitRectX(const RectF& r, const Point2F& pos) {
		if ((pos.y >= r.top) && (pos.y <= r.bottom)) {
			if ((pos.x >= r.right) && (pos.x - m_fRadius <= r.right)) {
				return true;
			}
			if ((pos.x <= r.left) && (pos.x + m_fRadius >= r.left)) {
				return true;
			}
		}
		return false;
	}

	bool WillHitRectY(const RectF& r, const Point2F& pos) {
		if ((pos.x >= r.left) && (pos.x <= r.right)) {
			if ((pos.y >= r.bottom) && (pos.y - m_fRadius <= r.bottom)) {
				return true;
			}
			if ((pos.y <= r.top) && (pos.y + m_fRadius >= r.top)) {
				return true;
			}
		}
		return false;
	}

protected:
	FLOAT m_fRadius;
};

class MovingRectangle : public Shape
{
public:
	MovingRectangle(const Point2F& pos, float width, float height, float speed, int dir, UINT32 rgb, LPARAM userdata = 0) :
		Shape(pos, speed, dir, rgb, userdata), m_fWidth(width), m_fHeight(height)
	{}

	bool HitTest(Point2F pos) override {
		return false;
	}

	void SetSize(FLOAT width, FLOAT height) {
		m_fWidth = width;
		m_fHeight = height;
	}

	moveResult WillHitBounds(const D2D1_RECT_U& bounds) {
		Point2F currPos = GetPos();
		__super::MovePos(currPos);

		// Check if it hit an edge
		if (currPos.x < bounds.left) {
			return moveResult::hitboundsleft;
		}
		if (currPos.x > bounds.right - m_fWidth) {
			return moveResult::hitboundsright;
		}
		if (currPos.y < bounds.top) {
			return moveResult::hitboundstop;
		}
		if (currPos.y > bounds.bottom - m_fHeight) {
			return moveResult::hitboundsbottom;
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

class MovingBitmap : public Shape
{
public:
	MovingBitmap(d2dBitmap* bitmap, const Point2F& pos, float width, float height, float speed, int dir, UINT32 rgb, LPARAM userdata = 0) :
		m_bitmap(bitmap), Shape(pos, speed, dir, rgb, userdata), m_fWidth(width), m_fHeight(height) {}

	void SetBitmap(d2dBitmap* bitmap) {
		m_bitmap = bitmap;
	}

	bool HitTest(Point2F pos) override {
		return false;
	}

	moveResult WillHitBounds(const D2D1_RECT_U& bounds) {
		Point2F currPos = GetPos();
		__super::MovePos(currPos);

		// Check if it hit an edge
		if (currPos.x < bounds.left) {
			return moveResult::hitboundsleft;
		}
		if (currPos.x > bounds.right - m_fWidth) {
			return moveResult::hitboundsright;
		}
		if (currPos.y < bounds.top) {
			return moveResult::hitboundstop;
		}
		if (currPos.y > bounds.bottom - m_fHeight) {
			return moveResult::hitboundsbottom;
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
		m_bitmap->Render(pRenderTarget, r);
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
	d2dBitmap* m_bitmap;
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

	void D2DOnCreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* pIWICFactory, const D2DRectScaler* pRS) override {
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

		__super::D2DOnCreateResources(pDWriteFactory, pRenderTarget, pIWICFactory, pRS);
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
	D2DWorld() : m_colorBackground(D2D1::ColorF::Black) {

	}

	virtual bool D2DCreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* pIWICFactory, D2DRectScaler* pRS)
	{
		return true;
	}

	virtual bool D2DDiscardResources() {
		for (auto p : m_shapes) {
			p->D2DDiscardResources();
		}
		return true;
	}

	void AddShape(Shape* p, IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* pIWICFactory, D2DRectScaler* pRS, bool active = true) {
		p->D2DOnCreateResources(pDWriteFactory, pRenderTarget, pIWICFactory, pRS);
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

	void D2DPreRender(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* pIWICFactory, D2DRectScaler* pRS) {
		// Now is the time to add queued shapes to the engine.
		for (auto p : m_shapesQueue) {
			AddShape(p, pDWriteFactory, pRenderTarget, pIWICFactory, pRS);
		}
		m_shapesQueue.clear();
	}

	void QueueShape(Shape* p) {
		m_shapesQueue.push_back(p);
	}

	bool Init() {
		return true;
	}

	virtual bool D2DUpdate(ULONGLONG tick, const Point2F& mouse) {
		return true;
	}

	virtual bool D2DRender(ID2D1HwndRenderTarget* pRenderTarget, D2DRectScaler* pRsFAR) {
		for (auto p : m_shapes)
			if (p->IsActive())
				p->Draw(pRenderTarget, pRsFAR);

		return true;
	}

	virtual w32Size D2DGetScreenSize() {
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
public:
	D2D1::ColorF m_colorBackground;
};