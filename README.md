# tree-sitter-eml

Tree-sitter grammar for [Dream](https://github.com/camlworks/dream)'s Embedded
ML (eml), the template language behind `.eml.ml`, `.eml.html` and `.eml.re`.

## Overview

This is a *host* grammar in the style of
[`tree-sitter-embedded-template`](https://github.com/tree-sitter/tree-sitter-embedded-template):
it recognises the template skeleton — where a template opens and closes, where
the `%` code lines and the `<% … %>` directives are — and leaves the OCaml and
the HTML opaque, so that editors inject `ocaml` and `html` into it.

The specification is the tokenizer, not the prose. Everything here follows
`src/eml/eml.ml` in `camlworks/dream` — `Tokenizer.scan`, `is_template_line`,
`at_text_line`, `scan_embedded`, `scan_embedded_line` — and the 34 expect tests
in `test/expect/eml/tokens.ml`, which are ported into `test/corpus/boundaries.txt`.
Where the Dream docs and `eml.ml` disagree, `eml.ml` wins: it is what
`dream_eml` actually runs.

## Features

- **Two-state, line-oriented parsing**: top-level code and template text, with
  the boundary decided by indentation exactly as `eml.ml` decides it
- **External scanner**: column- and indent-sensitive lexing for implicit
  template starts, implicit ends, `%` code lines and `%%` lines
- **Language injection**: OCaml (or Reason) into the code regions, HTML into
  the template text
- **Corpus-backed**: 41 corpus tests plus every `.eml.*` file shipped by Dream,
  parsed in CI with zero `ERROR` or `MISSING` nodes

## Supported file types

| Extension | Embedded language |
|---|---|
| `.eml.ml` | OCaml |
| `.eml.re` | Reason |
| `.eml.html` | OCaml **or** Reason — see below |

`dream_eml` picks the syntax from `Filename.extension`, which for
`foo.eml.html` is `.html` and so falls through to OCaml — *unless* the dune
rule passes `--emit-reason`. Dream ships one of each: `example/w-template-files`
and `example/r-template-files` have the same `template.eml.html` filename and
different languages inside. The extension therefore cannot decide it, and
editors should default `.eml.html` to OCaml and let the user override.

Do not register this grammar for a bare `.eml` extension: that is RFC 5322
email, already served by `tree-sitter-mail`.

Note that `file-types` in `tree-sitter.json` matches on the final extension, so
`eml.ml` there degenerates to `ml` and would collide with `tree-sitter-ocaml`.
Editors should match on a glob or pattern instead — Helix
`file-types = [{ glob = "*.eml.ml" }]`, Neovim `vim.filetype.add({ pattern = … })`.

## Supported constructs

| Construct | Syntax | Node |
|---|---|---|
| Code block | top-level lines, up to a template | `ocaml_block` |
| Template options | `  %%`, `  %% response` | `template_options` |
| Template terminator | `  %%` | `template_end` |
| Template text | anything at or beyond the opening indent | `text` |
| Code line | `% CODE` at column 0 | `code_line` |
| Statement directive | `<% CODE %>` | `directive` |
| Output directive | `<%s x %>`, `<%02X n %>` | `output_directive` |
| Unescaped output | `<%s! x %>` | `output_directive` with `raw` |

A template opens on the first line that is `%` in column 0, `[ ]*<`, or
`[ ]*%%`, and closes on a `[ ]*%%` line, on a non-blank line indented less than
the opening line, or at end of input. Blank lines never close a template.

## Divergences from `eml.ml`

Deliberate, and all in the same direction: eml's scanners run to the next
delimiter *anywhere in the file*, which turns a typo into a silently swallowed
document. This grammar stops at the line instead and reports a local `ERROR`,
which is what an editor wants. `dream_eml` remains the authority on whether a
file builds.

| Input | `eml.ml` | Here |
|---|---|---|
| `<%s%>` | options become `s%>`, code runs to the next `%>` in the file | `ERROR` at the directive |
| `<%s!x %>` | options `s!x`, a broken `Printf` conversion | `ERROR` at the directive |
| `<%` then a newline | the newline goes into the options | `ERROR` at the directive |
| `<%` at end of input | an empty embedded token | `ERROR` at the directive |
| `  %% trailing text` | hard error, "text following closing '%%'" | absorbed into `template_end` |

The last one is the one case where we are *more* permissive. A `template_end`
covering only `[ ]*%%` was considered, so the trailing text would stand out,
but it does not produce an `ERROR` — the parser is back in code state by then
and the text simply becomes an `ocaml_block`, which is more misleading than
one over-long terminator node.

Quirks that are *not* divergences, and are pinned in `test/corpus/quirks.txt`:

- Only spaces are indentation; a tab-indented `<html>` has indent 0.
- `%%` in column 0 is a code line, not an options or terminator line — `eml.ml`
  tests for a leading `%` before it tests for `%%`.
- Any line whose first non-space character is `<` opens a template, `<- y`
  included. The comment above `scan_code_block` claims two or more leading
  spaces are needed; the code accepts zero.
- A `<` in column 0 inside an indented template closes it and immediately opens
  another, with an empty code block between.
- Carriage returns are ordinary text, so a CRLF file's blank lines are not
  blank as far as the indent rules are concerned.

## Installation

Nothing published to package repositories just yet.

### From source

```bash
git clone https://github.com/tmcgilchrist/tree-sitter-eml
cd tree-sitter-eml
npm install
npm run generate      # tree-sitter generate --abi 14
tree-sitter test
```

The committed `src/parser.c` is ABI 14 on purpose: most Emacs builds cannot
load ABI 15 yet. Regenerate with `npm run generate`, not a bare
`tree-sitter generate`.

## Usage

### With the tree-sitter CLI

```bash
tree-sitter parse path/to/template.eml.ml
tree-sitter highlight path/to/template.eml.ml
```

### Injection

`queries/injections.scm` puts the top-level code and the bodies of the `%`
code lines into **one combined** OCaml document, because they are one statement
stream: `let render tasks =` opens a binding that `% tasks |> List.iter begin`
continues and `% end;` closes. They are captured by a single query pattern for
that reason — two patterns would be combined into two separate documents and
every `begin` would look unbalanced.

Directive bodies are injected individually, *not* combined, because `<%s name %>`
is an expression; splicing it into the statement stream would land it inside a
`begin … end` and derail the OCaml parse from there to the end of the block.

`queries/injections-reason.scm` is the same file with `reason` in place of
`ocaml`, for `.eml.re`.

## Development

```bash
npm run generate                     # regenerate src/parser.c at ABI 14
tree-sitter test                     # corpus tests
tree-sitter parse -q $(cat test/files.txt)   # the vendored Dream corpus
npm run lint                         # eslint on grammar.js
```

`test/files/dream/` is Dream's own `.eml.*` corpus, vendored; see the README
there. CI parses all of it and fails on any `ERROR` or `MISSING` node.

The two fixtures beside it, `eof-bare-code-line.eml.ml` and
`eof-mid-template.eml.ml`, deliberately have no final newline: `tree-sitter
test` terminates every corpus input with one, so a file that ends mid-token is
not expressible there. Both paths were real bugs. Keep them byte-exact — the
`-text` entries in `.gitattributes` are what stop a Windows checkout from
adding the newline back.

### Oracle check

For anything subtle, compare against the tokenizer itself rather than against
intuition:

```bash
cd /path/to/dream
dune exec dream_eml -- --stdout path/to/file.eml.ml
```

Every `Code_block` / `Text` / `Embedded` boundary it reports should line up
with a node boundary in the tree. `eml` emits `Newline` tokens and empty `Text`
tokens that have no counterpart here — a newline belongs to the `text` node
that ends with it, and an empty `Text` produces no node at all.

## Architecture

`grammar.js` holds the structure; `src/scanner.c` holds everything that depends
on column or on remembered indentation:

| External token | Job |
|---|---|
| `ocaml_block` | a run of top-level lines, stopping before the line that opens a template |
| `_template_start` | zero-width; records the template's indent |
| `template_options` | the `[ ]*%%…` line that opens a template explicitly |
| `template_end` | the `[ ]*%%…` line that closes one |
| `_template_dedent` | zero-width; implicit close, on a dedent or at end of input |
| `text` | template text, to `<%` or through the newline |
| `_code_marker` | `%` in column 0 |
| `_code_line_body` | the rest of that line, newline included |
| `_error_sentinel` | never produced; its validity means error recovery, and the scanner stands down |

The scanner's whole state is the current template's indent, two bytes. Whether
we are inside a template is deliberately not stored — `valid_symbols` already
says so, and a second copy of that fact is a second thing to desynchronise on
an incremental reparse.

`ocaml_block` is one opaque token per run of code lines, so an edit inside a
code block re-lexes that whole run. Across Dream's corpus the longest such run
is 23 lines, so this is bounded in practice; if it ever shows up on a file with
a long prelude, the fix is one `ocaml_block` per line, which the combined
injection reassembles anyway.

## Resources

- Dream's template documentation:
  <https://ocaml.org/p/dream/latest/doc/dream/Dream/index.html#templates>
- The tokenizer and generator: `src/eml/eml.ml` in `camlworks/dream`
- The oracle tests: `test/expect/eml/tokens.ml` in `camlworks/dream`

## Prior art

The directive rules — the `code` token, and the `seq(open, code, close)` shape
that lets `%>` beat a lone `%` by longest match — are adapted from
[`tree-sitter/tree-sitter-embedded-template`](https://github.com/tree-sitter/tree-sitter-embedded-template),
MIT licensed. They are vendored rather than depended on: embedded-template has
no external scanner, its dialects (erb, ejs, etlua) are one shared parser split
only by queries, and its root rule assumes a single language outside the
directives. eml needs the opposite — a two-state machine whose default language
changes at an indent boundary — which is a change to the root node every
downstream query depends on.

The zero-width `_template_start` / `_template_dedent` pair is the same device
`tree-sitter-python` uses for indent and dedent.

## License

MIT. See `LICENSE`.
