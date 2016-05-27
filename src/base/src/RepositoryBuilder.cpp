/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
[LICENSE END]
*/

#include "Hash.h"

#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>
#include <string>

#include "FileIO.h"

#include <pugixml.hpp>

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "Uuid.h"

#include <assert.h>

#include "Log.h"

#include "install-db-structure.h"

#include <map>

#include "sql/Database.h"
#include "Exception.h"

namespace {
struct BuildContext
{
	kyla::Path sourceDirectory;
	kyla::Path targetDirectory;
};

struct File
{
	kyla::Path source;
	kyla::Path target;

	kyla::SHA256Digest hash;
};

struct FileSet
{
	std::vector<File> files;

	std::string name;
	kyla::Uuid id;
};

///////////////////////////////////////////////////////////////////////////////
std::vector<FileSet> GetFileSets (const pugi::xml_document& doc,
	const BuildContext& ctx)
{
	std::vector<FileSet> result;

	int filesFound = 0;

	for (const auto& fileSetNode : doc.select_nodes ("//FileSet")) {
		FileSet fileSet;

		fileSet.id = kyla::Uuid::Parse (fileSetNode.node ().attribute ("Id").as_string ());
		fileSet.name = fileSetNode.node ().attribute ("Name").as_string ();

		for (const auto& fileNode : fileSetNode.node ().children ("File")) {
			File file;
			file.source = fileNode.attribute ("Source").as_string ();

			if (fileNode.attribute ("Target")) {
				file.target = fileNode.attribute ("Target").as_string ();
			} else {
				file.target = file.source;
			}

			fileSet.files.push_back (file);

			++filesFound;
		}

		result.emplace_back (std::move (fileSet));
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
void HashFiles (std::vector<FileSet>& fileSets,
	const BuildContext& ctx)
{
	for (auto& fileSet : fileSets) {
		for (auto& file : fileSet.files) {
			file.hash = kyla::ComputeSHA256 (ctx.sourceDirectory / file.source);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
struct UniqueContentObjects
{
	kyla::Path sourceFile;
	kyla::SHA256Digest hash;
	std::size_t size;

	std::vector<kyla::Path> duplicates;
};

///////////////////////////////////////////////////////////////////////////////
/**
Given a couple of file sets, we find unique files by hashing everything
and merging the results on the hash.
*/
std::vector<UniqueContentObjects> FindUniqueFileContents (std::vector<FileSet>& fileSets,
	const BuildContext& ctx)
{
	std::unordered_map<kyla::SHA256Digest, std::vector<std::pair<kyla::Path, kyla::Path>>,
		kyla::HashDigestHash, kyla::HashDigestEqual> uniqueContents;

	for (const auto& fileSet : fileSets) {
		for (const auto& file : fileSet.files) {
			// This assumes the hashes are up-to-date, i.e. initialized
			uniqueContents [file.hash].push_back (std::make_pair (file.source, file.target));
		}
	}

	std::vector<UniqueContentObjects> result;
	result.reserve (uniqueContents.size ());

	for (const auto& kv : uniqueContents) {
		UniqueContentObjects uf;

		uf.hash = kv.first;
		uf.sourceFile = ctx.sourceDirectory / kv.second.front ().first;

		uf.size = kyla::Stat (uf.sourceFile.string ().c_str ()).size;

		for (const auto& sourceTargetPair : kv.second){
			uf.duplicates.push_back (sourceTargetPair.second);
		}

		result.push_back (uf);
	}

	return result;
}

struct IRepositoryBuilder
{
	virtual ~IRepositoryBuilder ()
	{
	}

	virtual void Build (const BuildContext& ctx,
		const std::vector<FileSet>& fileSets,
		const std::vector<UniqueContentObjects>& uniqueFiles) = 0;
};

/**
A loose repository is little more than the files themselves, with hashes.
*/
struct LooseRepositoryBuilder final : public IRepositoryBuilder
{
	void Build (const BuildContext& ctx,
		const std::vector<FileSet>& fileSets,
		const std::vector<UniqueContentObjects>& uniqueFiles) override
	{
		boost::filesystem::create_directories (ctx.targetDirectory / ".ky");
		boost::filesystem::create_directories (ctx.targetDirectory / ".ky" / "objects");

		auto dbFile = ctx.targetDirectory / ".ky" / "repository.db";
		boost::filesystem::remove (dbFile);

		auto db = kyla::Sql::Database::Create (
			dbFile.string ().c_str ());

		db.Execute (install_db_structure);
		db.Execute ("PRAGMA journal_mode=WAL;");
		db.Execute ("PRAGMA synchronous=NORMAL;");

		const auto fileToFileSetId = PopulateFileSets (db, fileSets);
		PopulateContentObjectsAndFiles (db, uniqueFiles, fileToFileSetId,
			ctx.targetDirectory / ".ky" / "objects");

		db.Execute ("PRAGMA journal_mode=DELETE;");
		// Necessary to get good index statistics
		db.Execute ("ANALYZE");

		db.Close ();
	}

private:
	std::map<kyla::Path, std::int64_t> PopulateFileSets (kyla::Sql::Database& db,
		const std::vector<FileSet>& fileSets)
	{
		auto fileSetsInsert = db.BeginTransaction ();
		auto fileSetsInsertQuery = db.Prepare (
			"INSERT INTO file_sets (Uuid, Name) VALUES (?, ?);");

		std::map<kyla::Path, std::int64_t> result;

		for (const auto& fileSet : fileSets) {
			fileSetsInsertQuery.BindArguments (
				fileSet.id, fileSet.name);

			fileSetsInsertQuery.Step ();
			fileSetsInsertQuery.Reset ();

			const auto fileSetId = db.GetLastRowId ();

			for (const auto& file : fileSet.files) {
				result [file.target] = fileSetId;
			}
		}

		fileSetsInsert.Commit ();

		return result;
	}

	void PopulateContentObjectsAndFiles (kyla::Sql::Database& db,
		const std::vector<UniqueContentObjects>& uniqueFiles,
		const std::map<kyla::Path, std::int64_t>& fileToFileSetId,
		const kyla::Path& contentObjectPath)
	{
		auto contentObjectInsert = db.BeginTransaction ();
		auto contentObjectInsertQuery = db.Prepare (
			"INSERT INTO content_objects (Hash, Size) VALUES (?, ?);");
		auto filesInsertQuery = db.Prepare (
			"INSERT INTO files (Path, ContentObjectId, FileSetId) VALUES (?, ?, ?);");

		// We can insert content objects directly - every unique file is one
		for (const auto& kv : uniqueFiles) {
			contentObjectInsertQuery.BindArguments (
				kv.hash,
				kv.size);
			contentObjectInsertQuery.Step ();
			contentObjectInsertQuery.Reset ();

			const auto contentObjectId = db.GetLastRowId ();

			for (const auto& reference : kv.duplicates) {
				const auto fileSetId = fileToFileSetId.find (reference)->second;

				filesInsertQuery.BindArguments (
					reference.string ().c_str (),
					contentObjectId,
					fileSetId);
				filesInsertQuery.Step ();
				filesInsertQuery.Reset ();
			}

			///@TODO(minor) Enable compression here
			// store the file itself
			boost::filesystem::copy_file (kv.sourceFile,
				contentObjectPath / ToString (kv.hash));
		}

		contentObjectInsert.Commit ();
	}
};

/**
Store all files into one or more source packages. A source package can be
compressed as well.
*/
struct PackedRepositoryBuilder final : public IRepositoryBuilder
{
	void Build (const BuildContext& ctx,
		const std::vector<FileSet>& fileSets,
		const std::vector<UniqueContentObjects>& uniqueFiles) override
	{
		auto dbFile = ctx.targetDirectory / "repository.db";
		boost::filesystem::remove (dbFile);

		auto db = kyla::Sql::Database::Create (
			dbFile.string ().c_str ());

		db.Execute (install_db_structure);
		db.Execute ("PRAGMA journal_mode=WAL;");
		db.Execute ("PRAGMA synchronous=NORMAL;");

		const auto fileToFileSetId = PopulateFileSets (db, fileSets);
		PopulateContentObjectsAndFiles (db, uniqueFiles, fileToFileSetId,
			ctx.targetDirectory);

		db.Execute ("PRAGMA journal_mode=DELETE;");
		// Necessary to get good index statistics
		db.Execute ("ANALYZE");

		db.Close ();
	}

private:
	std::map<kyla::Path, std::int64_t> PopulateFileSets (kyla::Sql::Database& db,
		const std::vector<FileSet>& fileSets)
	{
		auto fileSetsInsert = db.BeginTransaction ();
		auto fileSetsInsertQuery = db.Prepare (
			"INSERT INTO file_sets (Uuid, Name) VALUES (?, ?);");

		std::map<kyla::Path, std::int64_t> result;

		for (const auto& fileSet : fileSets) {
			fileSetsInsertQuery.BindArguments (
				fileSet.id, fileSet.name);

			fileSetsInsertQuery.Step ();
			fileSetsInsertQuery.Reset ();

			const auto fileSetId = db.GetLastRowId ();

			for (const auto& file : fileSet.files) {
				result [file.target] = fileSetId;
			}
		}

		fileSetsInsert.Commit ();

		return result;
	}

	void PopulateContentObjectsAndFiles (kyla::Sql::Database& db,
		const std::vector<UniqueContentObjects>& uniqueFiles,
		const std::map<kyla::Path, std::int64_t>& fileToFileSetId,
		const kyla::Path& packagePath)
	{
		auto contentObjectInsert = db.BeginTransaction ();
		auto contentObjectInsertQuery = db.Prepare (
			"INSERT INTO content_objects (Hash, Size) VALUES (?, ?);");
		auto filesInsertQuery = db.Prepare (
			"INSERT INTO files (Path, ContentObjectId, FileSetId) VALUES (?, ?, ?);");
		auto packageInsertQuery = db.Prepare (
			"INSERT INTO source_packages (Name, Filename, Uuid) VALUES (?, ?, ?)");
		auto storageMappingInsertQuery = db.Prepare (
			"INSERT INTO storage_mapping "
			"(ContentObjectId, SourcePackageId, PackageOffset, PackageSize, SourceOffset, Compression) "
			"VALUES (?, ?, ?, ?, ?, ?)");

		auto package = kyla::CreateFile (packagePath / "data.kypkg");

		// The file starts with a header followed by all content objects.
		// The database is stored separately
		struct PackageHeader
		{
			char id [8];
			std::uint64_t version;
			char reserved [48];

			static void Initialize (PackageHeader& header)
			{
				memset (&header, 0, sizeof (header));

				memcpy (header.id, "KYLAPKG", 8);
				header.version = 0x0001000000000000ULL;
			}
		};

		PackageHeader packageHeader;
		PackageHeader::Initialize (packageHeader);

		package->Write (kyla::ArrayRef<PackageHeader> (packageHeader));

		packageInsertQuery.BindArguments ("package", "data.kypkg",
			kyla::Uuid::CreateRandom ());
		packageInsertQuery.Step ();
		packageInsertQuery.Reset ();

		const auto packageId = db.GetLastRowId ();

		// For now we support only a single package

		// We can insert content objects directly - every unique file is one
		for (const auto& kv : uniqueFiles) {
			contentObjectInsertQuery.BindArguments (
				kv.hash,
				kv.size);
			contentObjectInsertQuery.Step ();
			contentObjectInsertQuery.Reset ();

			const auto contentObjectId = db.GetLastRowId ();

			for (const auto& reference : kv.duplicates) {
				const auto fileSetId = fileToFileSetId.find (reference)->second;

				filesInsertQuery.BindArguments (
					reference.string ().c_str (),
					contentObjectId,
					fileSetId);
				filesInsertQuery.Step ();
				filesInsertQuery.Reset ();
			}

			///@TODO(minor) Chunk the input file here based on uncompressed size

			auto startOffset = package->Tell ();
			
			auto inputFile = kyla::OpenFile (kv.sourceFile, kyla::FileOpenMode::Read);
			BlockCopy (*inputFile, *package);
			auto endOffset = package->Tell ();

			storageMappingInsertQuery.BindArguments (contentObjectId, packageId,
				startOffset, endOffset - startOffset, 0 /* offset inside the content object */,
				kyla::Sql::Null () /* no compression for now */);
			storageMappingInsertQuery.Step ();
			storageMappingInsertQuery.Reset ();
		}

		contentObjectInsert.Commit ();
	}
};
}

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
void BuildRepository (const char* descriptorFile,
	const char* sourceDirectory, const char* targetDirectory)
{
	const auto inputFile = descriptorFile;

	BuildContext ctx;
	ctx.sourceDirectory = sourceDirectory;
	ctx.targetDirectory = targetDirectory;

	boost::filesystem::create_directories (ctx.targetDirectory);

	pugi::xml_document doc;
	if (!doc.load_file (inputFile)) {
		throw RuntimeException ("Could not parse input file.",
			KYLA_FILE_LINE);
	}

	auto fileSets = GetFileSets (doc, ctx);

	HashFiles (fileSets, ctx);

	auto uniqueFiles = FindUniqueFileContents (fileSets, ctx);

	const auto packageTypeNode = doc.select_node ("//Package/Type");

	std::unique_ptr<IRepositoryBuilder> builder;

	if (packageTypeNode) {
		if (strcmp (packageTypeNode.node ().text ().as_string (), "Loose") == 0) {
			builder.reset (new LooseRepositoryBuilder);
		} else if (strcmp (packageTypeNode.node ().text ().as_string (), "Packed") == 0) {
			builder.reset (new PackedRepositoryBuilder);
		}
	}

	if (builder) {
		builder->Build (ctx, fileSets, uniqueFiles);
	} else {
		throw RuntimeException ("Package type not specified.",
			KYLA_FILE_LINE);
	}
}
}
