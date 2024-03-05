#ifndef FMT_UTIL_H
#define FMT_UTIL_H
/*
Simplified signle-header C++ printf implementation
Floating point code and some logics got from stb_sprintf:

http://github.com/nothings/stb

allowed types:  sc uidBboXx p GgEef n
Lengths are ignored, template types used

Only constant format strings accepted
to allow compile-time loop unrolling

If unrolls fails, code may blow-up to megabytes
so enabling compiler optimization is very recommended
*/

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)
#define noinline __attribute__ ((noinline))
#define forceinline __attribute__ ((always_inline))
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#define noinline
#define forceinline
#endif // __GNUC__

// stb_sprintf float utility
#define STBSP__SPECIAL 0x7000

#define stbsp__uint32 unsigned int
#define stbsp__int32 signed int
#define stbsp__uint64 unsigned long long
#define stbsp__int64 signed long long
#define stbsp__uint16 unsigned short

#define STBSP__COPYFP(dest, src)  *(stbsp__uint64*)&dest = *(stbsp__uint64*)&src
#define stbsp__ddmulthi(oh, ol, xh, yh)                            \
   {                                                               \
	  double ahi = 0, alo, bhi = 0, blo;                           \
	  stbsp__int64 bt;                                             \
	  oh = xh * yh;                                                \
	  STBSP__COPYFP(bt, xh);                                       \
	  bt &= ((~(stbsp__uint64)0) << 27);                           \
	  STBSP__COPYFP(ahi, bt);                                      \
	  alo = xh - ahi;                                              \
	  STBSP__COPYFP(bt, yh);                                       \
	  bt &= ((~(stbsp__uint64)0) << 27);                           \
	  STBSP__COPYFP(bhi, bt);                                      \
	  blo = yh - bhi;                                              \
	  ol = ((ahi * bhi - oh) + ahi * blo + alo * bhi) + alo * blo; \
   }
#define stbsp__ddtoS64(ob, xh, xl)          \
   {                                        \
	  double ahi = 0, alo, vh, t;           \
	  ob = (stbsp__int64)xh;                \
	  vh = (double)ob;                      \
	  ahi = (xh - vh);                      \
	  t = (ahi - xh);                       \
	  alo = (xh - (ahi - t)) - (vh + t);    \
	  ob += (stbsp__int64)(ahi + alo + xl); \
   }
#define stbsp__ddrenorm(oh, ol) \
   {                            \
	  double s;                 \
	  s = oh + ol;              \
	  ol = ol - (s - oh);       \
	  oh = s;                   \
   }
#define stbsp__ddmultlo(oh, ol, xh, xl, yh, yl) ol = ol + (xh * yl + xl * yh);
#define stbsp__ddmultlos(oh, ol, xh, yl) ol = ol + (xh * yl);

forceinline static inline void fmtutil_stbsp__raise_to_power10(double *ohi, double *olo, double d, stbsp__int32 power) // power can be -323 to +350
{
	constexpr static double const stbsp__bot[23] = {
		1e+000, 1e+001, 1e+002, 1e+003, 1e+004, 1e+005, 1e+006, 1e+007, 1e+008, 1e+009, 1e+010, 1e+011,
		1e+012, 1e+013, 1e+014, 1e+015, 1e+016, 1e+017, 1e+018, 1e+019, 1e+020, 1e+021, 1e+022
	};
	constexpr static double const stbsp__negbot[22] = {
		1e-001, 1e-002, 1e-003, 1e-004, 1e-005, 1e-006, 1e-007, 1e-008, 1e-009, 1e-010, 1e-011,
		1e-012, 1e-013, 1e-014, 1e-015, 1e-016, 1e-017, 1e-018, 1e-019, 1e-020, 1e-021, 1e-022
	};
	constexpr static double const stbsp__negboterr[22] = {
		-5.551115123125783e-018,  -2.0816681711721684e-019, -2.0816681711721686e-020, -4.7921736023859299e-021, -8.1803053914031305e-022, 4.5251888174113741e-023,
		4.5251888174113739e-024,  -2.0922560830128471e-025, -6.2281591457779853e-026, -3.6432197315497743e-027, 6.0503030718060191e-028,  2.0113352370744385e-029,
		-3.0373745563400371e-030, 1.1806906454401013e-032,  -7.7705399876661076e-032, 2.0902213275965398e-033,  -7.1542424054621921e-034, -7.1542424054621926e-035,
		2.4754073164739869e-036,  5.4846728545790429e-037,  9.2462547772103625e-038,  -4.8596774326570872e-039
	};
	constexpr static double const stbsp__top[13] = {
		1e+023, 1e+046, 1e+069, 1e+092, 1e+115, 1e+138, 1e+161, 1e+184, 1e+207, 1e+230, 1e+253, 1e+276, 1e+299
	};
	constexpr static double const stbsp__negtop[13] = {
		1e-023, 1e-046, 1e-069, 1e-092, 1e-115, 1e-138, 1e-161, 1e-184, 1e-207, 1e-230, 1e-253, 1e-276, 1e-299
	};
	constexpr static double const stbsp__toperr[13] = {
		8388608,
		6.8601809640529717e+028,
		-7.253143638152921e+052,
		-4.3377296974619174e+075,
		-1.5559416129466825e+098,
		-3.2841562489204913e+121,
		-3.7745893248228135e+144,
		-1.7356668416969134e+167,
		-3.8893577551088374e+190,
		-9.9566444326005119e+213,
		6.3641293062232429e+236,
		-5.2069140800249813e+259,
		-5.2504760255204387e+282
	};
	constexpr static double const stbsp__negtoperr[13] = {
		3.9565301985100693e-040,  -2.299904345391321e-063,  3.6506201437945798e-086,  1.1875228833981544e-109,
		-5.0644902316928607e-132, -6.7156837247865426e-155, -2.812077463003139e-178,  -5.7778912386589953e-201,
		7.4997100559334532e-224,  -4.6439668915134491e-247, -6.3691100762962136e-270, -9.436808465446358e-293,
		8.0970921678014997e-317
	};
	double ph, pl;
	if ((power >= 0) && (power <= 22)) {
		stbsp__ddmulthi(ph, pl, d, stbsp__bot[power]);
	} else {
		stbsp__int32 e, et, eb;
		double p2h, p2l;

		e = power;
		if (power < 0)
			e = -e;
		et = (e * 0x2c9) >> 14; /* %23 */
		if (et > 13)
			et = 13;
		eb = e - (et * 23);

		ph = d;
		pl = 0.0;
		if (power < 0) {
			if (eb) {
				--eb;
				stbsp__ddmulthi(ph, pl, d, stbsp__negbot[eb]);
				stbsp__ddmultlos(ph, pl, d, stbsp__negboterr[eb]);
			}
			if (et) {
				stbsp__ddrenorm(ph, pl);
				--et;
				stbsp__ddmulthi(p2h, p2l, ph, stbsp__negtop[et]);
				stbsp__ddmultlo(p2h, p2l, ph, pl, stbsp__negtop[et], stbsp__negtoperr[et]);
				ph = p2h;
				pl = p2l;
			}
		} else {
			if (eb) {
				e = eb;
				if (eb > 22)
					eb = 22;
				e -= eb;
				stbsp__ddmulthi(ph, pl, d, stbsp__bot[eb]);
				if (e) {
					stbsp__ddrenorm(ph, pl);
					stbsp__ddmulthi(p2h, p2l, ph, stbsp__bot[e]);
					stbsp__ddmultlos(p2h, p2l, stbsp__bot[e], pl);
					ph = p2h;
					pl = p2l;
				}
			}
			if (et) {
				stbsp__ddrenorm(ph, pl);
				--et;
				stbsp__ddmulthi(p2h, p2l, ph, stbsp__top[et]);
				stbsp__ddmultlo(p2h, p2l, ph, pl, stbsp__top[et], stbsp__toperr[et]);
				ph = p2h;
				pl = p2l;
			}
		}
	}
	stbsp__ddrenorm(ph, pl);
	*ohi = ph;
	*olo = pl;
}

#undef stbsp__ddmulthi
#undef stbsp__ddrenorm
#undef stbsp__ddmultlo
#undef stbsp__ddmultlos

struct TenPowers
{
	unsigned long long pow[20];
	unsigned char rev[64];
	constexpr TenPowers() : pow(), rev() {
		unsigned long long p = 1;

		for(int i = 0; i < 20; i++)
		{
			pow[i] = p;
			p *= 10;
		}
		for(int i = 0; i < 64; i++)
		{
			char b = 0;
			for(int j = 0; j < 20; j++)
				if(pow[j] <= 1ULL << i) b = j + 1;
			rev[i] = b;
		}
	}
};
forceinline static inline size_t GetMsb(unsigned long long n)
{
#ifdef __GNUC__
	return __builtin_clzll(n) ^ 0x3F;
#else
	double ff = (double)n;
	return ((*(1+(unsigned int *)&ff))>>20)-1023;
#endif
}
constexpr TenPowers tenPowers;

// given a float value, returns the significant bits in bits, and the position of the
//   decimal point in decimal_pos.  +/-INF and NAN are specified by special values
//   returned in the decimal_pos parameter.
// frac_digits is absolute normally, but if you want from first significant digits (got %g and %e), or in 0x80000000
static inline stbsp__int32 fmtutil_stbsp__real_to_str(char const **start, stbsp__uint32 *len, char *out, stbsp__int32 *decimal_pos, double d, stbsp__uint32 frac_digits)
{
	constexpr static stbsp__uint64 const stbsp__tento19th = 1000000000000000000ULL;
	stbsp__int64 bits = 0;
	stbsp__int32 expo, e, ng, tens;

	STBSP__COPYFP(bits, d);
	expo = (stbsp__int32)((bits >> 52) & 2047);
	ng = (stbsp__int32)((stbsp__uint64) bits >> 63);
	if (ng)
		d = -d;

	if (expo == 2047) // is nan or inf?
	{
		*start = (bits & ((((stbsp__uint64)1) << 52) - 1)) ? "NaN" : "Inf";
		*decimal_pos = STBSP__SPECIAL;
		*len = 3;
		return ng;
	}

	if (expo == 0) // is zero or denormal
	{
		if (((stbsp__uint64) bits << 1) == 0) // do zero
		{
			*decimal_pos = 1;
			*start = out;
			out[0] = '0';
			*len = 1;
			return ng;
		}
		// find the right expo for denormals
		{
			stbsp__int64 v = ((stbsp__uint64)1) << 51;
			while ((bits & v) == 0) {
				--expo;
				v >>= 1;
			}
		}
	}

	// find the decimal exponent as well as the decimal bits of the value
	{
		double ph, pl;

		// log10 estimate - very specifically tweaked to hit or undershoot by no more than 1 of log10 of all expos 1..2046
		tens = expo - 1023;
		tens = (tens < 0) ? ((tens * 617) / 2048) : (((tens * 1233) / 4096) + 1);

		// move the significant bits into position and stick them into an int
		fmtutil_stbsp__raise_to_power10(&ph, &pl, d, 18 - tens);

		// get full as much precision from double-double as possible
		stbsp__ddtoS64(bits, ph, pl);

		// check if we undershot
		if (((stbsp__uint64)bits) >= stbsp__tento19th)
			++tens;
	}

	// now do the rounding in integer land
	frac_digits = (frac_digits & 0x80000000) ? ((frac_digits & 0x7ffffff) + 1) : (tens + frac_digits);
	if ((frac_digits < 24)) {
		stbsp__uint32 dg = 1;
		if ((stbsp__uint64)bits >= tenPowers.pow[9])
			dg = 10;
		#pragma GCC unroll 128
		for( int i = dg; i < 20; i++)
			dg += (stbsp__uint64)bits >= tenPowers.pow[i];

		if (frac_digits < dg || dg == 20) {
			stbsp__uint64 r;
			// add 0.5 at the right position and round
			e = dg - frac_digits;
			if ((stbsp__uint32)e < 24)
			{
				r = tenPowers.pow[e];
				bits = bits + (r / 2);
				if ((stbsp__uint64)bits >= tenPowers.pow[dg])
					++tens;
				bits /= r;
			}
		}
	}

	// kill long trailing runs of zeros
	while(!(bits % 1000))
		bits /= 1000;

	// convert to string
	out += 24;
	e = 0;
	stbsp__uint64 n = bits;
#pragma GCC unroll 128
	for(int i = 0; i < 19; i++)
	{
		*out = (n % 10) + '0';
		n /= 10;
		if(!n)
			break;
		out --, e++;
	}
	if ((e) && (out[0] == '0'))
		out++,e--;
	e++;
	*decimal_pos = tens;
	*start = out;
	*len = e;
	return ng;
}
#undef stbsp__ddtoS64
#undef STBSP__COPYFP

// Type traits

template<class A, class B>
struct CompareTypes_w {
	constexpr static bool res  = false;
};

template<class T>
struct CompareTypes_w<T, T>{
	constexpr static bool res  = true;
};

// if constexpr(CompareTypes(T,int)) { ... }
#define CompareTypes(A,B) (CompareTypes_w<A,B>::res)

// OverloadSet utility
template<class... Fs> struct FunctionOverloadSet{};

template<class F0, class...Fs>
struct FunctionOverloadSet<F0, Fs...> : F0, FunctionOverloadSet<Fs...>
{
	constexpr FunctionOverloadSet(F0 f0, Fs... fs) : F0(f0), FunctionOverloadSet<Fs...>(fs...){}
	using F0::operator();
	using FunctionOverloadSet<Fs...>::operator();
};

template<class F>struct FunctionOverloadSet<F> : F
{
	constexpr FunctionOverloadSet(F f) :F(f){};
	using F::operator();
};
template<class...Fs> constexpr FunctionOverloadSet<Fs...> FunctionOverload(Fs... fs)
{
	return {fs...};
}

// OverloadCheck(can_call, &ptr, x->Method(x))
// OverloadCheck(can_cast, &ptr, (type)x)
// if constexpr(can_cast && can_call) { ... }
// can be combined to single expression in clang, but gcc8 fails with lambda in unelevated context
#define OverloadCheck(result,pointer,expr) \
	constexpr auto _gL_##result = FunctionOverload(\
		[](auto&& x) -> decltype(expr, 1U) {return{};}, \
		[](...)      -> char { return {}; } \
	);constexpr auto result = sizeof(decltype(_gL_##result(pointer))) == 4;

// all supported format specifiers
enum Formats {
	FMT_UNKNOWN = 0,
	FMT_STRING,
	FMT_CHAR,
	FMT_BIN,
	FMT_OCT,
	FMT_HEX,
	FMT_FLOAT,
	FMT_FLOAT_EXP,
	FMT_FLOAT_AUTO,
	FMT_INT,
	FMT_UINT
};

// internal output buffer type
// helps incapsulate write operations
// preventing compiler from trying unroll write
// and fail all unrols at all
template <size_t blen> struct FixedOutputBuffer
{
	char *buf;
	size_t left = blen;
	forceinline void w(char c)
	{
		*buf = c;
		buf += left > 0;
		left--;
	}

	forceinline void w(const char *s, int len)
	{
#pragma GCC unroll 8
		for(size_t t = 0; t < len; t++ )
			w(s[t]);
	}

};

struct OutputBuffer
{
	char *buf;
	char *end;
	forceinline void w(char c)
	{
		// branchless bounds checking
		// with branching compiler may generate
		// muuuch useless conditional jumps for each char
		*buf = c;
		buf += ((size_t)(buf - end)) >> 63;
	}

	forceinline void w(const char *s, int len)
	{
#pragma GCC unroll 8
		for(size_t t = 0; t < len; t++ )
			w(s[t]);
	}
};

// Format wrappers

// octal, hex or binary
template<unsigned int bits,typename Buf, class Arg>
forceinline static inline void ConvertH(Buf &s, Arg arg, char fmtc, size_t fw, char lead)
{
	OverloadCheck(can_convert, &arg, (unsigned long long)*x);
	if constexpr(can_convert)
	{
		// allow uppercase by setting 0x10 (1<<4) bit
		constexpr const char hex[] = "0123456789abcdef0123456789ABCDEF";
		unsigned long long n = (unsigned long long)arg;
		unsigned int chars = (GetMsb(n|1))/ bits + 1;
		if(fw < chars)
			fw = chars;
		if(fw == 0)
			fw = sizeof(Arg)*2;
		if constexpr(bits == 3)
			fw++;

		while(fw)
			s.w(hex[(n >> bits *--fw)& ((1 << bits) - 1) | (fmtc == 'X') << 4]);
	}
	else
		s.w(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__));
}

// integer
template<typename Buf, class Arg>
forceinline static inline void ConvertI(Buf &s, Arg arg, char fmtc, size_t fw, char lead, char sign)
{
	OverloadCheck(can_convert, &arg, (unsigned long long)*x);
	// cannot change sign on pointers or sign-compare with zero, so it's special case
	OverloadCheck(pointer_type, &arg, **x);
	if constexpr(can_convert)
	{
		unsigned long long n = (unsigned long long)arg;

		// invert signed types
		if constexpr(!pointer_type && !CompareTypes(Arg,void*))
		{
			if(arg < 0 && fmtc != 'u')
				n = -arg, sign = '-';
		}

		int b = tenPowers.rev[GetMsb(n|1)];
		int i = 0;
		if(fw && sign)
			fw--;
		if(sign && lead == '0')
			s.w(sign), sign = 0;
		int l = b;
		while(l++ < fw)
			s.w(lead);
		if(sign)
			s.w(sign);
		const char *digitpairs = "00010203040506070809101112131415161718192021222324"
								 "25262728293031323334353637383940414243444546474849"
								 "50515253545556575859606162636465666768697071727374"
								 "75767778798081828384858687888990919293949596979899";
		int b2 = b;
		if( b & 1)
			n = n * 10, b++,i++;
		if(b > s.left)
			b = b2 = s.left;
		s.buf += b;
		while(i < b)
		{
			memcpy(s.buf-=2, &digitpairs[(n % 100) << 1] + (b & 1), 2);
			n /= 100;
			i+=2;
		}
		s.buf += b2, s.left -= b2;
	}
	else
		s.w(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__));
}

// float without exponent
template<typename Buf, class Arg>
forceinline static inline void ConvertF(Buf &s, Arg arg, size_t fw, int pr, char lead, char sign )
{
	OverloadCheck(can_convert, &arg, (double)*x);
	if constexpr(can_convert)
	{
		// just skip heavy bloating point if argument is not floating
		if constexpr(((Arg)0.1f) != (Arg)0.0f)
		{
			char s1[32];
			const char *s2; // digits start
			unsigned int l; // raw digits len
			unsigned int n = 10;
			int dp; // decimal point

			if(pr <0)
				pr = 6;

			if(pr == 6 && fw == 0 && arg > 0.00001 && arg < 999999.0 )
			{
				unsigned long long low1= (arg * 1000000.0);
				unsigned int low = low1 % 1000000;
				unsigned int high = arg;
				unsigned int i;
				for(i = 100000; high < i; i /= 10);
				while(i)
				{
					s.w('0' + (high / i) % 10);
					i /= 10;
				}
				if(low)
				{
					s.w('.');
					for(i = 100000; i; i /= 10)
					{
						int p = low / i;
						low = low - p * i;
						if(!low && !p)
							break;
						s.w('0' + p);
					}
				}
			}
			else
			{
				// get number in digit form with decimal point position
				if(fmtutil_stbsp__real_to_str(&s2, &l, s1, &dp, (double)arg,pr))
				{
					if(fw)
						fw--;
					sign = '-';
				}

				// handle field width and sign
				if(dp != STBSP__SPECIAL)
				{
					// only extending is supported
					if(lead == '0')
						s.w(sign),sign = 0;
					if(dp > l)
					{
						while(fw-- > dp - 2)
							s.w(lead);
					}
					else if(dp > 0)
					{
						while(fw-- > l - 2)
							s.w(lead);
					}
					if(sign)
						s.w(sign);
				}

				// limit array and
				((char*)s2)[l] = 0;

				// do actual copy, place decimal point and
				if(dp != STBSP__SPECIAL)
				{
					if(dp <= 0) // 0.00000x
					{
						unsigned int n1 = -dp;
						if((int)n1>pr)
						{
							// cut number for given precision
							n1 = n = pr;
						}
						s.w('0');
						if(pr)
							s.w('.');
						while(n1--)
							s.w('0');
					}
					else
					{
						if(dp > l) // XX00000.00000
						{
							n = 0;
							while(*s2 && n < l)
								s.w(*s2++),n++;
							while(n < dp)
								s.w('0'),n++;
							if(*s2)
								s.w('.');
						}
						else // 00XXX.XXX
						{
							int n1 = 0;
							while(*s2 && n1 < dp)
								s.w(*s2++),n1++;
							if(*s2)
								s.w('.');
						}
					}
				}

				while(*s2 && n > 0)
					s.w(*s2++),n--;
			}
		}
		else // exclude integer types during template unfolding
			ConvertI(s,arg,0,fw,lead,sign);
	}
	else
		s.w(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__));
}

// expoonent float format
template<typename Buf, class Arg>
forceinline static inline void ConvertE(Buf &s, Arg arg, size_t fw, int pr, char sign )
{
	OverloadCheck(can_convert, &arg, (double)*x);
	if constexpr(can_convert)
	{
		// just skip heavy bloating point if argument is not floating
		if constexpr(((Arg)0.1f) != (Arg)0.0f)
		{
			char s1[32];
			const char *s2; // digits start
			unsigned int l; // raw digits len
			unsigned int n = 10;
			int dp; // decimal point

			if(pr <0)
				pr = 6;

			if(fmtutil_stbsp__real_to_str(&s2, &l, s1, &dp, (double)arg,pr))
			{
				if(fw)
					fw--;
				sign = '-';
			}


			if(sign)
				s.w(sign);

			if(dp != STBSP__SPECIAL)
			{
				// first digit and point
				s.w(*s2++);
				s.w('.');
				if ((l - 1) > (unsigned int)pr)
					l = pr + 1;
			}
			// copy leftover
			((char*)s2)[l] = 0;

			while(*s2 && n > 0)
				s.w(*s2++),n--;

			// write exponent suffix
			if(dp != STBSP__SPECIAL)
			{
				s.w('e');
				if(dp < 0)
				{
					dp = -dp;
					s.w('-');
				}
				else
					s.w('+');
				n = (dp >= 100) ? 5 : 4;
				while( n-- > 3)
				{
					s.w('0' + (dp % 10));
					dp/=10;
				}
			}
		}
		else
			ConvertI(s,arg,0,fw,' ',sign);
	}
	else
		s.w(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__));
}

// autodetect exponent/float formats
template<typename Buf, class Arg>
forceinline static inline void ConvertG(Buf &s, Arg arg, size_t fw, int pr, char sign )
{
	OverloadCheck(can_convert, &arg, (double)*x);
	if constexpr(can_convert)
	{
		// just skip heavy bloating point if argument is not floating
		if constexpr(((Arg)0.1f) != (Arg)0.0f)
		{
			Arg a = arg < 0.0f ? -arg : arg;
			if((a< 0.01f) || (a > 10000000.0))
				ConvertE(s,arg,fw,pr,sign);
			else
				ConvertF(s,arg,fw,pr,' ',sign);
		}
		else
			ConvertI(s,arg,0,fw,' ',sign);
	}
	else
		s.w(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__));
}

// strcpy-like, fields are not supported
template<typename Buf, class Arg>
forceinline static inline void ConvertS(Buf &s, Arg arg)
{
	OverloadCheck(can_convert, &arg, (const char)(*x)[1]);
	if constexpr(can_convert)
	{
		size_t l;
		for(l = 0; arg[l]; l++ )
			s.w(arg[l]);
	}
	else
		s.w(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__));
}

// single char
template<typename Buf, class Arg>
forceinline static inline void ConvertC(Buf &s, Arg arg)
{
	OverloadCheck(can_convert, &arg, (const char)(*x));
	if constexpr(can_convert)
			s.w((const char)arg);
	else
		s.w(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__));
}
/// TODO: Use OverloadSet to implement custom prints

// Bake format modifiers to array
// Compile-time loop unrolling seems to work much better
// with array reads than with checking multiple constants
struct BakedFlags {
#if 1
	constexpr BakedFlags() : arr() {
		for (auto i = 0; i != 127; ++i)
		{
			unsigned int type = 0;
			char sign = 0;
			char lead = 0;
			bool skip = false;

			switch ((char)i) {
			// types
			case 'o':
			case 'O':
				type = FMT_OCT;
				break;
			case 'b':
			case 'B':
				type = FMT_BIN;
				break;
			case 'x':
			case 'X':
			case 'p':
			case 'P':
				type = FMT_HEX;
				break;
			case 'd':
			case 'i':
				type = FMT_INT;
				break;
			case 'u':
				type = FMT_UINT;
				break;
			case 'g':
			case 'G':
				type = FMT_FLOAT_AUTO;
				break;
			case 'f':
			case 'F':
				type = FMT_FLOAT;
				break;
			case 'e':
			case 'E':
				type = FMT_FLOAT_EXP;
				break;
			case 'c':
				type = FMT_CHAR;
				break;
			case 's':
				type = FMT_STRING;
				break;
			// flags
			case '+':
			   sign = '+';
			   skip = true;
			   break;
			case '0':
			   lead = '0';
			   skip = true;
				break;
			case ' ':
			   lead = ' ';
			   skip = true;
			   break;

			case '#':
			case '-':
			// sizes
			case 'h':
			case 'l':
			case 'j':
			case 'z':
			case 't':
				skip = true;
			default:
				break;
			}
			arr[i] = sign << 8 | lead << 16 | skip << 4 | type;
		}
	}

	unsigned int arr[256];
#else
// left just for checking if constexpr affecting compile time
	unsigned int arr[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2097168, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 11024, 0, 16, 0, 0, 3145744, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 6, 5, 7, 0, 0, 0, 0, 0, 0, 0, 3, 4, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 8, 6, 5, 7, 16, 8, 16, 0, 16, 0, 0, 3, 4, 0, 0, 0, 16, 9, 0, 0, 4, 0, 16, 0, 0, 0, 0};
#endif
};

template<typename Buf, class Arg, size_t fmtlen>
static inline forceinline size_t PrintArgument(Buf &s, size_t p1, const char (&fmt)[fmtlen], const Arg &arg)
{
	size_t p;
	constexpr const BakedFlags baked_flags = BakedFlags();
#pragma GCC unroll 128
	for(p = p1; p < fmtlen; p++ )
	{
		if(p && fmt[p] =='%' && fmt[p-1] == '%')
		{
			// touching iterator will break unrolling, so check previous
		}
		else if(fmt[p] == '%' && fmt[p+1] != '%')
		{
			// branchless flag reading
#define UNPACK_NEXT_CHAR \
	f &= ~(1U << 4); \
	f |= baked_flags.arr[(unsigned char)fmt[p+off+1]]; \
	off += (f & (1U << 4)) >> 4

			unsigned int f = baked_flags.arr[(unsigned char)fmt[p+1]];
			size_t off = (f & (1U << 4)) >> 4;
			size_t fw = 0;
			int pr = -1;

			UNPACK_NEXT_CHAR;
			UNPACK_NEXT_CHAR;

			// field width
			while ((fmt[p+off+1] >= '0') && (fmt[p+off+1] <= '9')) {
			   fw = fw * 10 + fmt[p+off+1] - '0';
			   off++;
			}

			// precision, up to 100 digits
			if (fmt[p+off+1] == '.')
			{
				off++;
				pr = 0;
				if(fmt[p+off+1] >='0' && fmt[p+off+1] < '9')
					pr = fmt[p+off+1] - '0',off++;
				if(fmt[p+off+1] >='0' && fmt[p+off+1] < '9')
					pr = pr * 10 + fmt[p+off+1] - '0',off++;
			}

			// skip up to two size modifiers and stop on format
			UNPACK_NEXT_CHAR;
			UNPACK_NEXT_CHAR;

			f &= ~(1U << 4);
			f |= baked_flags.arr[(unsigned char)fmt[p+off+1]];
			//off += (f & (1U << 4)) >> 4;

			// actual format char
			char fmtc = fmt[p + off +1];

			// unpack baked data
			int intf = f & 0xF;
			char lead = (f >> 16U) & 0xFF;
			if(!lead) lead = ' ';
			char sign = (f >> 8U) & 0xFF;

			if(intf== FMT_INT)
			{
				ConvertI(s, arg,fmtc, fw,  lead, sign);
			}
			else if(intf == FMT_FLOAT_AUTO)
			{
				ConvertG(s,arg,fw, pr, sign);
			}
			else if(intf == FMT_FLOAT_EXP)
			{
				ConvertE(s,arg,fw, pr, sign);
			}
			else if(intf == FMT_FLOAT)
			{
				ConvertF(s,arg,fw, pr, lead, sign);
			}
			else if(intf == FMT_STRING)
			{
				ConvertS(s,arg);
			}
			else if(intf == FMT_CHAR)
			{
				ConvertC(s,arg);
			}
			else if(intf == FMT_HEX)
			{
				ConvertH<4>(s,arg, fmtc, fw, lead);
			}
			else if(intf == FMT_OCT)
			{
				ConvertH<3>(s,arg, fmtc, fw, lead);
			}
			else if(intf == FMT_BIN)
			{
				ConvertH<1>(s,arg, fmtc, fw, lead);
			}
			else // error symbol
				s.w('$');
			return p + off + 2;
		}
		else
			s.w(fmt[p]);
	}
	return p;
}

// arguments unfolding
template<typename Buf, size_t fmtlen,  typename Arg> forceinline inline void SPrint_impl(Buf &s, size_t p, char const (&fmt)[fmtlen], const Arg& arg)
{
	size_t ss = PrintArgument(s, p,  fmt,arg);
	// copy trailing format chars or '%''s for missing args
	while(ss<fmtlen)
		s.w(fmt[ss++]);
}

template<typename Buf, size_t fmtlen> forceinline inline void SPrint_impl(Buf &s, size_t p, char const (&fmt)[fmtlen])
{
	size_t ss = 0;
	while(ss<fmtlen)
		s.w(fmt[ss++]);
}

template<typename Buf, size_t fmtlen, typename Arg, typename ... Args> forceinline inline void SPrint_impl(Buf &s, size_t p, char const (&fmt)[fmtlen], const Arg& arg, const Args& ... args)
{
	size_t ss = PrintArgument(s, p, fmt,arg);
	SPrint_impl(s, ss, fmt, args...);
}

// interface
///TODO: hide under namespace or change naming
template<size_t fmtlen, size_t buflen, typename ... Args> forceinline inline int SBPrint(char (&buf)[buflen], char const (&fmt)[fmtlen], const Args& ... args)
{
	FixedOutputBuffer<buflen> s{buf};
	SPrint_impl(s, 0, fmt, args...);
	return s.buf - buf;
}

template<size_t fmtlen, typename ... Args> forceinline inline int SNPrint(char *buf, size_t len, char const (&fmt)[fmtlen], const Args& ... args)
{
	OutputBuffer s{buf, &buf[len]};
	SPrint_impl(s, 0, fmt, args...);
	return s.buf - buf;
}

#ifdef FMT_ENABLE_STDIO
#include<stdio.h>
template<size_t fmtlen, typename ... Args> forceinline inline int FPrint(FILE *f, char const (&fmt)[fmtlen], const Args& ... args)
{
	char buf[1024];
	FixedOutputBuffer<1024> s{buf};
	SPrint_impl(s, 0, fmt, args...);
	fputs(buf,f);
	return s.buf - buf;
}
#endif


#endif // FMT_UTIL_H
/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2024 mittorn@disroot.org
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
