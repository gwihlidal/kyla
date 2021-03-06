/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_FILEIO_H
#define KYLA_CORE_INTERNAL_FILEIO_H

#include <cstdint>
#include <memory>

#include <boost/filesystem.hpp>

#include "ArrayRef.h"

namespace kyla {
using Path = boost::filesystem::path;

struct File
{
	virtual ~File ();

	File ();
	File (const File&) = delete;
	File& operator= (const File&) = delete;

	void Write (const ArrayRef<>& data)
	{
		WriteImpl (data);
	}

	std::int64_t Read (const MutableArrayRef<>& buffer)
	{
		return ReadImpl (buffer);
	}

	void Seek (const std::int64_t offset)
	{
		SeekImpl (offset);
	}

	std::int64_t Tell () const
	{
		return TellImpl ();
	}

	void* Map (const std::int64_t offset, const std::int64_t size)
	{
		return MapImpl (offset, size);
	}

	void* Map ()
	{
		return MapImpl (0, GetSize ());
	}

	void* Unmap (void* p)
	{
		return UnmapImpl (p);
	}

	void SetSize (const std::int64_t size)
	{
		SetSizeImpl (size);
	}

	std::int64_t GetSize () const
	{
		return GetSizeImpl ();
	}

	void Close ()
	{
		CloseImpl ();
	}

private:
	virtual void WriteImpl (const ArrayRef<>& data) = 0;
	virtual std::int64_t ReadImpl (const MutableArrayRef<>& buffer) = 0;

	virtual void SeekImpl (const std::int64_t offset) = 0;
	virtual std::int64_t TellImpl () const = 0;
	virtual void* MapImpl (const std::int64_t offset, const std::int64_t size) = 0;
	virtual void* UnmapImpl (void* p) = 0;

	virtual void SetSizeImpl (const std::int64_t size) = 0;
	virtual std::int64_t GetSizeImpl () const = 0;

	virtual void CloseImpl () = 0;
};

struct FileStat
{
	std::size_t size;
};

FileStat Stat (const Path& path);
FileStat Stat (const char* path);

enum class FileOpenMode
{
	Read,
	Write,
	ReadWrite
};

std::unique_ptr<File> OpenFile (const char* path, FileOpenMode openMode);
std::unique_ptr<File> CreateFile (const char* path);


std::unique_ptr<File> OpenFile (const Path& path, FileOpenMode openMode);
std::unique_ptr<File> CreateFile (const Path& path);

void BlockCopy (File& input, File& output);
void BlockCopy (File& input, File& output, const MutableArrayRef<byte>& buffer);

Path GetTemporaryFilename ();
}

#endif // FILEIO_H
