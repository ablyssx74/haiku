#include "XfwmTheme.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <new>
#include <string>

#include <Entry.h>

#include "ServerBitmap.h"


// #pragma mark - XPM reading


namespace {

struct XpmColor {
	std::string	key;
	uint8		red, green, blue, alpha;
};


bool
ParseHexColor(const std::string& text, uint8& red, uint8& green, uint8& blue)
{
	if (text.size() < 4 || text[0] != '#')
		return false;
	size_t digits = text.size() - 1;
	if (digits % 3 != 0)
		return false;
	size_t each = digits / 3;
	if (each < 1 || each > 4)
		return false;
	unsigned long values[3];
	for (int i = 0; i < 3; i++) {
		std::string part = text.substr(1 + i * each, each);
		values[i] = strtoul(part.c_str(), NULL, 16);
		// scale to 8 bits
		if (each == 1)
			values[i] *= 17;
		else if (each == 3)
			values[i] >>= 4;
		else if (each == 4)
			values[i] >>= 8;
	}
	red = (uint8)values[0];
	green = (uint8)values[1];
	blue = (uint8)values[2];
	return true;
}


void
NamedColor(const std::string& name, uint8& red, uint8& green, uint8& blue)
{
	std::string lower;
	for (size_t i = 0; i < name.size(); i++) {
		if (name[i] != ' ')
			lower += (char)tolower((unsigned char)name[i]);
	}
	struct { const char* name; uint8 r, g, b; } kNames[] = {
		{"black", 0, 0, 0}, {"white", 255, 255, 255}, {"red", 255, 0, 0}, {"green", 0, 255, 0},
		{"blue", 0, 0, 255}, {"yellow", 255, 255, 0}, {"cyan", 0, 255, 255}, {"magenta", 255, 0, 255},
		{"gray", 190, 190, 190}, {"grey", 190, 190, 190}, {"lightgray", 211, 211, 211},
		{"lightgrey", 211, 211, 211}, {"darkgray", 169, 169, 169}, {"darkgrey", 169, 169, 169},
		{"orange", 255, 165, 0}, {"brown", 165, 42, 42}, {"pink", 255, 192, 203},
		{"purple", 160, 32, 240}, {"navy", 0, 0, 128}, {"darkblue", 0, 0, 139}, {"darkgreen", 0, 100, 0}
	};
	for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++) {
		if (lower == kNames[i].name) {
			red = kNames[i].r;
			green = kNames[i].g;
			blue = kNames[i].b;
			return;
		}
	}
	// "grayNN" / "greyNN"
	if ((lower.compare(0, 4, "gray") == 0 || lower.compare(0, 4, "grey") == 0) && lower.size() > 4) {
		int level = atoi(lower.c_str() + 4);
		red = green = blue = (uint8)(level * 255 / 100);
		return;
	}
	red = green = blue = 0;
}

}	// namespace


// #pragma mark - XfwmImage


XfwmImage::XfwmImage()
	:
	fBitmap(NULL),
	fWidth(0),
	fHeight(0),
	fBounds(0, 0, -1, -1)
{
}


XfwmImage::~XfwmImage()
{
	if (fBitmap != NULL)
		fBitmap->ReleaseReference();
}


bool
XfwmImage::Load(const char* path)
{
	FILE* file = fopen(path, "r");
	if (file == NULL)
		return false;

	std::string text;
	char buffer[4096];
	size_t got;
	while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
		text.append(buffer, got);
	fclose(file);

	// the quoted strings, in order
	std::vector<std::string> strings;
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*') {
			size_t end = text.find("*/", i + 2);
			if (end == std::string::npos)
				break;
			i = end + 1;
			continue;
		}
		if (text[i] != '"')
			continue;
		std::string value;
		for (i++; i < text.size() && text[i] != '"'; i++) {
			if (text[i] == '\\' && i + 1 < text.size())
				i++;
			value += text[i];
		}
		strings.push_back(value);
	}
	if (strings.empty())
		return false;

	int width = 0, height = 0, colorCount = 0, charsPerPixel = 0;
	if (sscanf(strings[0].c_str(), "%d %d %d %d", &width, &height, &colorCount, &charsPerPixel) != 4
		|| width <= 0 || height <= 0 || colorCount <= 0 || charsPerPixel <= 0 || charsPerPixel > 4
		|| width > 4096 || height > 4096
		|| (int)strings.size() < 1 + colorCount + height) {
		return false;
	}

	std::vector<XpmColor> colors;
	for (int i = 0; i < colorCount; i++) {
		const std::string& line = strings[1 + i];
		XpmColor color;
		color.key = line.substr(0, charsPerPixel);
		color.red = color.green = color.blue = 0;
		color.alpha = 255;

		// "<key> c <colour>" (other visuals -- m, g, g4, s -- are ignored)
		std::vector<std::string> tokens;
		std::string current;
		for (size_t k = charsPerPixel; k <= line.size(); k++) {
			if (k == line.size() || line[k] == ' ' || line[k] == '\t') {
				if (!current.empty())
					tokens.push_back(current);
				current.clear();
			} else
				current += line[k];
		}
		std::string value;
		bool found = false;
		for (size_t t = 0; t + 1 < tokens.size() && !found; t++) {
			if (tokens[t] == "c") {
				for (size_t u = t + 1; u < tokens.size(); u++) {
					if (tokens[u] == "m" || tokens[u] == "s" || tokens[u] == "g" || tokens[u] == "g4")
						break;
					if (!value.empty())
						value += " ";
					value += tokens[u];
				}
				found = true;
			}
		}
		if (!found && tokens.size() >= 2)
			value = tokens[1];

		std::string lower;
		for (size_t k = 0; k < value.size(); k++)
			lower += (char)tolower((unsigned char)value[k]);
		if (lower == "none" || lower.empty())
			color.alpha = 0;
		else if (!ParseHexColor(value, color.red, color.green, color.blue))
			NamedColor(value, color.red, color.green, color.blue);
		colors.push_back(color);
	}

	UtilityBitmap* bitmap = new(std::nothrow) UtilityBitmap(BRect(0, 0, width - 1, height - 1), B_RGBA32, 0);
	if (bitmap == NULL)
		return false;
	if (!bitmap->IsValid()) {
		delete bitmap;
		return false;
	}

	fRuns.clear();
	int32 minX = width, minY = height, maxX = -1, maxY = -1;
	for (int y = 0; y < height; y++) {
		const std::string& row = strings[1 + colorCount + y];
		uint8* out = bitmap->Bits() + y * bitmap->BytesPerRow();
		int32 runStart = -1;
		for (int x = 0; x < width; x++) {
			uint8 blue = 0, green = 0, red = 0, alpha = 0;
			if ((int)row.size() >= (x + 1) * charsPerPixel) {
				for (size_t c = 0; c < colors.size(); c++) {
					if (row.compare(x * charsPerPixel, charsPerPixel, colors[c].key) == 0) {
						red = colors[c].red;
						green = colors[c].green;
						blue = colors[c].blue;
						alpha = colors[c].alpha;
						break;
					}
				}
			}
			out[x * 4 + 0] = blue;
			out[x * 4 + 1] = green;
			out[x * 4 + 2] = red;
			out[x * 4 + 3] = alpha;

			if (alpha > 0) {
				if (runStart < 0)
					runStart = x;
				if (x < minX)
					minX = x;
				if (x > maxX)
					maxX = x;
				if (y < minY)
					minY = y;
				if (y > maxY)
					maxY = y;
			} else if (runStart >= 0) {
				Run run = {y, runStart, x - 1};
				fRuns.push_back(run);
				runStart = -1;
			}
		}
		if (runStart >= 0) {
			Run run = {y, runStart, width - 1};
			fRuns.push_back(run);
		}
	}

	if (fBitmap != NULL)
		fBitmap->ReleaseReference();
	fBitmap = bitmap;
	fWidth = width;
	fHeight = height;
	fBounds = maxX >= 0 ? BRect(minX, minY, maxX, maxY) : BRect(0, 0, -1, -1);
	return true;
}


bool
XfwmImage::LoadFlipped(const XfwmImage& source)
{
	if (!source.IsValid())
		return false;

	UtilityBitmap* bitmap = new(std::nothrow) UtilityBitmap(BRect(0, 0, source.fWidth - 1, source.fHeight - 1),
		B_RGBA32, 0);
	if (bitmap == NULL)
		return false;
	if (!bitmap->IsValid()) {
		delete bitmap;
		return false;
	}

	fRuns.clear();
	int32 minX = source.fWidth, minY = source.fHeight, maxX = -1, maxY = -1;
	for (int32 y = 0; y < source.fHeight; y++) {
		const uint8* in = source.fBitmap->Bits() + (source.fHeight - 1 - y) * source.fBitmap->BytesPerRow();
		uint8* out = bitmap->Bits() + y * bitmap->BytesPerRow();
		memcpy(out, in, source.fWidth * 4);

		int32 runStart = -1;
		for (int32 x = 0; x < source.fWidth; x++) {
			if (out[x * 4 + 3] > 0) {
				if (runStart < 0)
					runStart = x;
				minX = std::min(minX, x);
				maxX = std::max(maxX, x);
				minY = std::min(minY, y);
				maxY = std::max(maxY, y);
			} else if (runStart >= 0) {
				Run run = {y, runStart, x - 1};
				fRuns.push_back(run);
				runStart = -1;
			}
		}
		if (runStart >= 0) {
			Run run = {y, runStart, source.fWidth - 1};
			fRuns.push_back(run);
		}
	}

	if (fBitmap != NULL)
		fBitmap->ReleaseReference();
	fBitmap = bitmap;
	fWidth = source.fWidth;
	fHeight = source.fHeight;
	fBounds = maxX >= 0 ? BRect(minX, minY, maxX, maxY) : BRect(0, 0, -1, -1);
	return true;
}


void
XfwmImage::IncludeIn(BRegion& region, int32 x, int32 y) const
{
	for (size_t i = 0; i < fRuns.size(); i++)
		region.Include(BRect(x + fRuns[i].left, y + fRuns[i].row, x + fRuns[i].right, y + fRuns[i].row));
}


// #pragma mark - XfwmTheme


static const char* const kThemeFolders[] = {
	"/boot/home/config/non-packaged/data/xfwm4-themes",
	"/boot/home/config/data/xfwm4-themes",
	"/boot/system/non-packaged/data/xfwm4-themes",
	"/boot/system/data/xfwm4-themes"
};


XfwmTheme::XfwmTheme()
	:
	fValid(false),
	fBorderWidth(5),
	fLeftMargin(0),
	fTitleHeight(24),
	fButtonWidth(12),
	fOffsetActive(0),
	fOffsetInactive(0),
	fAlignment(0),
	fButtonOffset(0),
	fButtonSpacing(0),
	fButtonLayout("O|HMC")
{
	fOutline.red = fOutline.green = fOutline.blue = 0;
	fOutline.alpha = 255;
	fActiveText.red = fActiveText.green = fActiveText.blue = 0;
	fActiveText.alpha = 255;
	fInactiveText = fActiveText;
}


bool
XfwmTheme::_FindFolder(const char* name, BString& path) const
{
	for (size_t i = 0; i < sizeof(kThemeFolders) / sizeof(kThemeFolders[0]); i++) {
		BString candidate(kThemeFolders[i]);
		candidate << "/" << name;
		BString probe(candidate);
		probe << "/themerc";
		if (BEntry(probe.String()).Exists()) {
			path = candidate;
			return true;
		}
	}
	return false;
}


bool
XfwmTheme::Load(const char* name)
{
	fValid = false;
	BString folder;
	if (!_FindFolder(name, folder))
		return false;

	_ReadThemerc(BString(folder).Append("/themerc").String());

	struct Piece {
		XfwmImage*	image;
		const char*	file;
	};

	for (int32 state = 0; state < 2; state++) {
		const char* suffix = state == 0 ? "active" : "inactive";
		struct { XfwmImage* image; const char* base; } parts[] = {
			{&fTopLeft[state], "top-left"}, {&fTopRight[state], "top-right"},
			{&fTitle[0][state], "title-1"}, {&fTitle[1][state], "title-2"}, {&fTitle[2][state], "title-3"},
			{&fTitle[3][state], "title-4"}, {&fTitle[4][state], "title-5"},
			{&fLeft[state], "left"}, {&fRight[state], "right"}, {&fBottom[state], "bottom"},
			{&fBottomLeft[state], "bottom-left"}, {&fBottomRight[state], "bottom-right"}
		};
		for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
			BString path(folder);
			path << "/" << parts[i].base << "-" << suffix << ".xpm";
			if (!parts[i].image->Load(path.String()) && state == 1) {
				// no inactive picture: use the active one
				BString active(folder);
				active << "/" << parts[i].base << "-active.xpm";
				parts[i].image->Load(active.String());
			}
		}
	}

	static const char* const kButtonNames[kButtonCount] = {"close", "maximize", "hide", "shade"};
	static const char* const kStateNames[kStateCount] = {"active", "inactive", "pressed"};
	for (int32 button = 0; button < kButtonCount; button++) {
		for (int32 state = 0; state < kStateCount; state++) {
			BString path(folder);
			path << "/" << kButtonNames[button] << "-" << kStateNames[state] << ".xpm";
			if (!fButton[button][state].Load(path.String()) && state != kStateActive) {
				BString active(folder);
				active << "/" << kButtonNames[button] << "-active.xpm";
				fButton[button][state].Load(active.String());
			}
		}
	}

	// everything the decorator draws must exist
	const XfwmImage& left = fLeft[0];
	const XfwmImage& right = fRight[0];
	const XfwmImage& bottom = fBottom[0];
	if (!fTopLeft[0].IsValid() || !fTopRight[0].IsValid() || !fTitle[2][0].IsValid() || !left.IsValid()
		|| !right.IsValid() || !bottom.IsValid() || !fBottomLeft[0].IsValid() || !fBottomRight[0].IsValid()) {
		return false;
	}

	int32 leftWidth = (int32)left.OpaqueBounds().Width() + 1;
	int32 rightWidth = (int32)right.OpaqueBounds().Width() + 1;
	int32 bottomHeight = (int32)bottom.OpaqueBounds().Height() + 1;
	fBorderWidth = leftWidth;
	if (rightWidth > fBorderWidth)
		fBorderWidth = rightWidth;
	if (bottomHeight > fBorderWidth)
		fBorderWidth = bottomHeight;
	if (fBorderWidth < 1)
		fBorderWidth = 1;
	fLeftMargin = (int32)left.OpaqueBounds().left;
	fTitleHeight = fTitle[2][0].Height();
	if (fButton[kButtonClose][kStateActive].IsValid())
		fButtonWidth = fButton[kButtonClose][kStateActive].Width();

	for (int32 state = 0; state < 2; state++) {
		fTopFrame[state].LoadFlipped(fBottom[state]);
		fTopLeftCorner[state].LoadFlipped(fBottomLeft[state]);
		fTopRightCorner[state].LoadFlipped(fBottomRight[state]);
	}

	// the outer line of the border: the outermost opaque pixel of the left picture, halfway down
	{
		ServerBitmap* bitmap = left.Bitmap();
		int32 column = (int32)left.OpaqueBounds().left;
		int32 row = left.Height() / 2;
		const uint8* pixel = bitmap->Bits() + row * bitmap->BytesPerRow() + column * 4;
		fOutline.blue = pixel[0];
		fOutline.green = pixel[1];
		fOutline.red = pixel[2];
		fOutline.alpha = 255;
	}

	fValid = true;
	return true;
}


bool
XfwmTheme::ButtonsOnLeft(int32 button) const
{
	static const char kLetters[kButtonCount] = {'C', 'M', 'H', 'S'};
	int32 bar = fButtonLayout.FindFirst('|');
	int32 at = fButtonLayout.FindFirst(kLetters[button]);
	return at >= 0 && bar >= 0 && at < bar;
}


void
XfwmTheme::_ReadThemerc(const char* path)
{
	FILE* file = fopen(path, "r");
	if (file == NULL)
		return;

	char line[512];
	while (fgets(line, sizeof(line), file) != NULL) {
		char* equals = strchr(line, '=');
		if (equals == NULL)
			continue;
		*equals = '\0';
		char* key = line;
		char* value = equals + 1;
		// trim
		while (*key == ' ' || *key == '\t')
			key++;
		for (char* end = key + strlen(key); end > key && (end[-1] == ' ' || end[-1] == '\t'); end--)
			end[-1] = '\0';
		while (*value == ' ' || *value == '\t')
			value++;
		for (char* end = value + strlen(value);
			end > value && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'); end--) {
			end[-1] = '\0';
		}

		uint8 r, g, b;
		if (strcmp(key, "active_text_color") == 0 && ParseHexColor(value, r, g, b)) {
			fActiveText.red = r;
			fActiveText.green = g;
			fActiveText.blue = b;
		} else if (strcmp(key, "inactive_text_color") == 0 && ParseHexColor(value, r, g, b)) {
			fInactiveText.red = r;
			fInactiveText.green = g;
			fInactiveText.blue = b;
		} else if (strcmp(key, "title_vertical_offset_active") == 0)
			fOffsetActive = atoi(value);
		else if (strcmp(key, "title_vertical_offset_inactive") == 0)
			fOffsetInactive = atoi(value);
		else if (strcmp(key, "title_alignment") == 0)
			fAlignment = strcmp(value, "center") == 0 ? 1 : (strcmp(value, "right") == 0 ? 2 : 0);
		else if (strcmp(key, "button_layout") == 0)
			fButtonLayout = value;
		else if (strcmp(key, "button_offset") == 0)
			fButtonOffset = atoi(value);
		else if (strcmp(key, "button_spacing") == 0)
			fButtonSpacing = atoi(value);
	}
	fclose(file);
}
