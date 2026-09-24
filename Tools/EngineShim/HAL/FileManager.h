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
};
