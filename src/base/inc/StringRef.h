/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_STRINGREF_H_060C3DFF817310B78F30FAC909F07EB9FB2CDF2C
#define KYLA_STRINGREF_H_060C3DFF817310B78F30FAC909F07EB9FB2CDF2C

#include <cstring>
#include <string>
#include <vector>

#include "ArrayAdapter.h"
#include "Types.h"

namespace kyla {
class StringRef final
{
public:
	typedef const char* const_iterator;

	template <int Size>
	inline StringRef (const char (&str)[Size])
	: length_ (Size)
	, data_ (str)
	{
	}

	inline StringRef (const char* str)
	{
		if (str == nullptr) {
			length_ = 0;
			data_ = nullptr;
		} else {
			length_ = std::strlen (str);
			data_ = str;
		}
	}

	inline StringRef (const std::string& s)
		: length_ (s.size ())
		, data_ (s.c_str ())
	{
	}

	inline StringRef (const char* str, const int64 length)
	: length_ (length)
	, data_ (str)
	{
	}

	inline StringRef (const int64 length, const char* str)
	: length_ (length)
	, data_ (str)
	{
	}

	inline StringRef (const char& str)
	: length_ (1)
	, data_ (&str)
	{
	}

	inline StringRef (const char* begin, const char* end)
	: length_ (end - begin)
	, data_ (begin)
	{
	}

	inline StringRef ()
	: length_ (0)
	, data_ (nullptr)
	{
	}

	const char* begin () const
	{
		return data_;
	}

	const char* end () const
	{
		return data_ + length_;
	}

	const char* cbegin () const
	{
		return data_;
	}

	const char* cend () const
	{
		return data_ + length_;
	}

	bool IsEmpty () const
	{
		return length_ == 0;
	}

	bool operator== (const StringRef& str) const;
	bool operator!= (const StringRef& str) const;

	bool operator== (const char* str) const;
	bool operator!= (const char* str) const;

	std::string ToString () const;

	int64 GetLength () const
	{
		return length_;
	}

	int64 GetByteCount () const
	{
		return length_;
	}

	char operator[] (const int64 index) const
	{
		return data_ [index];
	}

	friend bool operator== (const char* str, const StringRef& ref)
	{
		return ref == str;
	}

	friend bool operator!= (const char* str, const StringRef& ref)
	{
		return ref != str;
	}

	std::size_t GetHash () const;

	bool operator< (const StringRef& str) const;

	const char* GetData () const
	{
		return data_;
	}

	StringRef SubString (const int64 start) const;
	StringRef SubString (const int64 start, const int64 length) const;

private:
	int64		length_;
	const char*	data_;

	void CheckRange (const int64 start, const int64 length) const;
};

template <>
struct ArrayAdapter<StringRef>
{
	static const char* GetDataPointer (const StringRef& s)
	{
		return s.GetData ();
	}

	static int64 GetSize (const StringRef& s)
	{
		return s.GetByteCount ();
	}

	static int64 GetCount (const StringRef& s)
	{
		return s.GetLength ();
	}

	typedef const char Type;
};
} // namespace kyla
#endif
