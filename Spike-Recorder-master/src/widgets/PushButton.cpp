#include "PushButton.h"
#include "Painter.h"
#include "BitmapFontGL.h"
#include "Application.h"
#include <cmath>

namespace BackyardBrains {

namespace Widgets {

namespace {
// Draw the frequently used controls at their actual size. Device illustrations
// still use their original textures; functional icons share one stroke weight.
bool drawControl(const TextureGL *texture, int w, int h, bool hover)
{
    if(!texture) return false;
    const std::string &path = texture->sourcePath();
    const std::string name = path.substr(path.find_last_of("/\\") + 1);
    if(name == "ckboxoff.bmp" || name == "ckboxon.bmp") {
        const bool checked = name == "ckboxoff.bmp"; // Legacy resource names are inverted.
        glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT);
        glDisable(GL_TEXTURE_2D);
        Painter::setColor(checked ? Colors::lightblue : hover ? Colors::buttonhigh : Colors::border);
        Painter::drawRoundedRect(Rect(1,1,w-2,h-2),4);
        if(checked) {
            Painter::setColor(Colors::background);
            glLineWidth(2.f);
            glBegin(GL_LINE_STRIP); glVertex2f(w*.24f,h*.5f); glVertex2f(w*.44f,h*.69f); glVertex2f(w*.78f,h*.29f); glEnd();
        } else {
            Painter::setColor(Colors::widgetbgdark);
            Painter::drawRoundedRect(Rect(2,2,w-4,h-4),3);
        }
        glPopAttrib();
        return true;
    }
    const char *text = NULL;
    if(name.find("preset-plant") == 0) text = "Plant";
    else if(name.find("preset-neuron") == 0) text = "Neuron";
    else if(name.find("preset-eeg") == 0) text = "EEG";
    else if(name.find("preset-ecg") == 0) text = "ECG";
    else if(name.find("preset-emg") == 0) text = "EMG";
    else if(name.find("calibratebtn") == 0) text = "Calibrate";
    else if(name.find("cancel-") == 0) text = "Cancel";
    else if(name.find("okbtn-") == 0) text = "Done";
    else if(name == "disconnected.bmp" || name == "bdisconnected.bmp") text = "Connect";
    else if(name == "connected.bmp" || name == "bconnected.bmp") text = "Disconnect";
    else if(name == "update.bmp" || name == "bupdate.bmp") text = "Update";
    if(text) {
        glPushAttrib(GL_ENABLE_BIT);
        glDisable(GL_TEXTURE_2D);
        Painter::setColor(hover ? Colors::buttonhigh : Colors::button);
        Painter::drawRoundedRect(Rect(2,2,w-4,h-4),2);
        glEnable(GL_TEXTURE_2D);
        Painter::setColor(Colors::white);
        Application::font()->draw(text,w/2,h/2,AlignCenter);
        glPopAttrib();
        return true;
    }
    int kind = -1;
    const char *label = "";
    if(name == "configcrossed.bmp" || name == "analysiscrossed.bmp") { kind = 11; label = "Close"; }
    else if(name.find("config") == 0) { kind = 0; label = "Setup"; }
    else if(name.find("thresh") == 0) { kind = 1; label = "Trigger"; }
    else if(name.find("fft") == 0) { kind = 2; label = "FFT"; }
    else if(name == "rec.bmp" || name == "rechigh.bmp") { kind = 3; label = "Record"; }
    else if(name.find("file") == 0) { kind = 4; label = "Open"; }
    else if(name.find("pause") == 0) kind = 5;
    else if(name.find("play") == 0) kind = 6;
    else if(name.find("backward") == 0) kind = 7;
    else if(name.find("forward") == 0) kind = 8;
    else if(name.find("analysis") == 0) { kind = 9; label = "Analyze"; }
    else if(name.find("save") == 0) { kind = 10; label = "Save"; }
    if(kind < 0) return false;
    const bool selected = kind != 11 && name.find("crossed") != std::string::npos;
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT);
    glDisable(GL_TEXTURE_2D);
    if(selected || hover) {
        Painter::setColor(selected ? Colors::selectedSurface : Colors::buttonhigh);
        Painter::drawRoundedRect(Rect(2, 2, w - 4, h - 4), 2);
    }
    Painter::setColor(kind == 3 ? Color(225, 119, 120) : selected ? Colors::lightblue : Colors::white);
    glPushMatrix();
    const bool caption = *label && h >= 44;
    glTranslatef(w / 2.f, caption ? 16.f : h / 2.f, 0.f);
    glScalef(.85f,.85f,1.f);
    glLineWidth(1.5f);
    auto line = [](float x, float y, float x2, float y2) {
        glBegin(GL_LINES); glVertex2f(x,y); glVertex2f(x2,y2); glEnd();
    };
    if(kind == 0) {
        for(int i = 0; i < 3; ++i) {
            const int y = i * 7 - 7, x = i == 1 ? 4 : -3;
            line(-10,y,10,y);
            Painter::drawRect(Rect(x-2,y-3,4,6));
        }
    } else if(kind == 1) {
        line(-11,4,-5,4); line(-5,4,-1,-9); line(-1,-9,3,8); line(3,8,6,0); line(6,0,11,0);
        Painter::setColor(Colors::mutedText); line(-11,-3,11,-3);
    } else if(kind == 2 || kind == 9) {
        for(int i=0;i<5;++i) { int v = (i==2 ? 20 : i%2 ? 13 : 7); Painter::drawRect(Rect(i*5-11,10-v,3,v)); }
    } else if(kind == 3) {
        Painter::drawCircle(Point(0,0),7,32);
    } else if(kind == 4) {
        glBegin(GL_LINE_LOOP); glVertex2i(-10,-7); glVertex2i(-3,-7); glVertex2i(0,-3); glVertex2i(10,-3); glVertex2i(10,8); glVertex2i(-10,8); glEnd();
    } else if(kind == 5) {
        Painter::drawRect(Rect(-7,-9,4,18)); Painter::drawRect(Rect(3,-9,4,18));
    } else if(kind == 6) {
        Painter::drawTriangle(-6,-10,-6,10,9,0);
    } else if(kind == 7 || kind == 8) {
        const int d = kind == 7 ? -1 : 1;
        line(-5*d,-7,3*d,0); line(3*d,0,-5*d,7); line(8*d,-7,8*d,7);
    } else if(kind == 10) {
        Painter::drawRect(Rect(-9,-9,18,18),false);
        Painter::drawRect(Rect(-5,2,10,7),false); line(-4,-8,-4,-2); line(-4,-2,5,-2);
    } else if(kind == 11) {
        line(-7,-7,7,7); line(-7,7,7,-7);
    }
    glPopMatrix();
    glEnable(GL_TEXTURE_2D);
    if(caption) {
        Painter::setColor(selected || hover ? Colors::white : Colors::mutedText);
        glPushMatrix(); glTranslatef(w/2.f,31.f,0.f); glScalef(.875f,.875f,1.f);
        Application::font()->draw(label,0,0,AlignHCenter);
        glPopMatrix();
    }
    glPopAttrib();
    return true;
}
}

PushButton::PushButton(Widget *parent) : Widget(parent), _normaltex(NULL), _hovertex(NULL), _hover(false)
{
	setSizeHint(Size(48,48));
}

void PushButton::setSize(Size newSize)
{
    setSizeHint(newSize);
}

void PushButton::paintEvent()
{
	if(drawControl(_normaltex, width() - _rightPadding, height(), _hover)) return;
	if(_normaltex != NULL) {
		if(_hover && _hovertex != NULL)
			_hovertex->bind();
		else
			_normaltex->bind();

		Painter::setColor(Colors::white);
        if(_rightPadding>0)
        {
            Painter::drawTexRect(Rect(0, 0, width()-_rightPadding, height()));
        }
        else
        {
            Painter::drawTexRect(rect());
        }
		glBindTexture(GL_TEXTURE_2D, 0);
	} else {
		Painter::setColor(_hover ? Colors::buttonhigh : Colors::button);
		Painter::drawCircle(rect().center(), rect().width()/2, 15);
	}
}

void PushButton::setNormalTex(const TextureGL *tex) {
	_normaltex = tex;
}

void PushButton::setRightPadding(int padding)
{
    _rightPadding = padding;
}

void PushButton::setHoverTex(const TextureGL *tex) {
	_hovertex = tex;
}

void PushButton::mousePressEvent(MouseEvent *event)
{
	event->accept();
}

void PushButton::mouseReleaseEvent(MouseEvent *event)
{
	if(event->button() == LeftButton && _hover)
    {
		clicked.emit();
        clickedWithRef(event, this);
    }
}

void PushButton::mouseMotionEvent(MouseEvent *event)
{
}

void PushButton::enterEvent() {
	_hover = true;
}

void PushButton::leaveEvent() {
	_hover = false;
}

} // namespace Widgets

} // namespace BackyardBrains
