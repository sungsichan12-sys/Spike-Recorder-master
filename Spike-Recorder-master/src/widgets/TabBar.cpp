#include "TabBar.h"
#include "Log.h"
#include "Application.h"
#include "BitmapFontGL.h"
#include "Color.h"
#include "Painter.h"
#include <algorithm>
#include <utility>

namespace BackyardBrains {
namespace Widgets {

TabBar::TabBar(Widget *parent) : Widget(parent), _selected(0) {
	setSizePolicy(SizePolicy(Widgets::SizePolicy::Expanding, Widgets::SizePolicy::Fixed));
	setSizeHint(Size(64,2*PADDING+Application::font()->characterHeight()));
}

void TabBar::paintEvent() {
	int cw = Application::font()->characterWidth();
	Painter::setColor(Colors::widgetbgdark);
	Painter::drawRect(rect());
	if(_entries.empty() || width() <= 0 || height() <= 0)
		return;
	int tabw = width()/static_cast<int>(_entries.size());
	if(tabw <= 0)
		return;
	Painter::setColor(Colors::button);
	Painter::drawRect(Rect(tabw*_selected, 0, tabw, height()));
	Painter::setColor(Colors::lightblue);
	Painter::drawRect(Rect(tabw*_selected, height()-3, tabw, 3));

	Painter::setColor(Colors::mutedText);
	for(unsigned int i = 0; i < _entries.size(); i++) {
		if(static_cast<int>(i) == _selected)
			Painter::setColor(Colors::white);
		else
			Painter::setColor(Colors::mutedText);
		int x = tabw*i+tabw/2;
		std::string str = _entries[i];
		int len = str.size();
		const int charactersPerTab = std::max(1, tabw / cw - 2);
		if(len > charactersPerTab) {
			len = std::max(0, charactersPerTab - 1);
			str = _entries[i].substr(0,len)+"-";
		}

		Application::font()->draw(str.c_str(),x,PADDING, AlignHCenter);
	}

}

void TabBar::setEntries(const std::vector<std::string> &entries) {
	_entries = entries;
	if(_entries.empty())
		_selected = 0;
	else if(_selected >= static_cast<int>(_entries.size()))
		_selected = static_cast<int>(_entries.size()) - 1;
}

int TabBar::selected() const {
	return _selected;
}

void TabBar::mousePressEvent(MouseEvent *event) {
	if(event->button() == LeftButton && !_entries.empty() && width() > 0) {
		int s = event->pos().x*(int)_entries.size()/width();
		s = std::max(0, std::min(s, static_cast<int>(_entries.size()) - 1));
		if(s != _selected) {
			_selected = s;
			selectionChanged.emit(s);
		}
	}
	event->accept();
}

}
}
