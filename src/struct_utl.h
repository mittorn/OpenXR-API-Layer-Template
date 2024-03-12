#ifndef STRUCT_UTIL_H
#define STRUCT_UTIL_H
#include <string.h>
#include "fmt_util.h"


template <typename T>
const auto &RawTypeName()
{
#ifdef _MSC_VER
	return __FUNCSIG__;
#else
	return __PRETTY_FUNCTION__;
#endif
}

template <typename E, E v>
const auto &RawEnumName()
{
#ifdef _MSC_VER
	return __FUNCSIG__;
#else
	return __PRETTY_FUNCTION__;
#endif
}

struct RawTypeNameFormat
{
	size_t leading_junk = 46, trailing_junk = 1;
	size_t enum_leading_junk = 46, enum_trailing_junk = 1, enum_type_mult = 0;
};

// Returns `false` on failure.
forceinline static inline bool GetRawTypeNameFormat(RawTypeNameFormat *format)
{
	const auto &str = RawTypeName<int>();
	for (size_t i = 0;; i++)
	{
		if (str[i] == 'i' && str[i+1] == 'n' && str[i+2] == 't')
		{
			if (format)
			{
				format->leading_junk = i;
				format->trailing_junk = sizeof(str)-i-3-1; // `3` is the length of "int", `1` is the space for the null terminator.
			}
			return true;
		}
	}
	return false;
}
enum IJK{
	XYZ
};
forceinline static inline bool GetRawEnumNameFormat(RawTypeNameFormat *format)
{
	const char *raw = RawTypeName<IJK>();
	const auto &str = RawEnumName<IJK, XYZ>();
	int j = 0;
	for (size_t i = 0;; i++)
	{
		if (str[i] == 'I' && str[i+1] == 'J' && str[i+2] == 'K')
		{
			j++;
		}
		if (str[i] == 'X' && str[i+1] == 'Y' && str[i+2] == 'Z')
		{
			if (format)
			{
				format->enum_leading_junk = i - j * (strlen(raw) - format->leading_junk - format->trailing_junk);
				format->enum_trailing_junk = sizeof(str)- i - 3 - 1; // `3` is the length of "XYZ", `1` is the space for the null terminator.
				format->enum_type_mult = j;
			}
			return true;
		}
	}
	return false;
}

forceinline static inline RawTypeNameFormat InitFormat()
{
	RawTypeNameFormat format;
	GetRawTypeNameFormat(&format);
	GetRawEnumNameFormat(&format);
	return format;
}

static RawTypeNameFormat tn_format = InitFormat();

template <typename T>
static inline void FillTypeName(char *name)
{
	const char *raw = RawTypeName<T>();
	RawTypeNameFormat &format = tn_format;
	size_t len = strlen(raw) - format.leading_junk - format.trailing_junk;
	if(len > 255) len = 255;

	for (size_t i = 0; i < len; i++)
		name[i] = raw[i + format.leading_junk];
}

template<typename T, T e>
static inline void FillEnumName(char *name)
{
	const char *raw = RawTypeName<T>();
	RawTypeNameFormat &format = tn_format;
	size_t len = strlen(raw) - format.leading_junk - format.trailing_junk;
	const char *rawe = RawEnumName<T,e>();
	size_t lene = strlen(rawe) - format.enum_leading_junk - format.enum_type_mult * len  - format.enum_trailing_junk;
	if(lene > 255) lene = 255;
	for (size_t i = 0; i < lene; i++)
		name[i] = rawe[i + format.enum_leading_junk + format.enum_type_mult * len];
}

template<typename T, T e = (T)0, T maxe = (T)255>
static inline void StringifyEnum(char *name, T val)
{
	if(e == val)
	{
		FillEnumName<T,e>(name);
		return;
	}
	if constexpr(e < maxe)
		StringifyEnum<T,(T)(e+1),maxe>(name,val);
}

template<typename T, T e = (T)0, T maxe = (T)255, T def = (T)0>
static inline T UnstringifyEnum(const char *name)
{
	char buffer[256];
	FillEnumName<T,e>(buffer);
	if(!strcmp(buffer, name))
		return e;
	if constexpr(e < maxe)
		return UnstringifyEnum<T,(T)(e+1),maxe,def>(name);
	return def;
}

template <typename T>
struct tn_t{
	char str[256];
	tn_t()
	{
		FillTypeName<T>(str);
	}
};

template <typename T>
tn_t<T> tn;

template <typename T>
static inline const char *TypeName()
{
	return tn<T>.str;
}
template <int ... i> struct ISeq{};
template<typename... Ts> struct MakeVoid { typedef void t; };

template <typename T, typename V, typename Seq = ISeq<>, typename = void>
struct ConstructorVisiotor : Seq {

	constexpr static int size = 0;
	void Fill(T *t, V v, int s){}
	void Construct(V v, int s){}
};

template <typename T, typename V, int... Indices>
struct ConstructorVisiotor<
	T, V, ISeq<Indices...>,
		typename MakeVoid<decltype(T{((void)(Indices), V())...,V()})>::t
	>
	: ConstructorVisiotor<T, V, ISeq<Indices..., sizeof...(Indices)>>
{
	using Base = ConstructorVisiotor<T,V,ISeq<Indices..., sizeof...(Indices)>>;
	constexpr static int size = 1 + Base::size;

	void Fill(T *t, V v, int s = size)
	{
		if(sizeof...(Indices) == s - 1)
		{
			*t = T{(v.SetIndex(Indices),v)...,(v.SetIndex(s - 1),v)};
			v.End(sizeof(T));
		}
		else
			Base::Fill(t, v, s);
	}
	void Construct(V v, int s = size)
	{
		if(sizeof...(Indices) == s - 1)
		{
			(void)T{(v.SetIndex(Indices),v)...,(v.SetIndex(s - 1),v)};
			v.End(sizeof(T));
		}
		else
			Base::Construct(v, s);
	}
};

template <typename T> static size_t AlignOf()
{
	struct A{char a; T b;char c;};
	return (size_t)(&((A*)0)->c) - sizeof(T);
}

struct GenericReflect
{
	char *base;
	size_t off = 0;
	int index =-1;
	char buffer[256] = "";
	size_t len = 0;
	char buffer_chars[256] = "";
	size_t chars_len = 0;
	size_t body_begin = 0;
	size_t zeroes_len = 0;
	const char *(*last_typename)() = nullptr;
	void SetIndex(int idx){
		index = idx;
	}
	//template<typename T>
	void Flush(const char *type, size_t off, size_t size)
	{
		if(chars_len)
		{
			snprintf(&buffer[body_begin], 256 - len, "%s", buffer_chars);
			len = body_begin = strlen(buffer);
		}
		chars_len = 0;
		if(zeroes_len)
		{
			snprintf(&buffer[body_begin], 256 - len, "(%d zeroes) ", (int)zeroes_len);
			len = body_begin = strlen(buffer);
		}
		zeroes_len = 0;
		if(len)
			puts(buffer);
		snprintf(buffer, 256, "%s %d %d %d ", type, index, (int)off, (int)size);
		len = body_begin = strlen(buffer);
	}
	void End(size_t size)
	{
		puts(buffer);
		if(off != size)
			printf("Size mismatch! %d %d\n", (int)off, (int)size);
		len = 0;
	}
	template<typename T>
	operator T()
	{
		if(index == 0)
			off = 0;
		while(off % AlignOf<T>())
			off++;
		OverloadCheck(is_numeric, (T*)0,(double)*x);
		OverloadCheck(is_pointer, (T*)0,**x);
		OverloadCheck(is_uniform_constructible2, (T*)0,(*x = {0,0}));
		OverloadCheck(is_uniform_constructible, (T*)0,(*x = {0}));
		OverloadCheck(is_array_subscriptable, (T*)0,(*x)[0]);
		OverloadCheck(is_zero_addable, (T*)0,*x + 0);
		OverloadCheck(is_num_assignable, (T*)0,*x = 2);
		OverloadCheck(is_constructible, (T*)0,*x = T());
		constexpr bool is_void_pointer = CompareTypes(T*, void**) || CompareTypes(T*, const void**);

		//printf(" %s %d %d %d %d %d %d %d %d\n", TypeName<T>(),(int)off, (int)sizeof(T), index, is_numeric, is_pointer,is_uniform_constructible2, is_array_subscriptable, is_num_assignable);
		if(last_typename != &TypeName<T>)
			Flush(TypeName<T>(),(int)off, (int)sizeof(T));


		if constexpr(is_constructible && !is_void_pointer && !is_zero_addable && !is_array_subscriptable && !is_pointer && !is_numeric)
		{
			OverloadCheck(is_pointer_castable, (T*)0,(char*)(void*)(*x));
			if constexpr(!is_pointer_castable)
			{
				if(last_typename == &TypeName<T>)
					Flush(TypeName<T>(),(int)off, (int)sizeof(T));
				DumpGenericStruct((T*)(base + off));
			}
		}
		else if constexpr(is_numeric && !is_pointer && !is_void_pointer)
		{
			if constexpr(sizeof(T) == 1)
			{
				const unsigned char c = *(const unsigned char*)(base + off);
				if(c == 0)
				{
					if(chars_len)
						Flush(TypeName<T>(),(int)off, (int)sizeof(T));
					zeroes_len++;
				}
				else if(c >= 32)
				{
					if(zeroes_len)
						Flush(TypeName<T>(),(int)off, (int)sizeof(T));
					if(chars_len < 255)
						buffer_chars[chars_len++] = c;
				}
				else if(chars_len || zeroes_len)
				{
					Flush(TypeName<T>(),(int)off, (int)sizeof(T));
				}
				else
					snprintf(&buffer[len], 256 - len, "%02X ", c);
			}
			else if constexpr(((T)0.1f) != (T)0.0f)
			{
				snprintf(&buffer[len], 256 - len, "%f ", (double)*(T*)(base + off));
			}
			else
			{
				if constexpr(is_num_assignable)
					snprintf(&buffer[len], 256 - len, "%lld ", (long long)*(T*)(base + off));
				else
				{
					char buf[256];
					StringifyEnum<T>(buf,*(T*)(base + off));
					snprintf(&buffer[len], 256 - len, "%lld %s", (long long)*(T*)(base + off), buf);
				}
			}
			len = strlen(buffer);
			//printf("%f\n", (float)*(T*)(base + off));
		}
		else if constexpr(is_pointer || is_void_pointer)
		{
			//printf("%p\n", (void*)*(T*)(base + off));
			snprintf(&buffer[len], 256 - len, "%p ", (void*)*(T*)(base + off));
			len = strlen(buffer);
		}
		last_typename = TypeName<T>;
		off += sizeof(T);
		return T();
	}

};

template <typename T>
void DumpGenericStruct(T *data)
{
	printf("struct %s {\n", TypeName<T>());
	GenericReflect t;
	t.base = (char*)data;
	ConstructorVisiotor<T, GenericReflect>().Construct(t);
	printf("}\n");
}


#endif // STRUCT_UTIL_H
