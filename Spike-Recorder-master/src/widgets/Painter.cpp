#include "Painter.h"

#include <SDL_opengl.h>

#include <cmath>

namespace BackyardBrains {

namespace Widgets {

Painter::Painter()
{
}

Painter::~Painter()
{
}

void Painter::setColor(const Color &color)
{
	// glColor3b(color.r, color.b, color.g);
	glColor4f(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

void Painter::drawRect(const Rect &rect, bool filled)
{
	glBegin(filled ? GL_QUADS : GL_LINE_LOOP);
	drawRectFast(rect);
	glEnd();
}

void Painter::drawRoundedRect(const Rect &rect, int radius)
{
	const int r = std::max(0, std::min(radius, std::min(rect.width(), rect.height()) / 2));
	glBegin(GL_TRIANGLE_FAN);
	glVertex2f(rect.left() + rect.width() / 2.f, rect.top() + rect.height() / 2.f);
	// Match drawRect's winding: the UI projection flips the Y axis and
	// back-face culling is enabled by Application.
	for(int c = 3; c >= 0; --c) {
		const float cx = (c == 0 || c == 3) ? rect.right() - r : rect.left() + r;
		const float cy = c < 2 ? rect.bottom() - r : rect.top() + r;
		for(int step = 8; step >= 0; --step) {
			const float angle = (c * 90.f + step * 90.f / 8.f) * M_PI / 180.f;
			glVertex2f(cx + std::cos(angle) * r, cy + std::sin(angle) * r);
		}
	}
	glVertex2f(rect.right(), rect.top() + r);
	glEnd();
}
    
void Painter::drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, bool filled)
{
    glBegin(filled ? GL_TRIANGLES : GL_LINE_LOOP);
    glVertex2i(x1, y1);
    glVertex2i(x2, y2);
    glVertex2i(x3, y3);
    glEnd();
}

void Painter::drawTexRect(const Rect &rect) {
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2i(rect.left(), rect.top());
	glTexCoord2f(0, 1);
	glVertex2i(rect.left(), rect.bottom());
	glTexCoord2f(1, 1);
	glVertex2i(rect.right(), rect.bottom());
	glTexCoord2f(1, 0);
	glVertex2i(rect.right(), rect.top());
	glEnd();
}

void Painter::drawRectFast(const Rect &rect)
{
	glVertex2i(rect.left(), rect.top());
	glVertex2i(rect.left(), rect.bottom());
	glVertex2i(rect.right(), rect.bottom());
	glVertex2i(rect.right(), rect.top());
}
    


void Painter::drawCircle(const Point &center, int radius, int points)
{
	glBegin(GL_TRIANGLE_FAN);
	glVertex2i(center.x, center.y);
	for (int i = 0; i <= points; i++)
	{
		glVertex2f(center.x + cos(i*2.0*M_PI/points)*radius, center.y - sin(i*2.0*M_PI/points)*radius);
	}
	glEnd();
}

} // namespace Widgets

} // namespace BackyardBrains
