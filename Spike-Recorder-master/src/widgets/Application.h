#ifndef BACKYARDBRAINS_WIDGETS_APPLICATION_H
#define BACKYARDBRAINS_WIDGETS_APPLICATION_H

#include "Widget.h"
#include "Size.h"
#include <list>
#include <string>
#include <SDL.h>

namespace BackyardBrains {

namespace Widgets {

class Application : public sigslot::has_slots<>
{
public:
	static const int MIN_WINDOW_W;
	static const int MIN_WINDOW_H;

	Application();
	virtual ~Application();

	void run();

	void setWindowTitle(const std::string &str);
	std::string windowTitle() const;

	Widget *&keyboardGrabber();
	Widget *&mouseGrabber();
	Widget *&hoverWidget();
	void setFocusWidget(Widget *widget, bool captureKeyboard=false);

	void addPopup(Widget *w);
	void addWindow(Widget *w);

	void updateLayout();

	static const BitmapFontGL *font();
	static Application *getInstance();

	bool areWeOnTouchscreen(){return _lastMouseClickWasOnTouchscreen;}
    void getWindowSize(int *w, int *h);
protected:
	void createWindow(int w, int h);
    void closeAllWindows();
private:
	static Application *app;
	typedef std::vector<Widget*> WidgetVector;
	typedef std::list<Widget*> WidgetList;

	SDL_Window *sdlWindow = NULL;
	SDL_GLContext sdlGLContext = NULL;

	bool _running;
	Widget *_mouseGrabber;
	Widget *_keyboardGrabber;
	Widget *_hoverWidget;
	Widget *_widgetInFocus = NULL;
	bool _focusCapturesKeyboard = false;
	MouseButtons _buttonState = 0;
	WidgetList _windowStack;
	WidgetList _popupStack;
    bool setWindowSize = false;
    float remW = 0;
    float remH = 0;
	static BitmapFontGL *_font;

	virtual void advance();
	virtual void keyPressEvent(KeyboardEvent *event);
	virtual void keyReleaseEvent(KeyboardEvent *event);

	void _HandleEvent(const void *eventRaw);
	Widget * _GetWidgetAt(const Point &point) const;

	void _SetHoverWidget(Widget *widget);
	void removeClosed(WidgetList &w);

	std::string _windowTitle;

    //used to keep track of multifingers gestures
    SDL_Event _fingerMoveOne = {};
    SDL_Event _fingerMoveTwo = {};
    void keepTrackOfFingerMoves(const SDL_Event &event );
    int twoFingersGestureDirection();
    bool weAreInTwoFingersGesture = false; //true during 2 fingers gesture
    bool _lastMouseClickWasOnTouchscreen = false; //useful to change UI when user uses touchscreen
};

} // namespace Widgets

} // namespace BackyardBrains

#endif
