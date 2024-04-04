#ifndef STRING_UTL_H
#define STRING_UTL_H
#include <string.h>
// stringview-like
struct SubStr
{
	const char *begin, *end;
	template<size_t len>
	SubStr(const char (&buf)[len]) :begin(buf), end(&buf[len - 1]){}
	SubStr(const char *b, const char *e) : begin(b), end(e){}
	SubStr(const char *b, size_t len) : begin(b), end(b + len){}
	SubStr(){}
	size_t Len() const
	{
		return end - begin;
	}
	bool Equals(const SubStr &other) const
	{
		if(Len() != other.Len())
			return false;
		return !memcmp(begin, other.begin, Len());
	}

	template<size_t len>
	size_t CopyTo(char (&buf)[len]) const
	{
		size_t l = Len();
		if(l > len - 1)
			l = len - 1;
		memcpy(buf, begin, l);
		buf[l] = 0;
		return l;
	}
	size_t CopyTo(char *buf, size_t len) const
	{
		size_t l = Len();
		if(l > len - 1)
			l = len - 1;
		memcpy(buf, begin, l);
		buf[l] = 0;
		return l;
	}
	bool Split2(SubStr &part1, SubStr &part2, char ch) const
	{
		const char *s = begin;
		while(s < end)
		{
			if(*s++ == ch)
			{
				part1 = {begin, s - 1};
				part2 = {s, end};
				return true;
			}
		}
		return false;
	}

	SubStr GetSubStr(size_t s, size_t e = -1) const
	{
		if( e > Len())
			e = Len();
		return {begin + s, begin + e};
	}

	bool StartsWith(const SubStr &other) const
	{
		if(other.Len() > Len())
			return false;
		return !memcmp(begin, other.begin, other.Len());
	}
	bool EndsWith(const SubStr &other) const
	{
		if(other.Len() > Len())
			return false;
		return !memcmp(end - other.Len(), other.begin, other.Len());
	}

	bool Contains(const SubStr &other) const
	{
		// todo: fallback for this non-standart func
		return !!memmem(begin, Len(), other.begin, other.Len());
	}
};

#endif // STRING_UTL_H
