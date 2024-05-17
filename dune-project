(lang dune 2.0)

(name fs-diff-bench)

(generate_opam_files true)

(source
 (github koonwen/fs-diff-bench))

(license Unlicense)

(authors "Lee Koon Wen")

(maintainers "Lee Koon Wen")

(package
 (name fs-diff-bench)
 (synopsis "Minimal OCaml project for starting a new one")
 (description
   "Hello is the classic hello world but organised as a proper OCaml project incuding command-line parsing and online help. It serves as a starting point for new projects.")
 (depends
  eio_linux))
