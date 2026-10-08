#include "XfwmTheme.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <new>
#include <set>
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


// #pragma mark - PNG reading (the themes' real artwork is a PNG where there is one; app_server loads no libraries
// for it, so the few things needed are done here: a small inflate and the five filters)


namespace {

class Inflater {
public:
	Inflater(const uint8* in, size_t inLength, uint8* out, size_t outCapacity)
		:
		fIn(in),
		fInLength(inLength),
		fInPos(0),
		fBitBuffer(0),
		fBitCount(0),
		fOut(out),
		fOutCapacity(outCapacity),
		fOutPos(0),
		fFailed(false)
	{
	}

	// a zlib stream: two header bytes, deflate blocks (the checksum is not looked at)
	bool Run()
	{
		if (fInLength < 2 || (fIn[0] & 0x0f) != 8)
			return false;
		fInPos = 2;

		bool last;
		do {
			last = _Bits(1) != 0;
			int type = _Bits(2);
			if (fFailed)
				return false;
			bool ok;
			if (type == 0)
				ok = _Stored();
			else if (type == 1)
				ok = _Fixed();
			else if (type == 2)
				ok = _Dynamic();
			else
				ok = false;
			if (!ok || fFailed)
				return false;
		} while (!last);
		return true;
	}

	size_t Produced() const { return fOutPos; }

private:
	struct Huffman {
		uint16	count[16];
		uint16	symbol[288];
	};

	int _Bits(int need)
	{
		while (fBitCount < need) {
			if (fInPos >= fInLength) {
				fFailed = true;
				return 0;
			}
			fBitBuffer |= (uint32)fIn[fInPos++] << fBitCount;
			fBitCount += 8;
		}
		int value = (int)(fBitBuffer & ((1u << need) - 1));
		fBitBuffer >>= need;
		fBitCount -= need;
		return value;
	}

	int _Decode(const Huffman& h)
	{
		int code = 0, first = 0, index = 0;
		for (int length = 1; length <= 15; length++) {
			code |= _Bits(1);
			if (fFailed)
				return -1;
			int count = h.count[length];
			if (code - count < first)
				return h.symbol[index + (code - first)];
			index += count;
			first += count;
			first <<= 1;
			code <<= 1;
		}
		return -1;
	}

	// 0: complete, > 0: incomplete, < 0: over-subscribed
	static int _Construct(Huffman& h, const uint16* lengths, int n)
	{
		for (int i = 0; i < 16; i++)
			h.count[i] = 0;
		for (int i = 0; i < n; i++)
			h.count[lengths[i]]++;
		if (h.count[0] == n)
			return 0;

		int left = 1;
		for (int length = 1; length <= 15; length++) {
			left <<= 1;
			left -= h.count[length];
			if (left < 0)
				return left;
		}

		uint16 offsets[16];
		offsets[1] = 0;
		for (int length = 1; length < 15; length++)
			offsets[length + 1] = offsets[length] + h.count[length];
		for (int symbol = 0; symbol < n; symbol++) {
			if (lengths[symbol] != 0)
				h.symbol[offsets[lengths[symbol]]++] = symbol;
		}
		return left;
	}

	bool _Codes(const Huffman& lengthCodes, const Huffman& distanceCodes)
	{
		static const uint16 kLengths[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
			67, 83, 99, 115, 131, 163, 195, 227, 258};
		static const uint16 kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
			4, 5, 5, 5, 5, 0};
		static const uint16 kDistances[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
			513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
		static const uint16 kDistanceExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
			10, 10, 11, 11, 12, 12, 13, 13};

		for (;;) {
			int symbol = _Decode(lengthCodes);
			if (symbol < 0)
				return false;
			if (symbol < 256) {
				if (fOutPos >= fOutCapacity)
					return false;
				fOut[fOutPos++] = (uint8)symbol;
			} else if (symbol == 256) {
				return true;
			} else {
				symbol -= 257;
				if (symbol >= 29)
					return false;
				int length = kLengths[symbol] + _Bits(kLengthExtra[symbol]);
				symbol = _Decode(distanceCodes);
				if (symbol < 0 || symbol >= 30)
					return false;
				size_t distance = kDistances[symbol] + _Bits(kDistanceExtra[symbol]);
				if (fFailed || distance > fOutPos || fOutPos + length > fOutCapacity)
					return false;
				for (int i = 0; i < length; i++, fOutPos++)
					fOut[fOutPos] = fOut[fOutPos - distance];
			}
		}
	}

	bool _Stored()
	{
		fBitBuffer = 0;
		fBitCount = 0;
		if (fInPos + 4 > fInLength)
			return false;
		unsigned length = fIn[fInPos] | (fIn[fInPos + 1] << 8);
		unsigned complement = fIn[fInPos + 2] | (fIn[fInPos + 3] << 8);
		fInPos += 4;
		if (length != (~complement & 0xffff) || fInPos + length > fInLength || fOutPos + length > fOutCapacity)
			return false;
		memcpy(fOut + fOutPos, fIn + fInPos, length);
		fInPos += length;
		fOutPos += length;
		return true;
	}

	bool _Fixed()
	{
		uint16 lengths[288];
		int i = 0;
		for (; i < 144; i++)
			lengths[i] = 8;
		for (; i < 256; i++)
			lengths[i] = 9;
		for (; i < 280; i++)
			lengths[i] = 7;
		for (; i < 288; i++)
			lengths[i] = 8;
		Huffman lengthCodes, distanceCodes;
		_Construct(lengthCodes, lengths, 288);
		for (i = 0; i < 30; i++)
			lengths[i] = 5;
		_Construct(distanceCodes, lengths, 30);
		return _Codes(lengthCodes, distanceCodes);
	}

	bool _Dynamic()
	{
		static const uint8 kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

		int lengthCount = _Bits(5) + 257;
		int distanceCount = _Bits(5) + 1;
		int codeCount = _Bits(4) + 4;
		if (fFailed || lengthCount > 286 || distanceCount > 30)
			return false;

		uint16 lengths[320];
		int index = 0;
		for (; index < codeCount; index++)
			lengths[kOrder[index]] = _Bits(3);
		for (; index < 19; index++)
			lengths[kOrder[index]] = 0;

		Huffman lengthCodes, distanceCodes;
		if (_Construct(lengthCodes, lengths, 19) != 0)
			return false;

		index = 0;
		while (index < lengthCount + distanceCount) {
			int symbol = _Decode(lengthCodes);
			if (symbol < 0)
				return false;
			if (symbol < 16) {
				lengths[index++] = symbol;
			} else {
				int previous = 0;
				if (symbol == 16) {
					if (index == 0)
						return false;
					previous = lengths[index - 1];
					symbol = 3 + _Bits(2);
				} else if (symbol == 17) {
					symbol = 3 + _Bits(3);
				} else {
					symbol = 11 + _Bits(7);
				}
				if (fFailed || index + symbol > lengthCount + distanceCount)
					return false;
				while (symbol-- > 0)
					lengths[index++] = previous;
			}
		}
		if (lengths[256] == 0)
			return false;

		int error = _Construct(lengthCodes, lengths, lengthCount);
		if (error != 0 && (error < 0 || lengthCount != lengthCodes.count[0] + lengthCodes.count[1]))
			return false;
		error = _Construct(distanceCodes, lengths + lengthCount, distanceCount);
		if (error != 0 && (error < 0 || distanceCount != distanceCodes.count[0] + distanceCodes.count[1]))
			return false;
		return _Codes(lengthCodes, distanceCodes);
	}

	const uint8*	fIn;
	size_t			fInLength;
	size_t			fInPos;
	uint32			fBitBuffer;
	int				fBitCount;
	uint8*			fOut;
	size_t			fOutCapacity;
	size_t			fOutPos;
	bool			fFailed;
};


uint32
BigEndian32(const uint8* p)
{
	return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3];
}


// 8 bit PNGs, not interlaced: grey, grey with alpha, colour (with or without alpha) and palette. -> BGRA, rows
// top down, straight (not premultiplied) alpha
bool
DecodePng(const char* path, int32& width, int32& height, std::vector<uint8>& bgra)
{
	FILE* file = fopen(path, "rb");
	if (file == NULL)
		return false;
	std::vector<uint8> data;
	uint8 buffer[8192];
	size_t got;
	while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
		data.insert(data.end(), buffer, buffer + got);
	fclose(file);

	static const uint8 kSignature[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
	if (data.size() < 8 || memcmp(&data[0], kSignature, 8) != 0)
		return false;

	int colorType = -1;
	std::vector<uint8> compressed, palette, paletteAlpha;
	size_t pos = 8;
	while (pos + 12 <= data.size()) {
		uint32 length = BigEndian32(&data[pos]);
		const uint8* type = &data[pos + 4];
		const uint8* body = &data[pos + 8];
		if (pos + 12 + length > data.size())
			return false;
		if (memcmp(type, "IHDR", 4) == 0 && length >= 13) {
			width = (int32)BigEndian32(body);
			height = (int32)BigEndian32(body + 4);
			if (body[8] != 8 || body[12] != 0)
				return false;		// 8 bits only, not interlaced
			colorType = body[9];
		} else if (memcmp(type, "PLTE", 4) == 0) {
			palette.assign(body, body + length);
		} else if (memcmp(type, "tRNS", 4) == 0) {
			paletteAlpha.assign(body, body + length);
		} else if (memcmp(type, "IDAT", 4) == 0) {
			compressed.insert(compressed.end(), body, body + length);
		} else if (memcmp(type, "IEND", 4) == 0) {
			break;
		}
		pos += 12 + length;
	}

	int channels;
	switch (colorType) {
		case 0: channels = 1; break;
		case 2: channels = 3; break;
		case 3: channels = 1; break;
		case 4: channels = 2; break;
		case 6: channels = 4; break;
		default: return false;
	}
	if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || compressed.empty())
		return false;

	const size_t stride = (size_t)width * channels;
	std::vector<uint8> raw((stride + 1) * height);
	Inflater inflater(&compressed[0], compressed.size(), &raw[0], raw.size());
	if (!inflater.Run() || inflater.Produced() != raw.size())
		return false;

	// undo the filters
	std::vector<uint8> pixels(stride * height);
	for (int32 y = 0; y < height; y++) {
		const uint8 filter = raw[y * (stride + 1)];
		const uint8* in = &raw[y * (stride + 1) + 1];
		uint8* out = &pixels[y * stride];
		const uint8* up = y > 0 ? &pixels[(y - 1) * stride] : NULL;
		for (size_t i = 0; i < stride; i++) {
			int a = i >= (size_t)channels ? out[i - channels] : 0;
			int b = up != NULL ? up[i] : 0;
			int c = (up != NULL && i >= (size_t)channels) ? up[i - channels] : 0;
			int predicted;
			switch (filter) {
				case 0: predicted = 0; break;
				case 1: predicted = a; break;
				case 2: predicted = b; break;
				case 3: predicted = (a + b) / 2; break;
				case 4: {
					int p = a + b - c;
					int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
					predicted = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
					break;
				}
				default: return false;
			}
			out[i] = (uint8)(in[i] + predicted);
		}
	}

	bgra.assign((size_t)width * height * 4, 0);
	for (int32 y = 0; y < height; y++) {
		for (int32 x = 0; x < width; x++) {
			const uint8* p = &pixels[y * stride + (size_t)x * channels];
			uint8* q = &bgra[((size_t)y * width + x) * 4];
			uint8 r, g, b, a = 255;
			switch (colorType) {
				case 0:
					r = g = b = p[0];
					break;
				case 2:
					r = p[0]; g = p[1]; b = p[2];
					break;
				case 3: {
					size_t index = p[0];
					if (index * 3 + 2 >= palette.size())
						return false;
					r = palette[index * 3]; g = palette[index * 3 + 1]; b = palette[index * 3 + 2];
					if (index < paletteAlpha.size())
						a = paletteAlpha[index];
					break;
				}
				case 4:
					r = g = b = p[0];
					a = p[1];
					break;
				default:
					r = p[0]; g = p[1]; b = p[2]; a = p[3];
					break;
			}
			q[0] = b;
			q[1] = g;
			q[2] = r;
			q[3] = a;
		}
	}
	return true;
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


// Takes decoded pixels (BGRA). A pixel is either part of the frame or not: the area the decorator owns on the
// screen is made of opaque pixels, and a half transparent one would blend with whatever the screen showed there
// before, not with the desktop behind the window.
bool
XfwmImage::_Adopt(int32 width, int32 height, const std::vector<uint8>& bgra)
{
	if (width <= 0 || height <= 0 || bgra.size() < (size_t)width * height * 4)
		return false;

	UtilityBitmap* bitmap = new(std::nothrow) UtilityBitmap(BRect(0, 0, width - 1, height - 1), B_RGBA32, 0);
	if (bitmap == NULL)
		return false;
	if (!bitmap->IsValid()) {
		delete bitmap;
		return false;
	}

	fRuns.clear();
	int32 minX = width, minY = height, maxX = -1, maxY = -1;
	for (int32 y = 0; y < height; y++) {
		uint8* out = bitmap->Bits() + y * bitmap->BytesPerRow();
		int32 runStart = -1;
		for (int32 x = 0; x < width; x++) {
			const uint8* in = &bgra[((size_t)y * width + x) * 4];
			const bool opaque = in[3] >= 128;
			out[x * 4 + 0] = opaque ? in[0] : 0;
			out[x * 4 + 1] = opaque ? in[1] : 0;
			out[x * 4 + 2] = opaque ? in[2] : 0;
			out[x * 4 + 3] = opaque ? 255 : 0;
			if (opaque) {
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

	std::vector<uint8> bgra((size_t)width * height * 4, 0);
	std::set<unsigned long> flatColors;		// the opaque colours that really appear
	for (int y = 0; y < height; y++) {
		const std::string& row = strings[1 + colorCount + y];
		for (int x = 0; x < width; x++) {
			uint8* out = &bgra[((size_t)y * width + x) * 4];
			if ((int)row.size() < (x + 1) * charsPerPixel)
				continue;
			for (size_t c = 0; c < colors.size(); c++) {
				if (row.compare(x * charsPerPixel, charsPerPixel, colors[c].key) == 0) {
					out[0] = colors[c].blue;
					out[1] = colors[c].green;
					out[2] = colors[c].red;
					out[3] = colors[c].alpha;
					if (colors[c].alpha > 0)
						flatColors.insert(((unsigned long)colors[c].red << 16) | (colors[c].green << 8)
							| colors[c].blue);
					break;
				}
			}
		}
	}

	// A picture of one flat colour is a placeholder (xfce's default-4.4 to 4.8 recolour it from the GTK theme):
	// the shading and the glyphs are in a PNG beside it, with some transparency, to be laid over it.
	if (flatColors.size() <= 1) {
		std::string png(path);
		size_t dot = png.rfind('.');
		if (dot != std::string::npos) {
			png.replace(dot, std::string::npos, ".png");
			int32 pngWidth = 0, pngHeight = 0;
			std::vector<uint8> overlay;
			if (DecodePng(png.c_str(), pngWidth, pngHeight, overlay) && pngWidth == width && pngHeight == height) {
				for (size_t p = 0; p < (size_t)width * height; p++) {
					uint8* base = &bgra[p * 4];
					const uint8* over = &overlay[p * 4];
					if (over[3] == 0)
						continue;
					if (base[3] == 0) {
						// nothing under it: it stands on its own
						memcpy(base, over, 4);
					} else {
						for (int k = 0; k < 3; k++)
							base[k] = (uint8)((over[k] * over[3] + base[k] * (255 - over[3])) / 255);
					}
				}
			}
		}
	}

	return _Adopt(width, height, bgra);
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


// The hover look of a button the theme has no hover picture for. Only the background of the button changes, and
// the glyph on it keeps its colours: a pixel changes in proportion to how close its brightness is to the typical
// one of the picture. A dark button gets lighter, and a light one (where lighter would hardly show) darker.
bool
XfwmImage::LoadBrightened(const XfwmImage& source, float amount)
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

	// the average brightness of the opaque pixels
	float total = 0;
	int32 count = 0;
	for (int32 y = 0; y < source.fHeight; y++) {
		const uint8* in = source.fBitmap->Bits() + y * source.fBitmap->BytesPerRow();
		for (int32 x = 0; x < source.fWidth; x++) {
			if (in[x * 4 + 3] < 128)
				continue;
			total += 0.114f * in[x * 4 + 0] + 0.587f * in[x * 4 + 1] + 0.299f * in[x * 4 + 2];
			count++;
		}
	}
	const float mean = count > 0 ? total / count : 128.0f;
	const bool darken = mean > 170.0f;

	for (int32 y = 0; y < source.fHeight; y++) {
		const uint8* in = source.fBitmap->Bits() + y * source.fBitmap->BytesPerRow();
		uint8* out = bitmap->Bits() + y * bitmap->BytesPerRow();
		for (int32 x = 0; x < source.fWidth; x++) {
			float brightness = 0.114f * in[x * 4 + 0] + 0.587f * in[x * 4 + 1] + 0.299f * in[x * 4 + 2];
			// 1 for a pixel as bright as the picture's typical one, falling to 0 for the glyph's black or white
			float weight = 1.0f - std::min(1.0f, fabsf(brightness - mean) / 70.0f);
			float k = amount * weight;
			for (int channel = 0; channel < 3; channel++) {
				float value = in[x * 4 + channel];
				value = darken ? value * (1.0f - k * 0.6f) : value + (255.0f - value) * k;
				out[x * 4 + channel] = (uint8)value;
			}
			out[x * 4 + 3] = in[x * 4 + 3];
		}
	}

	if (fBitmap != NULL)
		fBitmap->ReleaseReference();
	fBitmap = bitmap;
	fWidth = source.fWidth;
	fHeight = source.fHeight;
	fBounds = source.fBounds;
	fRuns = source.fRuns;
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
	fFullWidth(true),
	fHasActiveText(false),
	fHasInactiveText(false),
	fButtonOffset(0),
	fButtonSpacing(0),
	fButtonLayout("O|HMC")
{
	fOutline.red = fOutline.green = fOutline.blue = 0;
	fOutline.alpha = 255;
	fFill = fOutline;
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
	static const char* const kStateNames[kStateCount] = {"active", "inactive", "pressed", "prelight", "prelight-inactive"};
	for (int32 button = 0; button < kButtonCount; button++) {
		for (int32 state = 0; state < kStateCount; state++) {
			BString path(folder);
			path << "/" << kButtonNames[button] << "-" << kStateNames[state] << ".xpm";
			if (!fButton[button][state].Load(path.String()) && state != kStateActive && state != kStatePrelight
			&& state != kStatePrelightInactive) {
				BString active(folder);
				active << "/" << kButtonNames[button] << "-active.xpm";
				fButton[button][state].Load(active.String());
			}
		}
	}

	// a button the theme has no hover picture for gets a lighter one
	for (int32 button = 0; button < kButtonCount; button++) {
		if (!fButton[button][kStatePrelight].IsValid())
			fButton[button][kStatePrelight].LoadBrightened(fButton[button][kStateActive], 0.4f);
		// a window that isn't the active one has its own, paler buttons
		fButton[button][kStatePrelightInactive].LoadBrightened(fButton[button][kStateInactive], 0.4f);
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

		// the body of the border: the innermost opaque column
		column = (int32)left.OpaqueBounds().right;
		pixel = bitmap->Bits() + row * bitmap->BytesPerRow() + column * 4;
		fFill.blue = pixel[0];
		fFill.green = pixel[1];
		fFill.red = pixel[2];
		fFill.alpha = 255;
	}

	if (!fHasActiveText)
		fActiveText = _ReadableOn(fTitle[2][0], true);
	if (!fHasInactiveText)
		fInactiveText = _ReadableOn(fTitle[2][1], false);

	fValid = true;
	return true;
}


rgb_color
XfwmTheme::_ReadableOn(const XfwmImage& bar, bool active) const
{
	// the average colour of the opaque pixels of the bar behind the title
	uint64 red = 0, green = 0, blue = 0, count = 0;
	ServerBitmap* bitmap = bar.Bitmap();
	if (bitmap != NULL) {
		for (int32 y = 0; y < bar.Height(); y++) {
			const uint8* pixel = bitmap->Bits() + y * bitmap->BytesPerRow();
			for (int32 x = 0; x < bar.Width(); x++, pixel += 4) {
				if (pixel[3] < 128)
					continue;
				blue += pixel[0];
				green += pixel[1];
				red += pixel[2];
				count++;
			}
		}
	}

	bool dark = true;
	if (count > 0) {
		float luminance = (0.2126f * red + 0.7152f * green + 0.0722f * blue) / count;
		dark = luminance < 140.0f;
	}

	rgb_color color;
	if (dark) {
		// light text; the inactive window's is a little dimmer
		color.red = active ? 255 : 189;
		color.green = active ? 255 : 190;
		color.blue = active ? 255 : 189;
	} else {
		color.red = color.green = color.blue = active ? 0 : 64;
	}
	color.alpha = 255;
	return color;
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
			fHasActiveText = true;
		} else if (strcmp(key, "inactive_text_color") == 0 && ParseHexColor(value, r, g, b)) {
			fInactiveText.red = r;
			fInactiveText.green = g;
			fInactiveText.blue = b;
			fHasInactiveText = true;
		} else if (strcmp(key, "title_vertical_offset_active") == 0)
			fOffsetActive = atoi(value);
		else if (strcmp(key, "title_vertical_offset_inactive") == 0)
			fOffsetInactive = atoi(value);
		else if (strcmp(key, "title_alignment") == 0)
			fAlignment = strcmp(value, "center") == 0 ? 1 : (strcmp(value, "right") == 0 ? 2 : 0);
		else if (strcmp(key, "full_width_title") == 0)
			fFullWidth = strcmp(value, "false") != 0 && strcmp(value, "0") != 0;
		else if (strcmp(key, "button_layout") == 0)
			fButtonLayout = value;
		else if (strcmp(key, "button_offset") == 0)
			fButtonOffset = atoi(value);
		else if (strcmp(key, "button_spacing") == 0)
			fButtonSpacing = atoi(value);
	}
	fclose(file);
}
