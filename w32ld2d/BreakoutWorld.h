#pragma once

#include <D2DWorld.h>

FLOAT LimitF(FLOAT x, FLOAT min, FLOAT max) {
	return (x < min) ? min : (x > max ? max : x);
}

class BreakoutWorld : public D2DWorld
{
protected:
	const int m_screenWidth = 1920;
	const int m_screenHeight = 1080;

	const FLOAT m_playerWidth = 200.0f;
	const FLOAT m_ballRadius = 10.0f;
	const FLOAT m_brickWidth = 80.0f;
	const FLOAT m_brickHeight = 20.0f;

public:
	BreakoutWorld() :
		m_player(Point2F(0, 0), m_playerWidth, 20.0f, 0, 0, RGB(255, 255, 255)),
		m_ball(Point2F(200.0f, 200.0f), m_ballRadius * 2, m_ballRadius * 2, 20.0f, 45, RGB(255, 255, 255))
	{}
	~BreakoutWorld() {}

	bool CreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRS) {
		for (FLOAT x = 0; x < m_screenWidth; x += m_brickWidth) {
			for (int y = 0; y < 5; y++) {
				MovingRectangle* pBrick = new MovingRectangle(Point2F(x, y * m_brickHeight),
					m_brickWidth - 1.0f, m_brickHeight - 1.0f, 0, 0,
					RGB(w32rand(255), w32rand(255), w32rand(255))
				);
				pBrick->D2DOnCreateResources(pDWriteFactory, pRenderTarget, pRS);
				m_bricks.push_back(pBrick);
			}
		}

		m_player.D2DOnCreateResources(pDWriteFactory, pRenderTarget, pRS);
		m_ball.D2DOnCreateResources(pDWriteFactory, pRenderTarget, pRS);
		return true;
	}

	bool DiscardResources() {
		for (auto p : m_bricks) {
			p->D2DDiscardResources();
		}
		m_player.D2DDiscardResources();
		m_ball.D2DDiscardResources();
		return true;
	}

	bool Init() {
		return true;
	}

	bool Update(ULONGLONG tick, const Point2F& mouse) {
		m_player.SetPos(Point2F(LimitF(mouse.x, 0.0f, m_screenWidth - m_playerWidth), 1000.0f));
		D2D1_RECT_U rectBounds;
		rectBounds.left = rectBounds.top = 0;
		rectBounds.right = m_screenWidth;
		rectBounds.bottom = m_screenHeight;
		switch (m_ball.Move(&rectBounds)) {
		case Position::moveResult::hitboundsright:
		case Position::moveResult::hitboundsleft:
			m_ball.BounceX();
			break;
		case Position::moveResult::hitboundstop:
		case Position::moveResult::hitboundsbottom:
			m_ball.BounceY();
			break;
		}

		if (m_ball.HitTestShape(m_player)) {
			m_ball.BounceY();
			m_ball.Move(&rectBounds);
		}

		auto it = m_bricks.begin();
		while (it != m_bricks.end()) {
			if (m_ball.HitTestShape(*(*it))) {
				delete* it;
				it = m_bricks.erase(it++);
				m_ball.BounceY();
			}
			else {
				++it;
			}
		}

		if (m_ball.GetPos().y > m_screenHeight - 2 * m_ballRadius) {
			return false;
		}

		return true;
	}

	bool Render(ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRsFAR) {
		m_player.Draw(pRenderTarget, pRsFAR);
		m_ball.Draw(pRenderTarget, pRsFAR);
		for (auto p : m_bricks) {
			p->Draw(pRenderTarget, pRsFAR);
		}
		return true;
	}

protected:
	MovingRectangle m_ball;
	MovingRectangle m_player;
	std::vector<Shape*> m_bricks;
};