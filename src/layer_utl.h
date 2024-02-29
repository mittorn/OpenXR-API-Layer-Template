// SPDX-FileCopyrightText: 2024 mittorn <mittorn@disroot.org>
//
// SPDX-License-Identifier: MIT
//

#pragma once
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
	~GrowArray()
	{
		if(mem)
			free(mem);
		mem = nullptr;
		count = 0;
		alloc = 0;
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

	~HashMap() {
		for(size_t i = 0; i < TblSize; i++)
		{
			Node *entry = table[i];
			while(entry)
			{
				Node *prev = entry;
				entry = entry->n;
				delete prev;
			}
			table[i] = NULL;
		}
	}

	size_t HashFunc(const Key &key) const
	{
		/// TODO: string hash?
		// handle hash: handle pointers usually aligned
		return (((size_t) key) >> 8)  & (TblSize - 1);
	}

	// just in case: check existance or constant access
	forceinline Value *GetPtr(const Key &key) const
	{
		size_t hashValue = HashFunc(key);
		Node *entry = table[hashValue];
		while(likely(entry))
		{
			if(entry->k == key)
				return &entry->v;
			entry = entry->n;
		}
		return nullptr;
	}

	Value &operator [] (const Key &key)
	{
		return GetOrAllocate(key);
	}

#define HASHFIND(key) \
		size_t hashValue = HashFunc(key); \
		Node *prev = NULL; \
		Node *entry = table[hashValue]; \
	\
		while(entry && entry->k != key) \
		{ \
			prev = entry; \
			entry = entry->n; \
		}

	Node * noinline _Allocate(Key key, size_t hashValue, Node *prev, Node *entry)
	{
		entry = new Node(key);
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

	Value& GetOrAllocate(const Key &key)
	{
		HASHFIND(key);

		if(unlikely(!entry))
			entry = _Allocate(key,hashValue, prev, entry);

		return entry->v;
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

	~HashArrayMap() {
	}

	size_t HashFunc(const Key &key) const
	{
		/// TODO: string hash?
		// handle hash: handle pointers usually aligned
		return (((size_t) key) >> 8)  & (TblSize - 1);
	}

	// just in case: check existance or constant access
	Value *GetPtr(const Key &key) const
	{
		size_t hashValue = HashFunc(key);
		const GrowArray<Node> &entry = table[hashValue];
		for(int i = 0; i < entry.count; i++)
		{
			if(entry[i].k == key)
				return &entry[i].v;
		}
		return nullptr;
	}

	Value &operator [] (const Key &key)
	{
		return GetOrAllocate(key);
	}

#define HASHFIND(key) \
	GrowArray<Node> &entry = table[HashFunc(key)]; \
	int i; \
	for(i = 0; i < entry.count; i++) \
		if( entry[i].k == key ) \
			break;

	Value& GetOrAllocate(const Key &key)
	{
		HASHFIND(key);
		if(i == entry.count )
			entry.Add(Node(key));

		return entry[i].v;
	}

	bool Remove(const Key &key)
	{
		HASHFIND(key);
		if(i != entry.count)
		{
			entry.RemoveAt(i);
			return true;
		}
		return false;
	}
#undef HASHFIND
};
