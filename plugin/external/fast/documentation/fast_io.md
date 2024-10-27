# Fast IO

## 1 Introduction

The `fast` namespace contains functions and classes for working with handles of various kinds for low-level IO that is capable of bypassing operating-system provided caching mechanisms and other subsystems for maximum performance. These functions and classes may have identical names or interfaces to those in the global namespace, but the rules, requirements, and specifications of their functionality is considerably different from that of those functions and interfaces within the global namespace.

## 2 Terms and definitions

Within this document, unless the context specifies otherwise:

*       The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED",  "MAY", and "OPTIONAL" in this document are to be interpreted as described in RFC 2119, regardless of their case.
* The terms "Undefined Behavior", "Implementation-Defined Behavior", "Unspecified Behavior", and "Implementation-Specific Behavior" have those definitions as proscribed by the C and C++ standards.
* The term "handle" shall mean an abstract reference or identifier used by a program to access and manage system resources or objects.

## 3 Flags for modes, caching behavior, creation behavior, and general IO behavior

Within the API exposed by this plugin, all interfaces, including top-level functions, shall accept, as arguments, an open mode, creation flags, caching flags, and general IO flags. These shall default to `mode_read`, `creation_open_existing`, `caching_all`, and `flag_multiplexable`, unless the target platform prohibits any of these defaults, whereupon the defaults to these arguments is unspecified.

When mode is `mode_unchanged`, the IO mode is kept from the last opening of the handle. The behavior is undefined if this mode is used on a top-level function which is designed to perform a single task, such as reading or writing an entire file.

When mode is `mode_none`, no reading or writing access is permitted. The implementation may only synchronize data to disk.

When mode is either `mode_attr_read` or `mode_attr_write`, the implementation may read or write, respectively, the attributes of the handle, but not it's contents or actual data. The behavior is undefined if this mode is used when performing a top-level operation.

When mode is `mode_read` or `mode_write`, the implementation may read or write, respectively, the attributes and contents or data of the handle.

When mode is `mode_append`, writes to handles may only append to the end of the handle, if the handle supports such an operation. It is unspecified if reads are allowed.

When creation is `creation_open_existing`, the on-disk resource to which a handle shall reference must exist when the handle is opened. An error shall be raised if the on-disk resource does not exist, is unavailable, or another error occurs.

When creation is `creation_only_if_not_exist`, the implementation shall atomically create the on-disk resource to which the handle shall reference if and only if it does not already exist. If the on-disk resource exists, an error shall be raised.

When creation is `creation_if_needed`, the on-disk resource to which the handle shall reference shall be atomically created if it does not exist. If the on-disk resource exists when the handle is opened, the open request shall be equivalent to `creation_open_existing`.

When creation is `creation_truncate_existing`, the on-disk resource must exist when the open request is made and it shall be truncated to 0 bytes, but the implementation shall leave the creation date and unique identifier, if supported by the underlying file system, unchanged.

When creation is `creation_always_new`, existing on-disk resources that exist when the handle is opened shall be replaced with new inodes. If an on-disk resource does not exist, it shall be created.

When caching is `caching_unchanged`, the last caching mode used when the handle was first opened is retained. The behavior is undefined if no prior caching mode existed or is known by the implementation.

When caching is `caching_none`, all caches are bypassed and reads and writes shall not complete until the underlying storage system confirms to the operating system that those operations have completed and are on the physical media. All reads or writes must be aligned to a 4K boundary. The behavior is undefined if this alignment requirement is violated.

When caching is `caching_only_metadata`, reads and writes to metadata may be cached by the implementation but reads and writes to data shall not complete until the underlying storage system indicates to the operating system that the request has completed servicing. Other handles that are opened which refer to the on-disk resource and which have cached data shall not be affected by IO on this handle. All reads or writes must be aligned to a 4K boundary. The behavior is undefined if this alignment requirement is violated.

When caching is `caching_reads`, read requests may be  cached by the implementation or the operating system. Writes shall not complete until they have been completed and stored on physical media.

When caching is `caching_reads_and_metadata`, read requests to data and reads and writes to metadata may be cached by the implementation or operating system, but writes shall not complete until they have been stored on physical media.

When caching is `caching_all`, reads and writes of both metadata and data shall complete immediately and the implementation and operating system may cache them in internal buffers. It is unspecified when those reads and writes are transmitted to the storage system.

When caching is `caching_safety_barriers`, reads and writes of both metadata and data shall be cached, and safety barriers may be inserted at undefined points during the execution of IO requests. All reads and writes shall complete immediately, regardless of whether they have been transmitted to the underlying storage system.

When caching is ` caching_temporary`, all reads and writes shall complete immediately, but updates may not be transmitted until the handle is closed or if memory becomes constrained. The implementation and operating system may treat all handles opened with this caching mode as a temporary file and it's permenance is not guaranteed by either. An error shall be raised if the operating system does not support this caching mode.

When flags is `flag_none`, no special flags are applied.

When flags is `flag_unlink_on_first_close`, the on-disk resource is unlinked upon handle close. On POSIX systems, this shall unlink the file path if and only if the inode matches. On Windows 10 1709 or later, the behavior shall be identical. On older versions of Windows, the file entry shall not disappear but shall become unavailable for others to open, and shall return an `resource_unavailable_try_again` error. If the `win_disable_unlink_emulation` flag is not specified, the implementation shall emulate POSIX behavior by renaming the file to a random name on close.

When flags is `flag_disable_safety_barriers`, additional fsync operations added by the implementation to mitigate inconsistent caching behaviors shall be disabled. When this flag is not specified, the implementation adds fsyncs during file truncation, handle closure, and on Linux, to the parent directory when files are created or closed. This flag shall affect caching modes `caching_none`, `caching_reads`, `caching_reads_and_metadata`, and `caching_safety_barriers`.

When flags is `flag_disable_safety_unlinks`, the implementation shall compare the inode of the path to be unlinked with that of the open handle before unlinking to prevent accidental deletion if the file has been renamed. This shall not prevent races where changes occur between the inode check and the unlink operation.

When flags is `flag_disable_prefetching`, the operating system is requested to disable prefetching of data, which can improve random IO performance. The operating system is not required to honor this request.

When flags is `flag_maximum_prefetching`, the operating system is requested to maximize data prefetching, potentially prefetching the entire file into kernel cache, which can improve sequential IO performance. The operating system is not required to honor this request.

When flags is `flag_win_disable_unlink_emulation`, the emulation of POSIX unlink behavior on older versions of Windows shall be disabled, as detailed under `flag_unlink_on_first_close`.

When flags is `flag_win_disable_sparse_file_creation`, the implementation must not opt into extents-based storage for newly created files on NTFS, which is typically disabled by default but nearly cost-free.

When flags is `flag_disable_parallelism`, the implementation must not use OpenMP or the Parallelism or Concurrency standard library extensions for IO operations involving multiple inodes, and shall default to linear time completion.

When flags is `flag_win_create_case_sensitive_directory`, NTFS directories shall be created with case-sensitive leaf-name lookup, providing exact POSIX semantics without requiring system registry modifications.

When flags is `flag_multiplexable`, the handle shall be created to allow multiplexed IO operations. On Windows, this shall require `OVERLAPPED` semantics. On POSIX systems, it shall require nonblocking semantics for non-file handles like pipes and sockets, but not for file, directory, and symlink handles.

When flags is `flag_byte_lock_insanity`, POSIX byte range locks shall be used, allowing multiple overlapping locks on the same file.

When flags is `flag_anonymous_inode`, an inode is created without a representation in the file system, making it effectively invisible and temporary.

The aforementioned permission flags, caching flags, creation flags, and IO flags may be ORed together. The implementation is not required to validate that the correct flags have been specified immediately upon opening of the handle, and an error may be delayed until the first actual IO operation. If bit 0 on the mode flags is set, the handle shall be writable. If bit 0 is set on the caching flags, safety barriers shall be enabled.

## 4 Top-level interfaces

### 4.1 read_file

#### Synopsis

```angelscript
#pragma plugin fast
string read_file(const string &filename, mode mode = mode_read, creation creation = creation_open_existing, caching caching = caching_all, flag flags = flag_multiplexable);
```

#### Description

The `read_file` function takes as input a file name to read and an associated open mode, caching flags, creation flags, and IO behavior modifiers. Upon success, the function returns the read data. This function may block for up to 1 minute to await the completion of the IO request.

An error shall be raised if:

* The file name is invalid on the operating system, is unable to be converted to a valid form, or cannot be converted into an absolute path;
* Any of the flags are invalid or unsupported;
* The requested IO operation is not supported on the on-disk resource; or
* Any other internal error occurs.

It is unspecified when this error is raised; the implementation may delay the error until the actual IO operation is attempted.

#### Returns

If the file was read, it's contents is returned. If no bytes were read, or the deadline expires, an empty string is returned and the function silently fails. If an error is raised, execution is aborted.

### 4.2 write_file

#### Synopsis

```angelscript
#pragma plugin fast
void write_file(const string &filename, const string &contents, mode mode = mode_write, creation creation = creation_if_needed, caching caching = caching_all, flag flags = flag_multiplexable);
```

#### Description

The `write_file` function takes as input a file name and the contents to write, and writes the associated contents to the file specified, using the associated caching mode, creation mode, open mode, and IO behavior modifiers. The file, if it already exists, is truncated by the implementation and all existing contents are lost. This function may block for up to 1 minute to await the completion of the IO request.

An error shall be raised if:

* The file name is invalid on the operating system, is unable to be converted to a valid form, or cannot be converted into an absolute path;
* Any of the flags are invalid or unsupported;
* The requested IO operation is not supported on the on-disk resource; or
* Any other internal error occurs.

It is unspecified when this error is raised; the implementation may delay the error until the actual IO operation is attempted.

#### Returns

This function returns nothing. If no bytes were written, or the deadline expires, the function silently fails. If an error is raised, execution is aborted.

