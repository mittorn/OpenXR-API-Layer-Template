#ifndef STRUCT_UTIL_H
#define STRUCT_UTIL_H
#include <string.h>
#include "fmt_util.h"

#ifdef __GNUC__
#define FUNCNAME __PRETTY_FUNCTION__
#else
#define FUNCNAME __FUNCSIG__
#endif

// first variant: keep full string
template <size_t len>
struct TN
{
	size_t funclen;
	char funcname[len];
	constexpr TN(const char (&func)[len], size_t begin, size_t end) : funcname(), funclen(len - end - 1 - begin){ for(int i = 0; i < funclen; i++)funcname[i] = func[i + begin];}
};

// only keep pointer and first char
template <size_t len>
struct TN2
{
	size_t funclen;
	const char *funcname;
	const char firstchar;
	constexpr TN2(const char (&func)[len], size_t begin, size_t end) : funcname(&func[begin]), funclen(len - end - 1 - begin), firstchar(func[begin]){}
};

struct TN_Format
{
	size_t leading_junk = 0, trailing_junk = 0, enum_leading_junk = 0, enum_trailing_junk = 0, enum_type_mult = 0;
};
enum IJK{
	XYZ
};

// raw (need for format detection)
template <typename T>
constexpr const auto GetTN0()
{
	return TN(FUNCNAME,0,0);
}
template <typename T>
constexpr auto TN_raw = GetTN0<T>();

template <typename T, T e>
constexpr const auto GetEN0(size_t off)
{
	return TN(FUNCNAME,0,0);
}
template <typename T, T e>
constexpr auto EN_raw = GetEN0<T,e>(0);

// find offsets of type and enum names
constexpr TN_Format PrepareFormat()
{
	TN_Format format;
	size_t i = 0, j = 0;
	for(i = 0; i < TN_raw<int>.funclen - 3; i++)
	{
		if (TN_raw<int>.funcname[i] == 'i' && TN_raw<int>.funcname[i+1] == 'n' && TN_raw<int>.funcname[i+2] == 't')
		{
			format.leading_junk = i;
			format.trailing_junk = TN_raw<int>.funclen-i-3-1;
			break;
		}
	}
	for (i = 0; i <EN_raw<IJK, XYZ>.funclen - 3; i++)
	{
		if (EN_raw<IJK, XYZ>.funcname[i] == 'I' && EN_raw<IJK, XYZ>.funcname[i+1] == 'J' && EN_raw<IJK, XYZ>.funcname[i+2] == 'K')
		{
			j++;
		}
		if (EN_raw<IJK, XYZ>.funcname[i] == 'X' && EN_raw<IJK, XYZ>.funcname[i+1] == 'Y' && EN_raw<IJK, XYZ>.funcname[i+2] == 'Z')
		{
			format.enum_leading_junk = i - j * (TN_raw<IJK>.funclen - format.leading_junk - format.trailing_junk);
			format.enum_trailing_junk = EN_raw<IJK, XYZ>.funclen - i - 3 - 1; // `3` is the length of "XYZ", `1` is the space for the null terminator.
			format.enum_type_mult = j;
			break;
		}
	}
	return format;
}

constexpr TN_Format constformat = PrepareFormat();

// full constexpr string copy (maybe slow)
template <typename T>
constexpr const auto GetTN1()
{
	return TN(FUNCNAME,constformat.leading_junk, constformat.trailing_junk + 1);
}
template <typename T>
constexpr auto TN_v = GetTN1<T>();

template <typename T, T e>
constexpr const auto GetEN1(size_t off)
{
	return TN(FUNCNAME,constformat.enum_leading_junk, constformat.enum_trailing_junk + 1);
}
template <typename T, T e>
constexpr static auto EN_v = GetEN1<T,e>(0);

// skip string copy, just pass pointer. String is not cut
template <typename T>
constexpr const auto GetTN2()
{
	return TN2(FUNCNAME,constformat.leading_junk, constformat.trailing_junk + 1);
}
template <typename T>
constexpr auto TN_d = GetTN2<T>();

template <typename T, T e>
constexpr const auto GetEN2(size_t off)
{
	return TN2(FUNCNAME,constformat.enum_leading_junk + off, constformat.enum_trailing_junk + 1);
}

template <typename T>
constexpr const char *TypeName()
{
	return TN_v<T>.funcname;
}
#if !(__clang__) && !(__GNUC__ > 8)
// broken
template<typename T, T e = (T)0, T maxe = (T)255>
static inline const char *StringifyEnum(T val)
{
	return "";
}

template<typename T, T e = (T)0, T maxe = (T)255>
forceinline static inline T UnstringifyEnum(const char *name)
{
	return (T)0;
}
#else
template<typename T, T e = (T)0, T maxe = (T)255, size_t missing = 0>
forceinline static inline const char *StringifyEnum(T val)
{
	constexpr size_t len1 = TN_d<T>.funclen + 1;
	constexpr auto &d = EN_v<T,e>;
	if constexpr(missing > 16)
		return "";
	else if constexpr(d.funcname[constformat.enum_type_mult * len1] < 'A' || !len1)
		return StringifyEnum<T,(T)(e+1),maxe, missing + 1>(val);
	if(e == val)
		return &d.funcname[constformat.enum_type_mult * len1];

	if constexpr(e < maxe)// && !(d.funcname[constformat.enum_type_mult * len1] < 'A' || !len1))
		return StringifyEnum<T,(T)(e+1),maxe, 0>(val);
	return "";
}

#ifndef __clang__
template<typename T, T e = (T)0, T maxe = (T)255, T def = (T)0, size_t missing = 0>
forceinline static inline T UnstringifyEnum(const char *name)
{
	constexpr size_t len1 = TN_d<T>.funclen + 1;
	constexpr auto d = GetEN2<T,e>(constformat.enum_type_mult * len1);
	if constexpr(missing > 16)
		return def;
	else if constexpr(d.firstchar < 'A' || !len1)
		return UnstringifyEnum<T,(T)(e+1),maxe,def, missing + 1>(name);

	if(!strncmp(name, d.funcname, d.funclen))
		return e;

	if constexpr(e < maxe && !(d.firstchar < 'A' || !len1))
		return UnstringifyEnum<T,(T)(e+1),maxe,def, 0>(name);
	else
		return def;
}
#else

template<typename T, T e = (T)0, T maxe = (T)255, T def = (T)0>
forceinline static inline T UnstringifyEnum(const char *name)
{
	constexpr size_t len1 = GetTN2<T>().funclen + 1;
	constexpr auto d = GetEN1<T,e>(0);
	if constexpr(d.funcname[constformat.enum_type_mult * len1] < 'A' || !len1)
		return def;

	if(!strncmp(name, d.funcname+constformat.enum_type_mult * len1, d.funclen - constformat.enum_type_mult * len1))
		return e;

	if constexpr(e < maxe && !(d.funcname[constformat.enum_type_mult * len1] < 'A' || !len1))
		return UnstringifyEnum<T,(T)(e+1),maxe,def>(name);
	else
		return def;
}

#endif
#endif
template <int ... i> struct ISeq{};
template<typename... Ts> struct MakeVoid { typedef void t; };

template <typename T, typename V, typename Seq = ISeq<>, typename = void>
struct ConstructorVisitor : Seq {
	constexpr static int size = 0;
};

struct StubPlacementNew{void *t;};
inline void* operator new (size_t n, const StubPlacementNew& s) {return s.t;};
template <typename T, typename V, int... Indices>
struct ConstructorVisitor<
	T, V, ISeq<Indices...>,
	typename MakeVoid<decltype(T{((void)(Indices), *(V*)0)...,*(V*)0})>::t
	>
	: ConstructorVisitor<T, V, ISeq<Indices..., sizeof...(Indices)>>
{
	using Base = ConstructorVisitor<T,V,ISeq<Indices..., sizeof...(Indices)>>;
	constexpr static int size = 1 + Base::size;

	forceinline void Fill(T *t, V &v)
	{
		if constexpr(size == 1)
		{
			t->~T();
			new(StubPlacementNew{t})T{(v.SetIndex(Indices),v)...,(v.SetIndex(sizeof...(Indices)),v)};
			v.End(sizeof(T));
		}
		else if constexpr(size > 1)
			Base::Fill(t, v);
	}
	forceinline void Construct(V &v)
	{
		if constexpr(size == 1)
		{
			(void)T{(v.SetIndex(Indices),v)...,(v.SetIndex(sizeof...(Indices)),v)};
			v.End(sizeof(T));
		}
		else if constexpr(size > 1)
			Base::Construct(v);
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
	const char *tmpbase;
	size_t off2 = 0;
	const char *(*last_typename)() = nullptr;
	void SetIndex(int idx){
		index = idx;
	}
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
		{
			off = off2 = 0;
		}
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

		//printf(" %s %d %d %d %d %d %d %d %d %d\n", TypeName<T>(), (int)off2, (int)off, (int)sizeof(T), index, is_numeric, is_pointer,is_uniform_constructible2, is_array_subscriptable, is_num_assignable);
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
			if constexpr(((T)0.1f) != (T)0.0f)
			{
				snprintf(&buffer[len], 256 - len, "%f ", (double)*(T*)(base + off));
			}
			else
			{
				if constexpr(is_num_assignable)
					snprintf(&buffer[len], 256 - len, "%lld ", (long long)*(T*)(base + off));
				else
					snprintf(&buffer[len], 256 - len, "%lld %s", (long long)*(T*)(base + off), StringifyEnum<T>(*(T*)(base + off)));
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
#if 0
		T local{}; // just proof NRVO is a myth
		if(index == 0)
			tmpbase = (const char*)(const void*)&local;
		else
			off2 = (const char*)(const void*)&local - tmpbase;
		return local;
#else
		return T();
#endif
	}

	operator char()
	{
		if(index == 0)
		{
			off = off2 = 0;
		}
		if(last_typename != &TypeName<char>)
		Flush("char",(int)off, 1);
		const unsigned char c = *(const unsigned char*)(base + off);
		if(c == 0)
		{
			if(chars_len)
				Flush("char",(int)off, 1);
			zeroes_len++;
		}
		else if(c >= 32)
		{
			if(zeroes_len)
				Flush("char",(int)off, 1);
			if(chars_len < 255)
				buffer_chars[chars_len++] = c;
		}
		else if(chars_len || zeroes_len)
		{
			Flush("char",(int)off, 1);
		}
		else
			snprintf(&buffer[len], 256 - len, "%02X ", c);
		len = strlen(buffer);
		last_typename = &TypeName<char>;
		off++;
		return 0;
	}
};

template <typename T>
void DumpGenericStruct(T *data)
{
	printf("struct %s {\n", TypeName<T>());
	GenericReflect t;
	t.base = (char*)data;
	ConstructorVisitor<T, GenericReflect>().Construct(t);
	printf("}\n");
}


#endif // STRUCT_UTIL_H
