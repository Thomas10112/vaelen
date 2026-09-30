// VAELEN - Tools/EngineShim. ADR-0134. See CoreMinimal.h for what this proves.
//
// 16.14: the engine's file manager, as much of it as the checkpoint store
// calls and no more. Declarations only, like FFileHelper: the parse proves the
// calls have the engine's shape (a Move with Replace, a FindFiles over a
// directory), and the engine machine proves they run. The defaults are the
// engine's, so a call that leans on one parses the way it will compile.
#pragma once

#include "CoreMinimal.h"

class IFileManager
{
public:
	static IFileManager& Get();
	virtual ~IFileManager() = default;

	virtual bool FileExists(const TCHAR* Filename) = 0;
	virtual int64 FileSize(const TCHAR* Filename) = 0;
	virtual bool Delete(const TCHAR* Filename, bool RequireExists = false, bool EvenReadOnly = false,
						bool Quiet = false) = 0;
	virtual bool Move(const TCHAR* Dest, const TCHAR* Src, bool Replace = true, bool EvenIfReadOnly = false,
					  bool Attributes = false, bool bDoNotRetryOrError = false) = 0;
	virtual bool MakeDirectory(const TCHAR* Path, bool Tree = false) = 0;
	virtual void FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension) = 0;
	/// 22.02 BELIEF: an archive over a file, to seek in and read a part of
	/// (the listing reads a save's head and trailer, not the file). Null when
	/// the file cannot be opened; the caller deletes it.
	virtual class FArchive* CreateFileReader(const TCHAR* Filename, uint32 ReadFlags = 0) = 0;
};

/// 22.02 BELIEF: the engine's FArchive (Serialization/Archive.h, reached
/// through CoreMinimal in the engine) - the four verbs a partial read needs.
class FArchive
{
public:
	virtual ~FArchive() = default;
	virtual void Seek(int64 InPos);
	virtual int64 TotalSize();
	virtual void Serialize(void* V, int64 Count);
	bool IsError() const;
};
