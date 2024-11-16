#pragma once

#include <D2DWorld.h>

class BouncyWorld : public D2DWorld
{
public:
	const int m_screenWidth = 1920;
	const int m_screenHeight = 1080;

	const int m_numShapes = 1000;
	const float m_maxRadius = 60.0f;
	const float m_minRadius = 10.0f;

	BouncyWorld() {}
	~BouncyWorld() {
		for (auto shape : m_shapes)
			delete shape;
	}

	bool CreateResources(IDWriteFactory* pDWriteFactory, ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRS) {
		for (auto& shape : m_shapes)
			shape->D2DOnCreateResources(pDWriteFactory, pRenderTarget, pRS);

		return true;
	}

	bool DiscardResources() {
		for (auto shape : m_shapes)
			shape->D2DDiscardResources();
		return true;
	}

	bool Init() {
		for (int i = 0; i < m_numShapes; i++) {
			FLOAT speed = w32randf(5.0f, 15.f);
			DWORD direction = w32rand(0, 359);
			COLORREF color = RGB(w32rand(0, 256), w32rand(0, 256), w32rand(0, 256));
			if (w32rand(1)) {
				FLOAT width = w32randf(m_minRadius, m_maxRadius);
				FLOAT height = w32randf(m_minRadius, m_maxRadius);

				Point2F pos(w32randf(0, m_screenWidth - width), w32randf(0, m_screenHeight - height));

				m_shapes.push_back(new MovingRectangle(
					pos,
					width, height,
					speed,
					direction,
					color));
			}
			else {
				float radius = w32randf(m_minRadius, m_maxRadius);
				Point2F pos(w32randf(radius, m_screenWidth - radius), w32randf(radius, m_screenHeight - radius));

				m_shapes.push_back(new MovingEllipse(
					pos,
					radius,
					speed,
					direction,
					color));
			}
		}
		return true;
	}

	bool Update(ULONGLONG tick, const Point2F& mouse) {
		D2D1_RECT_U rectBounds;
		rectBounds.left = rectBounds.top = 0;
		rectBounds.right = m_screenWidth;
		rectBounds.bottom = m_screenHeight;
		auto it = m_shapes.begin();
		while (it != m_shapes.end()) {
			// Move the shape
			switch ((*it)->Move(&rectBounds)) {
			case Position::moveResult::hitboundsleft:
				(*it)->BounceX();
				break;
			case Position::moveResult::hitboundsright:
				(*it)->BounceX();
				break;
			case Position::moveResult::hitboundstop:
				(*it)->BounceY();
				break;
			case Position::moveResult::hitboundsbottom:
				(*it)->BounceY();
				break;
			}

			// Is the mouse touching it?
			if ((*it)->HitTest(mouse)) {
				// Remove the shape and go to the next
				(*it)->D2DDiscardResources();
				it = m_shapes.erase(it++);
			}
			else {
				// Next shape
				++it;
			}
		}
		return true;
	}

	bool Render(ID2D1HwndRenderTarget* pRenderTarget, const D2DRectScaler* pRsFAR) {
		for (auto& shape : m_shapes) {
			shape->Draw(pRenderTarget, pRsFAR);
		}
		return true;
	}

protected:
	std::vector<Shape*> m_shapes;
};