# Filesystem Diffing Benchmarks

## Efficiency file stats (stat vs statx)

statx(2):
```
	A  filesystem may also fill in fields that the caller didn't ask for if
	it has values for them available and the information is available at no
	extra  cost.   If  this  happens, the corresponding bits will be set in
	stx_mask.
```
May be worthwhile checking what is the cheapest way to get the fields that we want, intentionally/unintentionally

## Efficiency subdir traversal (readdir vs fts_open)

## Sequential IO vs IO-uring batched requests
opendir(3)
- __open_nocancel -> openat(2)
- opendir_tail ->__fstat64_time64 -> __fstatat64_time64 -> __fstatat64_time64_statx -> newfstatat(2)

readdir(3)
- __getdents -> getdents64(2)

statx(3)
- statx(2)

printf(3)
- write(2)

closedir(3)
- close(2)

# IO-uring specific

## Linked vs Independent requests

## Optimal buffer size
