#ifndef RPN_CALC_H
#define RPN_CALC_H
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
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⡯⠀⠀⠀⢠⡐⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣠⣤⣤⣤⣴⣦⣾⣿⣿⣿⣶⣦⣴⣶⣶⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⢀⣽⣿⣿⡟⢻⣿⣿⣿⡟⣿⣿⣿⣿⠉⠉⠁⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⣿⣿⣠⣾⣿⣿⣏⣤⣾⣿⣿⣿⡆⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡧⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⢻⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠛⡄⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣿⠁⠉⠙⠛⠛⢿⣿⠟⠋⠁⠀⠀⠀⠈⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⠀⠀⠀⠀⠸⣿⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡘⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
*/
#include "container_utl.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

template<size_t len>
forceinline static inline bool CharIn(const char &c, const char (&chars)[len])
{
	for(int i = 0; i < len - 1; i++)
		if(chars[i] == c)
			return true;
	return false;
}

bool CalcIsOp(char c)
{
	return CharIn(c,"+-/*!%<>=");
}

int CalcArgCount(char op, char op2)
{
	if(CharIn(op,"*/%+-=><"))
		return 2;
	if(op == '!')
		return 1 + (op2 == '=');
	return 0;
}

template<typename T>
T CalcOp2(T val1, T val2, char op, char op2 )
{
	bool eq = op2 == '=';
	if(op == '*')
		return val1 * val2;
	if(op == '/')
		return val1 / val2;
	if(op == '%')
		return fmodf(val1, val2);
	if(op == '+')
		return val1 + val2;
	if(op == '-')
		return val1 - val2;
	if(op == '=' )
		return eq? val1 == val2: val2;
	if(op == '!' && eq)
		return val1 != val2;
	if(op == '>')
		return val1 > val2 || (eq && val1 == val2);
	if(op == '<')
		return val1 < val2 || (eq && val1 == val2);
	return -1;
}

#define IsOperatorToken(c) CharIn(c,"+-/*!%<>=(),")
template <typename Token>
bool ParseTokens( void *priv, GrowArray<Token> &arr, const char *string, size_t len = -1)
{
	bool is_op = IsOperatorToken(string[0]);;
	const char *tok_begin = string;
	char c;
	while((c = *string++) && len--)
	{
		//printf("%c %d\n", c, is_op);
		if(IsOperatorToken(c) && !(is_op && (c == '-')))
		{
			const char *end = string - 1;
			while(end - 1 > tok_begin && *(end - 1) == ' ')
				end--;
			if(tok_begin < end)
				arr.Add(Token(priv, tok_begin, end, false));
			const char *beg = string - 1;
			if(*string == '=')
				string++;
			arr.Add(Token(priv, beg, string, true));
			while(*string == ' ')
				string++;
			tok_begin = string;
			is_op = true;
		}
		else
			is_op = false;
	}
	const char *end = string - 1;
	while(end - 1 > tok_begin && *(end - 1) == ' ')
		end--;
	if(tok_begin < end)
		arr.Add(Token(priv, tok_begin, string - 1, false));
	return true;
}
#undef IsOperatorToken

int OpPri(const char c)
{
	if(c == '!')
		return 5;
	if(CharIn(c, "*/%"))
		return 4;
	if(CharIn(c, "+-"))
		return 3;
	if(CharIn(c, "<>"))
		return 2;
	if(c == '=')
		return 1;
	return 0;
}

#define PopOut(st) do{output.Add(st);stack.RemoveAt(stack.count - 1);}while(0)
template <typename Token>
bool PopBraces(GrowArray<Token> &stack, GrowArray<Token> &output)
{
	while(stack.count > 0)
	{
		Token &st = stack[stack.count - 1];
		if(st.Op() == '(')
			return true;
		else
			PopOut(st);
	}
	return false;
}
template <typename Token>
bool ShuntingYard(GrowArray<Token> &input, GrowArray<Token> &output)
{
	GrowArray<Token> stack;
	int pos = 0;
	while(pos < input.count)
	{
		const Token &t = input[pos];
		char c = t.Op();
		if(t.IsIdent())
			output.Add(t);
		else if(t.IsFunction())
			stack.Add(t);
		else if(c == ',')
		{
			if(!PopBraces(stack, output))
				return false;
		}
		else if(t.IsOperator())
		{
			while(stack.count > 0)
			{
				Token &st = stack[stack.count - 1];
				int pris = OpPri(st.Op());
				int pri = OpPri(c);
				int l = CharIn(c,"*/%+-=><");
				if(st.IsOperator() && ((l && (pri <= pris)) || (!l && (pri < pris))))
					PopOut(st);
				else break;
			}
			stack.Add(t);
		}
		else if(c == '(')
			stack.Add(t);
		else if(c == ')')
		{
			if(!PopBraces(stack, output))
				return false;
			stack.RemoveAt(stack.count - 1);
			if(stack.count > 0)
			{
				Token &st = stack[stack.count - 1];
				if(st.IsFunction())PopOut(st);
			}
		}
		else
			return false;
		pos++;
	}
	while(stack.count > 0)
	{
		Token &st = stack[stack.count - 1];
		if(CharIn(st.Op(), "()"))
			return false;
		PopOut(st);
	}
	return true;
}
#undef PopOut

template <typename Token>
bool DumpOrder(GrowArray<Token> &arr)
{
	int pos = 0;
	GrowArray<Token> stack;
	char tempvar[4];
	int tempvar_index = 0;
	while(pos < arr.count)
	{
		Token &t = arr[pos];

		if(t.IsIdent())
			stack.Add(t);
		else if(t.IsOperator() || t.IsFunction())
		{
			char buf[32];
			sprintf(tempvar, "$%d", tempvar_index++);
			printf("%s = ", tempvar);
			int nargs = t.ArgCount();
			if( stack.count < nargs)
				return false;
			t.Stringify(buf,32);
			printf("%s(", buf);
			while(nargs > 0)
			{
				Token &st = stack[stack.count - 1];
				st.Stringify(buf,32);
				printf(nargs> 1?"%s, ": "%s)\n", buf);
				stack.RemoveAt(stack.count - 1);
				nargs--;
			}
			stack.Add(Token(nullptr, tempvar, &tempvar[strlen(tempvar)], false));
		}
		pos++;
	}
	if( stack.count == 1)
	{
		char buf[32];
		Token &st = stack[stack.count - 1];
		st.Stringify(buf,32);
		printf("return %s\n", buf);
		return true;
	}
	return false;
}

template <typename Numeric = float, size_t stacksize = 32, typename Token>
Numeric Calculate(GrowArray<Token> &arr)
{
	Token stack[stacksize];
	size_t sp = 0;
	int pos = 0;
	while(pos < arr.count)
	{
		Token &t = arr[pos];
		if(sp >= stacksize - 1)
			return -1;
		if(t.IsIdent())
			stack[sp++] = t;
		else if(t.IsOperator() || t.IsFunction())
			if(!t.Calculate(stack, sp))
				return -1;
		pos++;
	}
	if( sp == 1)
		return stack[--sp].Val();
	return -1;
}

#ifdef TEST_RPNCALC
struct StringToken
{
	char ch[32] = "";
	bool op = false;
	bool func = false;
	StringToken(){}
	constexpr StringToken(void *priv, const char *begin, const char *end, bool _op)
	{
		char *s = ch;
		while(begin < end && s - ch < 31)
			*s++ = *begin++;
		*s = 0;
		op = _op;
		func = !strcmp(ch, "sin") || !strcmp(ch, "func3");
	}
	constexpr StringToken(float val)
	{
		snprintf(ch, 31, "%f", val);
	}

	char Op() const
	{
		return ch[0];
	}
	bool IsOperator() const
	{
		return op && CalcIsOp(Op());
	}
	bool IsIdent() const
	{
		return !op && !IsFunction();
	}
	bool IsFunction() const
	{
		return func;
	}
	void Stringify(char *buf, size_t len) const
	{
		strncpy(buf, ch, len - 1);
	}
	int ArgCount() const
	{
		if(func)
			return ch[0]=='f'?3:1;
		if(!op)
			return 0;
		return CalcArgCount(ch[0], ch[1]);
	}
	float Val()
	{
		return atof(ch);
	}
#define StackPop(var) auto var = stack[--sp].Val()
#define StackPush(val) stack[sp++] = val
	template<size_t stacksize>
	bool Calculate(StringToken (&stack)[stacksize], size_t &sp)
	{
		int ac = ArgCount();
		if(sp < ac)
			return false;
		if(func)
		{
			if(ch[0] == 's')
			{
				StackPop(val);
				StackPush(sinf(val));
				return true;
			}
			if(ch[0] == 'f')
			{
				StackPop(val3);
				StackPop(val2);
				StackPop(val1);
				StackPush(val1 + val2 + val3);
				return true;
			}
		}
		else if(op)
		{
			if(ac == 2)
			{
				StackPop(val2);
				StackPop(val1);
				float res = CalcOp2(val1, val2, Op(), ch[1]);
				StackPush(res);
				return true;
			}
			if(ac == 1)
			{
				StackPop(val);
				if(Op() == '!')
				{
					StackPush(!val);
					return true;
				}
				else return false;
			}
		}

		return false;
	}
#undef StackPop
#undef StackPush
};

template <typename T>
struct NumericToken
{
	enum
	{
		val,
		op,
		func,
		var
	} mode;
	union
	{
		char ch[2];
		T val;
		int funcindex;
	} d;
	NumericToken(){}
	NumericToken(void *priv, const char *begin, const char *end, bool _op)
	{
		if(!strncmp(begin, "sin", end - begin) || !strncmp(begin, "func3", end - begin))
		{
			mode = func;
			d.funcindex = *begin == 'f';
		}
		else if(_op)
		{
			mode = op;
			strncpy(d.ch,begin, 2);
			if(end < begin + 2)
				d.ch[end - begin] = 0;
		}
		else if(*begin == '$')
		{
			mode = var;
			d.funcindex = atoi(begin+1);
		}
		else
		{
			mode = val;
			d.val = atof(begin);
		}
	}
	NumericToken(T v)
	{
		mode = val;
		d.val = v;
	}
	char Op() const
	{
		return mode == op ? d.ch[0]:0;
	}
	bool IsOperator() const
	{
		return (mode == op) && CalcIsOp(Op());
	}
	bool IsIdent() const
	{
		return mode == val || mode == var;
	}
	bool IsFunction() const
	{
		return mode == func;
	}
	void Stringify(char *buf, size_t len) const
	{
		if(len < 2)
			return;
		if(mode == op)
		{
			strncpy(buf, d.ch, 2);
			buf[2] = 0;
		}
		else if(mode == val)
		{
			snprintf(buf, len - 1, "%f",(double)Val());
		}
		else if(mode == func)
		{
			snprintf(buf, len - 1, "func%d",d.funcindex);
		}
		else if(mode == var)
		{
			snprintf(buf, len - 1, "$%d",d.funcindex);
		}
	}
	int ArgCount() const
	{
		if(mode == func)
			return d.funcindex?3:1;
		if(mode != op)
			return 0;
		return CalcArgCount(d.ch[0], d.ch[1]);
	}
	T Val() const
	{
		if(mode == val)
			return d.val;
		return 0;
	}
#define StackPop(var) auto var = stack[--sp].Val()
#define StackPush(val) stack[sp++] = val
	template<size_t stacksize>
	bool Calculate(NumericToken (&stack)[stacksize], size_t &sp)
	{
		int ac = ArgCount();
		if(sp < ac)
			return false;
		if(mode == func)
		{
			if(d.funcindex == 0)
			{
				StackPop(val1);
				StackPush(sinf(val1));
				return true;
			}
			if(d.funcindex == 1)
			{
				StackPop(val3);
				StackPop(val2);
				StackPop(val1);
				StackPush(val1 + val2 + val3);
				return true;
			}
		}
		else if(mode == op)
		{
			if(ac == 2)
			{
				StackPop(val2);
				StackPop(val1);
				T res = CalcOp2(val1, val2, Op(), d.ch[1]);
				StackPush(res);
				return true;
			}
			if(ac == 1)
			{
				StackPop(val1);
				if(Op() == '!')
				{
					StackPush(!val1);
					return true;
				}
				else return false;
			}
		}

		return false;
	}
#undef StackPop
#undef StackPush
};

int main(int argc, const char **argv)
{
	const char *input = "a = sin(1) + 2";
	if(argc>1)
		input = argv[1];
	GrowArray<NumericToken<double>> arr;
	ParseTokens(nullptr, arr, input);
	for(int i = 0; i < arr.count; i++)
	{
		char buf[32] = "";
		arr[i].Stringify(buf,31);
		printf("t %s %d|\n", buf, arr[i].Op());
	}

	GrowArray<NumericToken<double>> out;
	if(ShuntingYard(arr, out))
	{
		for(int i = 0; i < out.count; i++)
		{
			char buf[32] = "";
			out[i].Stringify(buf,31);
			printf("o %s %d|\n", buf, out[i].Op());
		}
		DumpOrder(out);
		printf("Value %f\n", Calculate<double>(out));
	}
	else for(int i = 0; i < out.count; i++)
		{
			char buf[32] = "";
			out[i].Stringify(buf,31);
			printf("o %s %d|\n", buf, out[i].Op());
		}
	GrowArray<StringToken> arr1;
	ParseTokens(nullptr, arr1, input);
	for(int i = 0; i < arr1.count; i++)
	{
		char buf[32] = "";
		arr1[i].Stringify(buf,31);
		printf("t %s %d|\n", buf, arr1[i].Op());
	}

	GrowArray<StringToken> out1;
	if(ShuntingYard(arr1, out1))
	{
		for(int i = 0; i < out1.count; i++)
		{
			char buf[32] = "";
			out1[i].Stringify(buf,31);
			printf("o %s %d|\n", buf, out1[i].Op());
		}
		DumpOrder(out1);
		printf("Value %f\n", Calculate<double>(out1));
	}
	else for(int i = 0; i < out1.count; i++)
		{
			char buf[32] = "";
			out1[i].Stringify(buf,31);
			printf("o %s %d|\n", buf, out1[i].Op());
		}
	return 0;
}
#endif

#endif // RPN_CALC_H
