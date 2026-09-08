# Vendored corpus

Every `.eml.ml`, `.eml.html` and `.eml.re` file under this directory is copied
verbatim from [`camlworks/dream`](https://github.com/camlworks/dream) at commit
`ebb6d57` (2023-05-16), under Dream's MIT licence — see `LICENSE` at the root
of this repository for the notice.

They are the acceptance corpus: `test/files.txt` lists them and CI parses every
one, failing on any `ERROR` or `MISSING` node. Do not edit them. To refresh,
re-copy from a Dream checkout and update the commit above.
