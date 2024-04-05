#ifndef INI_PARSER_H
#define INI_PARSER_H
/*
MIT License

Copyright (c) mittorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "container_utl.h"
#include <string.h>
#include <stdio.h>

struct IniParserLine
{
	char *begin = nullptr, *end = nullptr;
	IniParserLine(){}
	IniParserLine(char *b, char *e) : begin(b), end(e) {}
	IniParserLine(const char *str) : begin((char*)str), end((char*)str+strlen(str)){}
	operator const char*(){ return begin;}
};

template<size_t TblSize>
forceinline static inline size_t HashFunc(const IniParserLine &key)
{
	size_t hash = 7;
	const unsigned char *s = (const unsigned char*)key.begin;
	while(s < (const unsigned char*)key.end)
		hash = hash * 31 + *s++;
	return hash & (TblSize - 1);
}

template<>
inline bool KeyCompare(const IniParserLine &a, const IniParserLine &b)
{
	return ((a.end - a.begin) == (b.end - b.begin)) && !memcmp(a.begin, b.begin, a.end - a.begin);
}

struct IniParser
{
	char *mpszData = nullptr;
	size_t muiLength;

	HashMap<IniParserLine, HashArrayMap<IniParserLine, IniParserLine>> mDict;
	static bool _CharIn(char ch, const char *s)
	{
		char c;
		while((c = *s++))
			if(c == ch)
				return true;
		return false;
	}

	static char *_LFind(char *pstr, char *end, const char *d, bool invert)
	{
		while(pstr < end && !(_CharIn(*pstr, d) ^ invert))pstr++;
		return pstr;
	}
	static char *_RFind(char *pstr, char *start, const char *d, bool invert)
	{
		while(pstr > start)
		{
			char c = *--pstr;
			if(_CharIn(c, d) ^ invert)
				break;
		}
		return pstr + 1;
	}
	size_t _GetLine(size_t start, IniParserLine &l)
	{
		char *end = mpszData + muiLength;
		char *pstr = mpszData + start;
		pstr = _LFind(pstr, end, " \t\n\r", true);
		l.begin = pstr;
		size_t next = 0;
		char prev = 0;
		while(pstr <= end)
		{
			char c = *pstr++;
			if(c == '\n' && prev != '\\')
				break;
			prev = c;
		}
		next = pstr - mpszData;
		pstr = _RFind(pstr, mpszData + start, " \t\n\r", true);
		if(pstr > end)
			pstr = end;
		l.end = pstr;
		*l.end = 0;
		return next;
	}

	explicit operator bool()
	{
		return !!mpszData;
	}
	IniParser(){}
	IniParser(const char *path)
	{
		_LoadFile(path);
		if(!*this)
			return;
		size_t pos= 0;
		static char root_section[] = "[root]";
		IniParserLine section = {&root_section[0], &root_section[5]};
		HashArrayMap<IniParserLine, IniParserLine> *sectDict = &mDict[section.begin];
		IniParserLine l;
		while((pos = _GetLine(pos, l)))
		{
			//printf("rline %s\n", l.begin);
			char *comment = _LFind(l.begin, l.end, "#;", false);
			if(comment != l.end)
			{
				//printf("comment %s\n", comment);
				l.end = _RFind(comment, l.begin," \t\n\r", true);
				*l.end = 0;
			}
			//printf("line %s %s\n", l.begin, l.end - 1);
			if(*l.begin == '[')
			{
				if(*(l.end - 1) == ']')
				{
					section = l;
					sectDict = &mDict[section];
				}
				else
				{
					printf("section error\n");
					// error
				}
			}
			char *assign = _LFind(l.begin, l.end, "=", false);
			if(*assign == '=')
			{
				IniParserLine var, val;
				var.begin = l.begin;
				var.end = _RFind(assign, l.begin," \t\n\r", true);
				*var.end = 0;
				val.begin = _LFind(assign + 1, l.end," \t\n\r", true);
				val.end = l.end;
				(*sectDict)[var] = val;
			}
			if(pos >= muiLength)
				break;
		}
	}
	~IniParser()
	{
		delete[] mpszData;
		mpszData = nullptr;
	}
	void _LoadFile(const char *path)
	{
		if(mpszData)
			return;
		FILE *f = fopen(path, "rb");
		if(!f)
			return;
		fseek(f, 0, SEEK_END);
		muiLength = ftell(f);
		mpszData = new char[muiLength + 1];
		if(mpszData)
		{
			mpszData[muiLength] = 0;
			fseek(f, 0, SEEK_SET);
			size_t ret = fread(mpszData, 1, muiLength, f);
			(void)ret;
		}
		fclose(f);
	}
};

#endif // INI_PARSER_H
