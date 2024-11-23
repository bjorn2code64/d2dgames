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

	const FLOAT m_batStartWidth = 150.0f;
	const FLOAT m_batHeight = 20.0f;
	const FLOAT m_ballRadius = 10.0f;
	const FLOAT m_brickWidth = 96.0f;
	const FLOAT m_brickHeight = 30.0f;

	const FLOAT m_ballStartSpeed = 10.0f;
	const FLOAT m_ballSpeedupIncrement = 1.0f;

	// Brick identifiers (held in userdata)
	const int m_brickNormal = 0;
	const int m_brickBatLarger = 1;
	const int m_brickBallSlower = 2;
	const int m_brickMultiball = 3;

	// Special timeouts
	const int batLargerTime = 10000;	// ms
	const int ballFasterTime = 4000;

public:
	BreakoutWorld() :
		m_bat(Point2F(0, 0), m_batStartWidth, m_batHeight, 0, 0, RGB(255, 255, 255)),
		m_batWidthRequired(m_batStartWidth),
		m_tdBatLarger(batLargerTime, false),
		m_tdBallFaster(ballFasterTime, false),
		m_ballSpeed(m_ballStartSpeed)
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
			// Skip comment lines
			if ((s.length() > 0) && (s.at(0) == '#')) {
				continue;
			}

			int x = 0;
			for (int i = 0; i < s.length(); i++) {
				COLORREF brickColor = RGB(0, 0, 0);
				int brickType = m_brickNormal;
				bool isBrick = true;

				switch (s.at(i)) {
					case '0':
						brickType = m_brickNormal;
						brickColor = RGB(200, 200, 200);
						break;
					case '1':
						brickType = m_brickBatLarger;
						brickColor = RGB(0, 255, 0);
						break;
					case '2':
						brickType = m_brickBallSlower;
						brickColor = RGB(0, 0, 255);
						break;
					case '3':
						brickType = m_brickMultiball;
						brickColor = RGB(254, 255, 163);
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
		m_balls.push_back(new MovingCircle(Point2F(200.0f, 600.0f), m_ballRadius, m_ballStartSpeed, 135, RGB(255, 255, 255)));

		m_balls.push_back(new MovingCircle(Point2F(200.0f, 600.0f), m_ballRadius, m_ballStartSpeed, 135, RGB(255, 255, 255)));
		m_balls.push_back(new MovingCircle(Point2F(200.0f, 600.0f), m_ballRadius, m_ballStartSpeed, 135, RGB(255, 255, 255)));

		GenerateBricks(pDWriteFactory, pRenderTarget, pIWICFactory, pRS);

		AddShape(&m_bat, pDWriteFactory, pRenderTarget, pIWICFactory, pRS);
		for (auto pBall : m_balls) {
			AddShape(pBall, pDWriteFactory, pRenderTarget, pIWICFactory, pRS);
		}

		m_balls.at(1)->SetActive(false);
		m_balls.at(2)->SetActive(false);

		m_tdBallFaster.SetActive(true);

		return true;
	}

	bool D2DUpdate(ULONGLONG tick, const Point2F& mouse) override {
		m_colorBackground = D2D1::ColorF::Black;

		// Larger bat timeout elapsed?
		if (m_tdBatLarger.Elapsed(tick)) {
			// Put the bat back to original size
			m_tdBatLarger.SetActive(false);
			m_batWidthRequired = m_batStartWidth;
		}

		if (m_tdBallFaster.Elapsed(tick)) {
			m_ballSpeed += m_ballSpeedupIncrement;
			for (auto pBall : m_balls) {
				pBall->SetSpeed(m_ballSpeed);
			}
		}

		FLOAT batWidth = m_bat.GetWidth();

		// Move the bat to follow the mouse position
		// Position the bat such that the middle is level with the mouse x position
		FLOAT batX = mouse.x - batWidth / 2.0F;
		m_bat.SetPos(Point2F(LimitF(batX, 0.0f, m_screenWidth - batWidth), 1000.0f));

		for (auto pBall : m_balls) {
			CheckBallHitScreenEdges(pBall);
		}

		for (auto pBall : m_balls) {
			CheckBallHitBat(pBall);
		}

		for (auto pBall : m_balls) {
			CheckBallHitBrick(pBall);
		}

		CheckBatHitBrick();	// for falling bricks

		// Ensure the ball angle is away from the horizontal
		for (auto pBall : m_balls) {
			AdjustBallAngle(pBall);
		}

		for (auto pBall : m_balls) {
			pBall->Move();
		}

		// Is the bat the correct size?
		if (batWidth < m_batWidthRequired) {
			m_bat.SetWidth(batWidth + 5.0f);
		}
		else if (batWidth > m_batWidthRequired) {
			m_bat.SetWidth(batWidth - 5.0f);
		}

		// Move any falling bricks
		for (auto pBrick : m_bricks) {
			pBrick->Move();
			// Has this brick gone past the player's bat?
			if (pBrick->GetPos().y > m_screenHeight - 2 * m_ballRadius) {
				pBrick->SetActive(false);
			}
		}

		// Missed the ball?
		for (auto pBall : m_balls) {
			if (pBall->GetPos().y > m_screenHeight - 2 * m_ballRadius) {
				pBall->SetActive(false);
			}
		}

		return true;
	}

	void CheckBallHitScreenEdges(Shape* pBall) {
		if (!pBall->IsActive())
			return;
		D2D1_RECT_U rectBounds;
		rectBounds.left = rectBounds.top = 0;
		rectBounds.right = m_screenWidth;
		rectBounds.bottom = m_screenHeight;

		// Will the ball hit an edge?
		switch (pBall->WillHitBounds(rectBounds)) {
		case Position::moveResult::hitboundsright:
		case Position::moveResult::hitboundsleft:
			pBall->BounceX();
			break;
		case Position::moveResult::hitboundstop:
		case Position::moveResult::hitboundsbottom:
			pBall->BounceY();
			break;
		}
	}

	void CheckBallHitBat(MovingCircle* pBall) {
		if (!pBall->IsActive())
			return;

		// Did the bat hit the ball?
		if (pBall->BounceOffRectSides(m_bat) || pBall->BounceOffRectCorners(m_bat)) {
			FLOAT batWidth = m_bat.GetWidth();
			FLOAT middleOfBat = m_bat.GetPos().x + batWidth / 2.0f;
			FLOAT middleOfBall = pBall->GetPos().x + m_ballRadius;
			FLOAT diff = middleOfBall - middleOfBat;	// +ve = ball to the right of the middle
			diff /= (batWidth / 2.0f);		// diff will go from -1 -> +1
			double deviation = diff * M_PI / 4.0;	// 45 deg
			pBall->AddDirection(deviation);
		}
	}

	void CheckBallHitBrick(MovingCircle* pBall) {
		int special = m_brickNormal;
		bool hit = false;
		Shape* pBrickHit = NULL;

		// Has the ball hit a brick
		for (auto pBrick : m_bricks) {
			if (pBrick->IsActive() && (pBrick->GetSpeed() == 0.0F)) {
				if (pBall->BounceOffRectSides(*pBrick)) {
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
					if (pBall->BounceOffRectCorners(*pBrick)) {
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
			if (pBrick->IsActive() && m_bat.HitTestShape(*pBrick)) {
				// The bat hit a (falling) brick. Activate the effect.
				int special = (int)pBrick->GetUserData();
				if (special == m_brickBatLarger) {
					m_batWidthRequired *= 2.0;
					if (m_batWidthRequired > 300.0F) {
						m_batWidthRequired = 300.0f;
					}
					m_tdBatLarger.SetActive(true);
				}
				else if (special == m_brickBallSlower) {
					m_ballSpeed -= 5.0f;
					if (m_ballSpeed < 5.0f) {
						m_ballSpeed = 5.0f;
					}
					for (auto pBall : m_balls) {
						pBall->SetSpeed(m_ballSpeed);
					}
				}
				else if (special == m_brickMultiball) {
					Point2F pos(0.0F, 0.0F);
					double direction;
					// Get the pos/dir of an active ball
					for (auto pBall : m_balls) {
						if (pBall->IsActive()) {
							pos = pBall->GetPos();
							direction = pBall->GetDirection();
							break;
						}
					}

					if ((pos.x != 0.0F) || (pos.y != 0.0F)) {
						// Spawn all the non active balls from it
						for (auto pBall : m_balls) {
							if (!pBall->IsActive()) {
								pBall->SetPos(pos);
								pBall->SetDirection(direction - 0.02f);
								pBall->SetActive(true);
							}
						}
					}
				}
				pBrick->SetActive(false);
			}
		}
	}

	void AdjustBallAngle(MovingCircle* pBall) {
		// The ball travelling too horizontally can be very boring while the player waits for it
		// to descend so if ever we see this, correct the angle to a steeper one.
		double direction = Position::RadToDeg(pBall->GetDirection());
		if ((direction > 70) && (direction <= 90)) {
			pBall->SetDirection(Position::DegToRad(69.5));
		}
		else if ((direction < 110) && (direction >= 90)) {
			pBall->SetDirection(Position::DegToRad(110.5));
		}
		else if ((direction < -70) && (direction >= -90)) {
			pBall->SetDirection(Position::DegToRad(-70.5));
		}
		else if ((direction > -110) && (direction <= -90)) {
			pBall->SetDirection(Position::DegToRad(-110.5));
		}
	}

protected:
	std::vector<MovingCircle*> m_balls;
	MovingRectangle m_bat;
	FLOAT m_batWidthRequired;
	std::vector<Shape*> m_bricks;
	TickDelta m_tdBatLarger;
	TickDelta m_tdBallFaster;
	FLOAT m_ballSpeed;
};