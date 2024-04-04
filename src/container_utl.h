// SPDX-FileCopyrightText: 2024 mittorn <mittorn@disroot.org>
//
// SPDX-License-Identifier: MIT
//

#pragma once
#ifndef CONTAINER_UTL_H
#define CONTAINER_UTL_H
#include <stdlib.h>
#include <string.h>

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

// simpliest vector and hashmap ever?
template<class T>
struct GrowArray
{
	T *mem = nullptr;
	size_t count = 0;
	size_t alloc = 0;

	GrowArray(size_t init = 0)
	{
		if(init)
			Grow(init);
	}
	void Clear()
	{
		if(mem)
			free(mem);
		mem = nullptr;
		count = 0;
		alloc = 0;
	}
	~GrowArray()
	{
		Clear();
	}

	// non-copyable
	GrowArray(const GrowArray &) = delete;
	GrowArray &operator = (const GrowArray &) = delete;

	void noinline Grow(size_t size)
	{
		if(!size)
			size = 32;
		T *newMem = (T*)(mem? realloc(mem, sizeof(T) * size): malloc(sizeof(T)*size));

		if(!newMem)
			return;

		alloc = size;
		mem = newMem;
	}

	// TODO: insert/append
	bool Add(const T& newVal)
	{
		size_t newIdx = count + 1;
		if(unlikely(newIdx > alloc))
			Grow(alloc * 2);
		if(unlikely(newIdx > alloc))
			return false;
		mem[count] = newVal;
		count = newIdx;
		return true;
	}

	// TODO: test
	bool RemoveAt(size_t idx)
	{
		if(idx < count)
			memmove(&mem[idx], &mem[idx+1], sizeof(T) * (count - idx - 1));
		if(idx <= count)
		{
			count--;
			return true;
		}
		return false;
	}
	T& operator[](size_t i) const
	{
		return mem[i];
	}
};

template<typename Key>
forceinline static inline bool KeyCompare(const Key &a, const Key &b)
{
	return a == b;
}
template<>
inline bool KeyCompare(const char * const &a, const char * const &b)
{
	return !strcmp(a,b);
}


template<typename Key>
forceinline static inline const Key &KeyAlloc(const Key &a)
{
	return a;
}
template<>
inline const char * const &KeyAlloc<const char*>(const char * const &a) = delete;

inline const char * const KeyAlloc(const char * const &a)
{
	return strdup((const char*)a);
}

template<typename Key>
forceinline static inline void KeyDealloc(const Key &a){}
template<>
inline void KeyDealloc(const char * const &a)
{
	free((void*)a);
}

template<size_t TblSize, typename Key>
forceinline static inline size_t HashFunc(const Key &key)
{
	// handle hash: handle pointers usually aligned
	return (((size_t) key) >> 8)  & (TblSize - 1);
}

template<size_t TblSize>
forceinline static inline size_t HashFunc(const char * const &key)
{
	size_t hash = 7;
	unsigned char c;
	const unsigned char *s = (const unsigned char*)key;
	while((c = *s++))
		hash = hash * 31 + c;
	return hash & (TblSize - 1);
}

template <typename Key, typename Value, size_t TblPower = 2>
struct HashMap {

	struct Node
	{
		Key k;
		Value v;
		Node *n;

		// non-copyable
		Node(const Node &) = delete;
		Node & operator=(const Node &) = delete;
		Node(const Key &key, const Value &value) :
			k(key), v(value), n(NULL) {}

		Node(const Key &key) :
			k(key), v(), n(NULL) {}
	};

	constexpr static size_t TblSize = 1U << TblPower;
	Node *table[TblSize] = {nullptr};

	HashMap() {
	}
	void Clear()
	{
		for(size_t i = 0; i < TblSize; i++)
		{
			Node *entry = table[i];
			while(entry)
			{
				Node *prev = entry;
				entry = entry->n;
				KeyDealloc(prev->k);
				delete prev;
			}
			table[i] = NULL;
		}
	}

	~HashMap() {
		Clear();
	}

	// just in case: check existance or constant access
	forceinline Node *GetNode(const Key &key) const
	{
		size_t hashValue = HashFunc<TblSize>(key);
		Node *entry = table[hashValue];
		while(likely(entry))
		{
			if(KeyCompare(entry->k, key))
				return entry;
			entry = entry->n;
		}
		return nullptr;
	}

	forceinline Value *GetPtr(const Key &key) const
	{
		Node *entry = GetNode(key);
		if(entry)
			return &entry->v;
		return nullptr;
	}

	Value &operator [] (const Key &key)
	{
		return GetOrAllocate(key)->v;
	}

#define HASHFIND(key) \
	size_t hashValue = HashFunc<TblSize>(key); \
		Node *prev = NULL; \
		Node *entry = table[hashValue]; \
	\
		while(entry && !KeyCompare(entry->k, key)) \
	{ \
			prev = entry; \
			entry = entry->n; \
	}

	Node * noinline _Allocate(const Key &key, size_t hashValue, Node *prev, Node *entry)
	{
		entry = new Node(KeyAlloc(key));
		if(unlikely(!entry))
		{
			static Node error(key);
			return &error;
		}

		if(prev == NULL)
			table[hashValue] = entry;
		else
			prev->n = entry;
		return entry;
	}

	Node *GetOrAllocate(const Key &key)
	{
		HASHFIND(key);

		if(unlikely(!entry))
			entry = _Allocate(key,hashValue, prev, entry);

		return entry;
	}

	bool Remove(const Key &key)
	{
		HASHFIND(key);

		if(!entry)
			return false;

		if(prev == NULL)
			table[hashValue] = entry->n;
		else
			prev->n = entry->n;

		KeyDealloc(entry->k);

		delete entry;
		return true;
	}
#undef HASHFIND
};


template <typename Key, typename Value, size_t TblPower = 2>
struct HashArrayMap {

	struct Node
	{
		Key k;
		Value v;

		Node(const Key &key, const Value &value) :
			k(key), v(value){}
		Node(const Key &key) :
			k(key), v(){}
	};

	constexpr static size_t TblSize = 1U << TblPower;
	GrowArray<Node> table[TblSize];

	HashArrayMap() {
	}
	void Clear()
	{
		for(int i = 0; i < TblSize; i++)
		{
			for(int j = 0; j < table[i].count; j++)
				KeyDealloc(table[i][j].k);
			table[i].Clear();
		}
	}

	~HashArrayMap() {
		Clear();
	}

	Node *GetNode(const Key &key) const
	{
		size_t hashValue = HashFunc<TblSize>(key);
		const GrowArray<Node> &entry = table[hashValue];
		for(int i = 0; i < entry.count; i++)
		{
			if( KeyCompare(entry[i].k, key))
				return &entry[i];
		}
		return nullptr;
	}

	// just in case: check existance or constant access
	Value *GetPtr(const Key &key) const
	{
		Node *n = GetNode(key);
		if(n)
			return &n->v;
		return nullptr;
	}

	Value &operator [] (const Key &key)
	{
		return GetOrAllocate(key)->v;
	}

#define HASHFIND(key) \
	GrowArray<Node> &entry = table[HashFunc<TblSize>(key)]; \
		int i; \
		for(i = 0; i < entry.count; i++) \
		if( KeyCompare(entry[i].k, key)) \
		break;

	Node *GetOrAllocate(const Key &key)
	{
		HASHFIND(key);
		if(i == entry.count )
			entry.Add(Node(KeyAlloc(key)));

		return &entry[i];
	}

	bool Remove(const Key &key)
	{
		HASHFIND(key);
		if(i != entry.count)
		{
			KeyDealloc(entry[i]);
			entry.RemoveAt(i);
			return true;
		}
		return false;
	}
#undef HASHFIND
};

template <typename T, size_t Bits = 6>
struct CycleArray
{
	constexpr static size_t size = 1U << Bits;
	constexpr static size_t mask = (1U << Bits) - 1;
	T storage[size];
	forceinline inline T &operator[](size_t idx)
	{
		return storage[idx & mask];
	}
};

template<typename T, size_t Bits = 6>
struct CycleQueue
{
	CycleArray<T,Bits> array;
	size_t in = 0, out = 0;
	forceinline bool Enqueue(const T& el)
	{
		if(in - out > array.size)
			return false;
		array[in++] = el;
		return true;
	}
	forceinline bool Dequeue(T &o)
	{
		if(likely(out == in))
			return false;
		o = array[out++];
		return true;
	}
};

#endif // CONTAINER_UTL_H
