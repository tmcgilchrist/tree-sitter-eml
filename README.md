# tree-sitter-eml

Tree-sitter grammar for [Dream](https://github.com/camlworks/dream)'s Embedded
ML (eml).

## Overview

This is a *host* grammar in the style of
[`tree-sitter-embedded-template`](https://github.com/tree-sitter/tree-sitter-embedded-template).
It recognises the template skeleton: where a template opens and closes, where
the `%` code lines and the `<% ... %>` directives are. The OCaml and the HTML
are left opaque for editors to inject `ocaml` and `html` into.

It follows `src/eml/eml.ml` in `camlworks/dream` (`Tokenizer.scan`,
`is_template_line`, `at_text_line`, `scan_embedded`, `scan_embedded_line`) and
the 34 expect tests in `test/expect/eml/tokens.ml`, which are ported into
`test/corpus/boundaries.txt`. Where the Dream docs and `eml.ml` disagree,
follow `eml.ml`, it is what `dream_eml` runs.

Parsing is line-oriented and has two states, top-level code and template text,
with the boundary decided by indentation the way `eml.ml` decides it. That part
lives in an external scanner, since it depends on the column and on the
template's remembered indent.

Queries inject OCaml into the code regions and HTML into the template text.

## Supported file types

`.eml.ml` and `.eml.html`, with OCaml as the embedded language.

The template structure is the same whatever the code regions hold, so any eml
file parses. Only the injection query is OCaml-specific. `dream_eml` can emit
another language when the dune rule passes `--emit-reason`, and there is no
injection query for that here. If you want one, open an issue or a PR.

That flag is also why `.eml.html` cannot be decided from the extension.
`dream_eml` picks the syntax from `Filename.extension`, which for
`foo.eml.html` is `.html` and so falls through to OCaml unless the rule says
otherwise, and Dream ships one of each, `example/w-template-files` and
`example/r-template-files`, under the same `template.eml.html` name. Editors
should default `.eml.html` to OCaml and let the user override.

Do not register this grammar for a bare `.eml` extension: that is RFC 5322
email, already served by `tree-sitter-mail`.

`file-types` in `tree-sitter.json` matches on the final extension, so `eml.ml`
there degenerates to `ml` and collides with `tree-sitter-ocaml`. Editors should
match on a glob or pattern instead: Helix `file-types = [{ glob = "*.eml.ml" }]`,
Neovim `vim.filetype.add({ pattern = ... })`.

## Supported constructs

| Construct           | Syntax                                   | Node                          |
|---------------------|------------------------------------------|-------------------------------|
| Code block          | top-level lines, up to a template        | `ocaml_block`                 |
| Template options    | `  %%`, `  %% response`                  | `template_options`            |
| Template terminator | `  %%`                                   | `template_end`                |
| Template text       | anything at or beyond the opening indent | `text`                        |
| Code line           | `% CODE` at column 0                     | `code_line`                   |
| Statement directive | `<% CODE %>`                             | `directive`                   |
| Output directive    | `<%s x %>`, `<%02X n %>`                 | `output_directive`            |
| Unescaped output    | `<%s! x %>`                              | `output_directive` with `raw` |

A template opens on the first line that is `%` in column 0, `[ ]*<`, or
`[ ]*%%`, and closes on a `[ ]*%%` line, on a non-blank line indented less than
the opening line, or at end of input. Blank lines never close a template.

## Divergences from `eml.ml`

These all go the same way. eml's scanners run to the next delimiter *anywhere
in the file*, so a typo silently swallows the document. This grammar stops at
the line and reports a local `ERROR`, which is more use in an editor.
`dream_eml` remains the authority on whether a file builds.

| Input                | `eml.ml`                                                     | Here                         |
|----------------------|--------------------------------------------------------------|------------------------------|
| `<%s%>`              | options become `s%>`, code runs to the next `%>` in the file | `ERROR` at the directive     |
| `<%s!x %>`           | options `s!x`, a broken `Printf` conversion                  | `ERROR` at the directive     |
| `<%` then a newline  | the newline goes into the options                            | `ERROR` at the directive     |
| `<%` at end of input | an empty embedded token                                      | `ERROR` at the directive     |
| `  %% trailing text` | hard error, "text following closing '%%'"                    | absorbed into `template_end` |

The last row is the one case where this grammar is *more* permissive. A
`template_end` covering only `[ ]*%%` would make the trailing text stand out,
but it produces no `ERROR`: the parser is back in code state by then, so the
text becomes an `ocaml_block`, which misleads more than one over-long
terminator node does.

Quirks that are *not* divergences, and are pinned in `test/corpus/quirks.txt`:

- Only spaces are indentation. A tab-indented `<html>` has indent 0.
- `%%` in column 0 is a code line rather than an options or terminator line,
  because `eml.ml` tests for a leading `%` before it tests for `%%`.
- Any line whose first non-space character is `<` opens a template, `<- y`
  included. The comment above `scan_code_block` claims two or more leading
  spaces are needed. The code accepts zero.
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
npm run generate      # make generate, at the pinned ABI
tree-sitter test
```

The committed `src/parser.c` is ABI 14, because most Emacs builds cannot load
ABI 15 yet. Regenerate with `npm run generate` or `make generate`. The CLI and
ABI versions are set in the `Makefile` (`TREE_SITTER_VERSION` and 
`TS_ABI_VERSION`).

## Usage

### With the tree-sitter CLI

```bash
tree-sitter parse path/to/template.eml.ml
tree-sitter highlight path/to/template.eml.ml
```

### With Emacs

[neocaml](https://github.com/bbatsov/neocaml) ships `neocaml-eml-mode`, which
uses this grammar as its host and injects `ocaml` and `html` into it via
`treesit-range-rules`. It pins an ABI-14 tag of this repository, which is why
`src/parser.c` is committed at ABI 14 and not at the CLI default.

### Injection

`queries/injections.scm` puts the top-level code and the bodies of the `%`
code lines into **one combined** OCaml document, because they are one statement
stream: `let render tasks =` opens a binding that `% tasks |> List.iter begin`
continues and `% end;` closes. They are captured by a single query pattern for
that reason. Two patterns would be combined into two separate documents and
every `begin` would look unbalanced.

Directive bodies get their own patterns, so each `<% ... %>` is injected on its
own instead of joining that stream. This is a precaution rather than a measured
win. `<%s name %>` is an expression, and splicing one into the middle of a
statement stream could leave it somewhere it does not belong. On Dream's
corpus it changes nothing. Across the 25 `.eml.{ml,html}` examples, 12 files contain
an `ERROR` in the injected OCaml tree either way and 33 `ERROR` nodes either
way, with identical per-file counts. That includes
`w-template-logic/template.eml.ml`, which has a directive inside a
`begin ... end`.

Those errors come from somewhere else, and no grouping fixes them: a template
is usually the entire body of a `let`, so with the template text removed the
OCaml reads `let home =` followed by the next binding. Tree-sitter keeps the
tokens inside the `ERROR` node, so the surrounding OCaml still highlights.

`neocaml-eml-mode` takes the simpler option and puts directive bodies in the
same parser as everything else, one `treesit-range-rules` with no `:local t`.

## Development

```bash
npm run generate                     # regenerate at the pinned ABI
tree-sitter test                     # corpus tests
tree-sitter parse -q $(cat test/files.txt)   # the vendored Dream corpus
npm run lint                         # eslint on grammar.js
```

`test/files/dream/` is Dream's own `.eml.*` corpus, vendored. See the README
there for provenance. CI parses all of it and fails on any `ERROR` or `MISSING` node.

The two fixtures beside it, `eof-bare-code-line.eml.ml` and
`eof-mid-template.eml.ml`, have no final newline. `tree-sitter test` terminates
every corpus input with one, so a file ending mid-token cannot be expressed
there, and both of these paths were bugs at one point. Keep them byte-exact;
the `-text` entries in `.gitattributes` stop a Windows checkout adding the
newline back.

## Architecture

`grammar.js` holds the structure. `src/scanner.c` holds everything that depends
on the column or on the remembered indent:

| External token     | Job                                                                            |
|--------------------|--------------------------------------------------------------------------------|
| `ocaml_block`      | a run of top-level lines, stopping before the line that opens a template       |
| `_template_start`  | zero-width, records the template's indent                                      |
| `template_options` | the `[ ]*%%...` line that opens a template explicitly                          |
| `template_end`     | the `[ ]*%%...` line that closes one                                           |
| `_template_dedent` | zero-width, implicit close on a dedent or at end of input                     |
| `text`             | template text, to `<%` or through the newline                                  |
| `_code_marker`     | `%` in column 0                                                                |
| `_code_line_body`  | the rest of that line, newline included                                        |
| `_error_sentinel`  | never produced, and its validity means error recovery, so the scanner stands down |

The scanner's whole state is the current template's indent, two bytes. Whether
we are inside a template is not stored: `valid_symbols` already says so, and a
second copy of that fact is a second thing to desynchronise on an incremental
reparse.

`ocaml_block` is one opaque token per run of code lines, so an edit inside a
code block re-lexes that whole run. The longest such run in Dream's corpus is
23 lines, so the cost is bounded in practice. If it ever does show up, on a
file with a long prelude, emit one `ocaml_block` per line instead. The
combined injection reassembles them anyway.

## Resources

- Dream's template documentation:
  <https://ocaml.org/p/dream/latest/doc/dream/Dream/index.html#templates>
- The tokenizer and generator: `src/eml/eml.ml` in `camlworks/dream`
- The oracle tests: `test/expect/eml/tokens.ml` in `camlworks/dream`

## Prior art

The directive rules, the `code` token, and the `seq(open, code, close)` shape
that lets `%>` beat a lone `%` by longest match, are adapted from
[`tree-sitter/tree-sitter-embedded-template`](https://github.com/tree-sitter/tree-sitter-embedded-template),
MIT licensed. They are vendored, not depended on. embedded-template has no
external scanner, its dialects (erb, ejs, etlua) are one shared parser split
only by queries, and its root rule assumes a single language outside the
directives. eml needs a two-state machine whose default language changes at an
indent boundary, which means changing the root node that every downstream query
depends on.

The zero-width `_template_start` / `_template_dedent` pair is the same device
`tree-sitter-python` uses for indent and de-indent.
