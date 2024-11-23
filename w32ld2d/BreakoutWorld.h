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

	const FLOAT m_batStartWidth = 200.0f;
	const FLOAT m_batHeight = 20.0f;
	const FLOAT m_ballRadius = 10.0f;
	const FLOAT m_brickWidth = 96.0f;
	const FLOAT m_brickHeight = 30.0f;
	const FLOAT m_ballSpeed = 10.0f;

	// Brick identifiers (held in userdata)
	const int m_brickNormal = 0;
	const int m_brickBatSmaller = 1;
	const int m_brickBallFaster = 2;

	// Special timeouts
	const int batSmallerTime = 10000;	// ms
	const int ballFasterTime = 5000;

public:
	BreakoutWorld() :
		m_bat(Point2F(0, 0), m_batStartWidth, m_batHeight, 0, 0, RGB(255, 255, 255)),
		m_ball(Point2F(200.0f, 600.0f), m_ballRadius, m_ballSpeed, 135, RGB(255, 255, 255)),
		m_batWidth(m_batStartWidth),
		m_tdBatSmaller(batSmallerTime, false),
		m_tdBallFaster(ballFasterTime, false)
	{
	}
	~BreakoutWorld() {}

	void GenerateBricks(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* pIWICFactory, D2DRectScaler* pRS) {
		/*
		srand(time(NULL));

		for (FLOAT x = 0; x < m_screenWidth; x += m_brickWidth) {
					for (int y = 5; y < 35; y++) {
						double random = rand();
						if (random > RAND_MAX / 2) {
							continue;
						}

						MovingRectangle* pBrick = new MovingRectangle(Point2F(x, y * m_brickHeight),
							m_brickWidth, m_brickHeight, 0, 0,
							RGB(255, 0, 0)
						);

						m_bricks.push_back(pBrick);
						AddShape(pBrick, pDWriteFactory, pRenderTarget, pIWICFactory, pRS);
					}
				}*/

		File f;
		f.Open(L"level1.txt", GENERIC_READ, OPEN_EXISTING);
		std::string s;
		int y = 0;
		while (f.ReadLine(s)) {
			int x = 0;
			for (int i = 0; i < s.length(); i++) {
				COLORREF brickColor = RGB(0, 0, 0);
				int brickType = m_brickNormal;
				bool isBrick = true;

				switch (s.at(i)) {
					case '0':
						brickType = m_brickNormal;
						brickColor = RGB(255, 0, 0);
						break;
					case '1':
						brickType = m_brickBatSmaller;
						brickColor = RGB(0, 255, 0);
						break;
					case '2':
						brickType = m_brickBallFaster;
						brickColor = RGB(0, 0, 255);
						break;
					default:
						isBrick = false;
						break;
				}

				if (isBrick) {
					MovingRectangle* pBrick = new MovingRectangle(Point2F(x * m_brickWidth, y * m_brickHeight),
						m_brickWidth, m_brickHeight, 0, 0,
						brickColor
					);
					m_bricks.push_back(pBrick);
					AddShape(pBrick, pDWriteFactory, pRenderTarget, pIWICFactory, pRS);
					pBrick->SetUserData(brickType);
				}

				x++;
			}

			y++;
		}

/*		for (FLOAT x = 0; x < m_screenWidth; x += m_brickWidth) {
			for (int y = 3; y < 19; y++) {
				if ((y == 7) || (y == 8) || (y == 13) || (y == 14))
					continue;

				MovingRectangle* pBrick = new MovingRectangle(Point2F(x, y * m_brickHeight),
					m_brickWidth, m_brickHeight, 0, 0,
					GetRowColor(y)
				);

				double random = rand();
				double step = RAND_MAX / 20;
				if (random < step) {
					pBrick->SetUserData(m_brickBatSmaller);
					pBrick->SetColor(RGB(0, 255, 0));
				}
				else if (random < 2 * step) {
					pBrick->SetUserData(m_brickBallFaster);
					pBrick->SetColor(RGB(0, 0, 255));
				}

				m_bricks.push_back(pBrick);
				AddShape(pBrick, pDWriteFactory, pRenderTarget, pIWICFactory, pRS);
			}
		}*/
	}

	bool D2DCreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* pIWICFactory, D2DRectScaler* pRS) override {
		GenerateBricks(pDWriteFactory, pRenderTarget, pIWICFactory, pRS);

		AddShape(&m_bat, pDWriteFactory, pRenderTarget, pIWICFactory, pRS);
		AddShape(&m_ball, pDWriteFactory, pRenderTarget, pIWICFactory, pRS);

		return true;
	}

	bool D2DUpdate(ULONGLONG tick, const Point2F& mouse) override {
		m_colorBackground = D2D1::ColorF::Black;

		// Smaller bat timeout elapsed?
		if (m_tdBatSmaller.Elapsed(tick)) {
			// Put the bat back to original size
			m_tdBatSmaller.SetActive(false);
			m_batWidth = m_batStartWidth;
			m_bat.SetSize(m_batWidth, 20.0f);
		}

		if (m_tdBallFaster.Elapsed(tick)) {
			m_tdBallFaster.SetActive(false);
			m_ball.SetSpeed(m_ballSpeed);
		}

		// Move the bat to follow the mouse position
		// Position the bat such that the middle is level with the mouse x position
		FLOAT batX = mouse.x - m_batWidth / 2.0F;
		m_bat.SetPos(Point2F(LimitF(batX, 0.0f, m_screenWidth - m_batWidth), 1000.0f));

		CheckBallHitScreenEdges();

		CheckBallHitBat();

		CheckBallHitBrick();

		CheckBatHitBrick();	// for falling bricks

		// Ensure the ball angle is away from the horizontal
		AdjustBallAngle();

		m_ball.Move();

		for (auto pBrick : m_bricks) {
			pBrick->Move();
		}

		// Missed the ball?
		if (m_ball.GetPos().y > m_screenHeight - 2 * m_ballRadius) {
			return false;
		}

		return true;
	}

	void CheckBallHitScreenEdges() {
		D2D1_RECT_U rectBounds;
		rectBounds.left = rectBounds.top = 0;
		rectBounds.right = m_screenWidth;
		rectBounds.bottom = m_screenHeight;

		// Will the ball hit an edge?
		switch (m_ball.WillHitBounds(rectBounds)) {
		case Position::moveResult::hitboundsright:
		case Position::moveResult::hitboundsleft:
			m_ball.BounceX();
			break;
		case Position::moveResult::hitboundstop:
		case Position::moveResult::hitboundsbottom:
			m_ball.BounceY();
			break;
		}
	}

	void CheckBallHitBat() {
		// Did the bat hit the ball?
		if (m_ball.BounceOffRectSides(m_bat) || m_ball.BounceOffRectCorners(m_bat)) {
			FLOAT middleOfBat = m_bat.GetPos().x + m_batWidth / 2.0f;
			FLOAT middleOfBall = m_ball.GetPos().x + m_ballRadius;
			FLOAT diff = middleOfBall - middleOfBat;	// +ve = ball to the right of the middle
			diff /= (m_batWidth / 2.0f);		// diff will go from -1 -> +1
			double deviation = diff * M_PI / 4.0;	// 45 deg
			m_ball.AddDirection(deviation);
		}
	}

	void CheckBallHitBrick() {
		int special = m_brickNormal;
		bool hit = false;
		Shape* pBrickHit = NULL;

		// Has the ball hit a brick
		for (auto pBrick : m_bricks) {
			if (pBrick->IsActive() && (pBrick->GetSpeed() == 0.0F)) {
				if (m_ball.BounceOffRectSides(*pBrick)) {
					pBrick->SetActive(false);
					special = (int)pBrick->GetUserData();
					hit = true;
					pBrickHit = pBrick;
					break;
				}
			}
		}

		if (!hit) {
			for (auto pBrick : m_bricks) {
				if (pBrick->IsActive() && (pBrick->GetSpeed() == 0.0F)) {
					if (m_ball.BounceOffRectCorners(*pBrick)) {
						pBrick->SetActive(false);
						special = (int)pBrick->GetUserData();
						pBrickHit = pBrick;
						break;
					}
				}
			}
		}

		// Start the brick falling if it's a special
		if (special != m_brickNormal) {
			pBrickHit->SetActive(true);
			pBrickHit->SetDirectionInDeg(180);
			pBrickHit->SetSpeed(2.0F);
		}
	}

	void CheckBatHitBrick() {
		for (auto pBrick : m_bricks) {
			if (m_bat.HitTestShape(*pBrick)) {
				// The bat hit a (falling) brick. Activate the effect.
				int special = (int)pBrick->GetUserData();
				if (special == m_brickBatSmaller) {
					m_batWidth /= 2.0;
					m_bat.SetSize(m_batWidth, 20.0f);
					m_tdBatSmaller.SetActive(true);
				}
				else if (special == m_brickBallFaster) {
					m_ball.SetSpeed(m_ballSpeed * 2.0F);
					m_tdBallFaster.SetActive(true);
				}
				pBrick->SetActive(false);
			}
		}
	}

	void AdjustBallAngle() {
		// The ball travelling too horizontally can be very boring while the player waits for it
		// to descend so if ever we see this, correct the angle to a steeper one.
		double direction = Position::RadToDeg(m_ball.GetDirection());
		if ((direction > 70) && (direction <= 90)) {
			m_ball.SetDirection(Position::DegToRad(69.5));
		}
		else if ((direction < 110) && (direction >= 90)) {
			m_ball.SetDirection(Position::DegToRad(110.5));
		}
		else if ((direction < -70) && (direction >= -90)) {
			m_ball.SetDirection(Position::DegToRad(-70.5));
		}
		else if ((direction > -110) && (direction <= -90)) {
			m_ball.SetDirection(Position::DegToRad(-110.5));
		}
	}

protected:
	MovingCircle m_ball;
	MovingRectangle m_bat;
	FLOAT m_batWidth;
	std::vector<Shape*> m_bricks;
	TickDelta m_tdBatSmaller;
	TickDelta m_tdBallFaster;
};