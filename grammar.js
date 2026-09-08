/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

/*
 * Dream Embedded ML (eml).
 *
 * This is a host grammar in the style of `tree-sitter-embedded-template`: it
 * recognises the template skeleton and leaves the OCaml and HTML content
 * opaque, so that editors inject `ocaml` and `html` into it.  The directive
 * rules below (`code`, and the `seq(open, code, close)` shape) are adapted
 * from `tree-sitter/tree-sitter-embedded-template`, MIT licensed.  See README.
 *
 * The reference implementation is `src/eml/eml.ml` in `camlworks/dream` --
 * `Tokenizer.scan` and its helpers -- not the prose in the Dream docs.  Where
 * this grammar deliberately differs, README's "Divergences" section says so.
 */

export default grammar({
  name: 'eml',

  // Whitespace is significant everywhere: the indent that opens and closes a
  // template, the single space that terminates a directive's format, and the
  // column-0 `%` that starts a code line.
  extras: () => [],

  externals: $ => [
    // A run of top-level code lines, up to a template-start line.
    $.ocaml_block,
    // Zero-width.  Records the template's indent in the scanner state.
    $._template_start,
    // `[ ]*%%[options]\n` on a template's first line.
    $.template_options,
    // `[ ]*%%...\n` inside a template.  Ends it.
    $.template_end,
    // Zero-width.  Implicit end: a less-indented non-blank line, or EOF.
    $._template_dedent,
    // Template text, up to `<%` or through the newline.
    $.text,
    // `%` at column 0 inside a template.
    $._code_marker,
    // The rest of a `%` line, newline included (eml's `scan_embedded_line`).
    $._code_line_body,
    // Never produced.  Valid only during error recovery, which is how the
    // scanner knows to stand down and let the internal lexer cope.
    $._error_sentinel,
  ],

  rules: {
    source_file: $ => seq(
      optional($.ocaml_block),
      repeat(seq($.template, optional($.ocaml_block))),
    ),

    template: $ => seq(
      $._template_start,
      optional($.template_options),
      repeat(choice(
        $.text,
        $.code_line,
        $.directive,
        $.output_directive,
      )),
      choice($.template_end, $._template_dedent),
    ),

    // `% CODE` at column 0.  The body runs to the end of the line and the
    // newline belongs to it, exactly as in eml's `scan_embedded_line`; it is
    // absent only when the file ends immediately after the `%`.
    code_line: $ => seq(
      alias($._code_marker, '%'),
      optional(alias($._code_line_body, $.code)),
    ),

    // `<% CODE %>` -- emitted as a statement, prints nothing.
    directive: $ => seq('<%', ' ', optional($.code), '%>'),

    // `<%FMT CODE %>` and `<%FMT! CODE %>` -- printed via `Printf.bprintf`.
    // `!` is split out so queries can tell raw output from escaped output
    // without re-implementing eml's "last char of the format" rule.
    output_directive: $ => seq(
      '<%',
      field('format', $.format),
      optional(field('raw', $.raw)),
      ' ',
      optional($.code),
      '%>',
    ),

    // eml's `scan_options` runs to the first space, anywhere in the file.  We
    // stop at the characters that can only mean the directive is malformed, so
    // that `<%s%>` is a local ERROR rather than silently swallowing the rest
    // of the document.
    format: _ => /[^ \t\r\n!%<>]+/,

    raw: _ => '!',

    // From tree-sitter-embedded-template: `%>` is two characters and beats the
    // lone `%` alternative by longest match, so `%%>` closes the directive
    // after one `%` -- the same rule as eml's `scan_code`.
    code: _ => repeat1(choice(/[^%]+/, '%')),
  },
});
