// SPDX-FileCopyrightText: 2024 mittorn <mittorn@disroot.org>
//
// SPDX-License-Identifier: MIT
//
#pragma once
#include "container_utl.h"
#define FMT_ENABLE_STDIO
#include "fmt_util.h"
#include "string_utl.h"

// fmt_util extension
template<typename Buf>
forceinline constexpr static inline void ConvertS(Buf &s, const SubStr &arg)
{
	for(const char *str = arg.begin; str < arg.end;str++)
		s.w(*str);
}

// hashmap extension

template<>
inline bool KeyCompare(const SubStr &a, const SubStr &b)
{
	return a.Equals(b);
}

//inline const SubStr &KeyAlloc<SubStr>(const SubStr &a) = delete;

inline const SubStr  KeyAlloc(const SubStr &a)
{
	char *mem = (char*)malloc(a.Len() + 1);
	if(!mem)
		return "";
	memcpy(mem, a.begin, a.Len());
	mem[a.Len()] = 0;
	return SubStr(mem, mem + a.Len());
}

template<>
inline void KeyDealloc(const SubStr &a)
{
	free((void*)a.begin);
}

template<size_t TblSize>
forceinline static inline size_t HashFunc(const SubStr &key)
{
	size_t hash = 7;
	const unsigned char *s = (const unsigned char*)key.begin;
	while(s < (const unsigned char*)key.end)
		hash = hash * 31 + *s++;
	return hash & (TblSize - 1);
}

#define Log(...) FPrint(stderr, __VA_ARGS__)
