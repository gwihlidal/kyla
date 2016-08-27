/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matth�us G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_BASE_REPOSITORY_H
#define KYLA_CORE_INTERNAL_BASE_REPOSITORY_H

#include "Repository.h"

namespace kyla {
class BaseRepository : public Repository
{
public:
	virtual ~BaseRepository () = default;

private:
	virtual std::vector<Uuid> GetFilesetsImpl () override;
	virtual std::string GetFilesetNameImpl (const Uuid& filesetId) override;
	virtual int64_t GetFilesetFileCountImpl (const Uuid& filesetId) override;
	virtual int64_t GetFilesetSizeImpl (const Uuid& filesetId) override;

	void RepairImpl (Repository& source) override;
	void ValidateImpl (const ValidationCallback& validationCallback) override;
	void ConfigureImpl (Repository& other,
		const ArrayRef<Uuid>& filesets,
		Log& log, Progress& progress) override;
};
}

#endif
