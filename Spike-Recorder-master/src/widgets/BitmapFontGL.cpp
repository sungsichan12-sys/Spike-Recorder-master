#include "BitmapFontGL.h"
#include "Log.h"
#include <cstring>
#include <string>
#include "UIFontMetrics.h"
#ifdef _WIN32
#include <windows.h>
#include <map>
#include <vector>
#endif

namespace BackyardBrains {

namespace Widgets {

// Half-texel inset keeps linear filtering inside each atlas cell.
static const float glyphU = 31.f / 512.f;
static const float glyphV = 63.f / 1024.f;
static const int strideX = 8;
static const int strideY = 16;

#ifdef _WIN32
namespace {
struct NativeText { GLuint texture; int width, height, textureWidth, textureHeight; };

// Device names arrive as UTF-8 or the Windows system code page. Keep the
// original scientific-symbol atlas for strings that are neither encoding.
bool drawNativeText(const char *text, int x, int y, Alignment alignment)
{
    bool extended = false;
    for(const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p)
        extended = extended || *p >= 128;
    if(!extended || std::strchr(text, '\n')) return false;
    UINT codePage = CP_UTF8;
    int count = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if(!count) { codePage = CP_ACP; count = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, text, -1, NULL, 0); }
    if(count <= 1) return false;
    static std::map<std::string, NativeText> cache;
    static HGLRC owner = NULL;
    if(owner != wglGetCurrentContext()) { cache.clear(); owner = wglGetCurrentContext(); }
    auto found = cache.find(text);
    if(found == cache.end()) {
        std::vector<wchar_t> wide(count);
        MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, text, -1, wide.data(), count);
        HDC dc = CreateCompatibleDC(NULL);
        if(!dc) return false;
        HFONT font = CreateFontW(-26,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        if(!font) { DeleteDC(dc); return false; }
        HGDIOBJ oldFont = SelectObject(dc,font);
        SIZE size = {};
        GetTextExtentPoint32W(dc,wide.data(),count-1,&size);
        int tw = 1, th = 64;
        while(tw < size.cx + 4 && tw < 4096) tw *= 2;
        if(size.cx <= 0 || size.cx + 4 > tw) {
            SelectObject(dc,oldFont); DeleteObject(font); DeleteDC(dc); return false;
        }
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = tw; info.bmiHeader.biHeight = -th;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void *pixels = NULL;
        HBITMAP bitmap = CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);
        if(!bitmap) { SelectObject(dc,oldFont); DeleteObject(font); DeleteDC(dc); return false; }
        HGDIOBJ oldBitmap = SelectObject(dc,bitmap);
        std::memset(pixels,0,tw*th*4);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
        TextOutW(dc,0,0,wide.data(),count-1);
        GdiFlush();
        unsigned char *bytes = static_cast<unsigned char *>(pixels);
        for(int i=0; i<tw*th; ++i) {
            const unsigned char coverage = bytes[i*4];
            bytes[i*4] = bytes[i*4+1] = bytes[i*4+2] = 255;
            bytes[i*4+3] = coverage;
        }
        if(cache.size() >= 128) {
            for(const auto &entry : cache) glDeleteTextures(1,&entry.second.texture);
            cache.clear();
        }
        NativeText value = {0, size.cx, std::min(32, static_cast<int>(size.cy)), tw, th};
        glGenTextures(1,&value.texture); glBindTexture(GL_TEXTURE_2D,value.texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,tw,th,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        SelectObject(dc,oldBitmap); DeleteObject(bitmap);
        SelectObject(dc,oldFont); DeleteObject(font); DeleteDC(dc);
        found = cache.insert(std::make_pair(std::string(text),value)).first;
    }
    const NativeText &value = found->second;
    if(alignment & AlignHCenter) x -= value.width/4;
    else if(alignment & AlignRight) x -= value.width/2;
    if(alignment & AlignVCenter) y -= 8;
    else if(alignment & AlignBottom) y -= 16;
    const float u = value.width / static_cast<float>(value.textureWidth);
    const float v = value.height / static_cast<float>(value.textureHeight);
    glBindTexture(GL_TEXTURE_2D,value.texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(x,y);
    glTexCoord2f(0,v); glVertex2f(x,y+value.height/2.f);
    glTexCoord2f(u,v); glVertex2f(x+value.width/2.f,y+value.height/2.f);
    glTexCoord2f(u,0); glVertex2f(x+value.width/2.f,y);
    glEnd(); glBindTexture(GL_TEXTURE_2D,0);
    return true;
}
}
#endif

BitmapFontGL::BitmapFontGL() {
	Log::msg("Initializing font...");
	characters = TextureGL::get("data/ui-font.bmp");
}

int BitmapFontGL::characterWidth() const
{
	return strideX;
}

int BitmapFontGL::characterHeight() const
{
	return strideY;
}

int BitmapFontGL::textWidth(const char *text) const
{
#ifdef _WIN32
    bool extended=false;
    for(const unsigned char *p=reinterpret_cast<const unsigned char *>(text);*p;++p)
        extended=extended || *p>=128;
    if(extended) {
        UINT page=CP_UTF8;
        int count=MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,text,-1,NULL,0);
        if(!count) {page=CP_ACP;count=MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,text,-1,NULL,0);}
        if(count>1) {
            std::vector<wchar_t> wide(count);
            MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,text,-1,wide.data(),count);
            HDC dc=CreateCompatibleDC(NULL);
            HFONT font=CreateFontW(-26,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
            if(dc && font) {
                HGDIOBJ old=SelectObject(dc,font); SIZE size={};
                const bool measured=GetTextExtentPoint32W(dc,wide.data(),count-1,&size)!=0;
                SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);
                if(measured) return (size.cx+1)/2;
            } else {if(font) DeleteObject(font);if(dc) DeleteDC(dc);}
        }
    }
#endif
    float width=0;
    for(const unsigned char *p=reinterpret_cast<const unsigned char *>(text);*p;++p)
        width+=*p>=128?strideX:uiFontAdvances[*p]/4.f;
    return static_cast<int>(width+.5f);
}

// TODO use display list calling directly of the text and strlen(text) to accelerate drawing all of the quads
void BitmapFontGL::draw(const char *text, int xx, int yy, Alignment alignment) const
{
#ifdef _WIN32
	if(drawNativeText(text,xx,yy,alignment)) return;
#endif
	drawText(text, xx, yy, alignment, false);
}

void BitmapFontGL::drawFixed(const char *text, int xx, int yy, Alignment alignment) const
{
	drawText(text, xx, yy, alignment, true);
}

void BitmapFontGL::drawText(const char *text, int xx, int yy, Alignment alignment, bool fixed) const
{
	// align on first line
	const char *lb = strchr(text,'\n');
	int len;
	if(lb == 0) {
		len = strlen(text);
	} else {
		len = lb-text;
	}

	float textWidth = 0;
	for(int i=0;i<len;++i) {
		const unsigned char c = static_cast<unsigned char>(text[i]);
		textWidth += fixed || c >= 128 ? strideX : uiFontAdvances[c] / 4.f;
	}
	if (alignment & AlignHCenter)
		xx -= static_cast<int>(textWidth / 2.f);
	else if (alignment & AlignRight)
		xx -= static_cast<int>(textWidth);
	if (alignment & AlignVCenter)
		yy -= strideY/2;
	else if (alignment & AlignBottom)
		yy -= strideY;
	
	float x = xx;
	int y = yy;

	characters->bind();
	glBegin(GL_QUADS);
	for (; *text; ++text)
	{
		if (*text == '\n')
		{
			x = xx;
			y += strideY;
		}
		else
		{
			const unsigned char c = static_cast<unsigned char>(*text);
			const float advance = fixed || c >= 128 ? strideX : uiFontAdvances[c] / 4.f;
			const float left = x - (strideX - advance) / 2.f;
			float texX = static_cast<float>(*text & 0xF)/16.0 + .5f/512.f;
			float texY = static_cast<float>(static_cast<unsigned char>(*text) >> 4)/16.0 + .5f/1024.f;
			glTexCoord2f(texX, texY);
			glVertex2f(left, y);

			glTexCoord2f(texX, texY + glyphV);
			glVertex2f(left, y + strideY);

			glTexCoord2f(texX + glyphU, texY + glyphV);
			glVertex2f(left + strideX, y + strideY);

			glTexCoord2f(texX + glyphU, texY);
			glVertex2f(left + strideX, y);
			x += advance;
		}

	}
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
}

void BitmapFontGL::drawMultiline(const char *t, int x, int y, int width, Alignment alignment) const {
	std::string text(t);
	
	std::string::iterator lstart = text.begin();
	std::string::iterator lbreak = text.begin();
	int i = 0;
	int line = 0;
	int linelen = std::max(1,width/characterWidth());

	for(std::string::iterator it = text.begin(); it != text.end(); it++) {
		if(*it == ' ' || *it == '\n') {
			lbreak = it;
		}

		if(i >= linelen || *it == '\n') {
			if(lbreak < lstart) {
				lbreak = it;
			}
			draw(std::string(lstart, lbreak).c_str(), x, y+line*(characterHeight()+2), alignment);
			lstart = lbreak+1;
			i = it-lstart;
			line++;
		} else {
			i++;
		}
	}

	draw(std::string(lstart, text.end()).c_str(), x, y+line*(characterHeight()+2), alignment);
}

} // namespace Widgets

} // namespace BackyardBrains
