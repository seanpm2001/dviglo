// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include "../core/object.h"

namespace dviglo
{

/// %File entry within the package file.
struct PackageEntry
{
    /// Offset from the beginning.
    unsigned offset_;

    /// File size.
    unsigned size_;

    /// File checksum.
    hash32 checksum_;
};

/// Stores files of a directory tree sequentially for convenient access.
class DV_API PackageFile : public Object
{
    DV_OBJECT(PackageFile);

public:
    /// Construct.
    explicit PackageFile();
    /// Construct and open.
    PackageFile(const String& fileName, unsigned startOffset = 0);
    /// Destruct.
    ~PackageFile() override;

    /// Open the package file. Return true if successful.
    bool Open(const String& fileName, unsigned startOffset = 0);
    /// Check if a file exists within the package file. This will be case-insensitive on Windows and case-sensitive on other platforms.
    bool Exists(const String& fileName) const;
    /// Return the file entry corresponding to the name, or null if not found. This will be case-insensitive on Windows and case-sensitive on other platforms.
    const PackageEntry* GetEntry(const String& fileName) const;

    /// Return all file entries.
    const HashMap<String, PackageEntry>& GetEntries() const { return entries_; }

    /// Return the package file name.
    const String& GetName() const { return fileName_; }

    /// Return hash of the package file name.
    StringHash GetNameHash() const { return nameHash_; }

    /// Return number of files.
    unsigned GetNumFiles() const { return entries_.Size(); }

    /// Return total size of the package file.
    unsigned GetTotalSize() const { return totalSize_; }

    /// Return total data size from all the file entries in the package file.
    unsigned GetTotalDataSize() const { return totalDataSize_; }

    /// Return checksum of the package file contents.
    hash32 GetChecksum() const { return checksum_; }

    /// Return whether the files are compressed.
    bool IsCompressed() const { return compressed_; }

    /// Return list of file names in the package.
    const Vector<String> GetEntryNames() const { return entries_.Keys(); }

private:
    /// File entries.
    HashMap<String, PackageEntry> entries_;
    /// File name.
    String fileName_;
    /// Package file name hash.
    StringHash nameHash_;
    /// Package file total size.
    unsigned totalSize_;
    /// Total data size in the package using each entry's actual size if it is a compressed package file.
    unsigned totalDataSize_;
    /// Package file checksum.
    hash32 checksum_;
    /// Compressed flag.
    bool compressed_;
};

}
