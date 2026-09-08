#include "tree_sitter/parser.h"

#include <stdint.h>
#include <stdlib.h>

// Must match the order of the `externals` array in grammar.js.
enum TokenType {
  OCAML_BLOCK,
  TEMPLATE_START,
  TEMPLATE_OPTIONS,
  TEMPLATE_END,
  TEMPLATE_DEDENT,
  TEXT,
  CODE_MARKER,
  CODE_LINE_BODY,
  ERROR_SENTINEL,
};

// The only thing worth remembering across tokens is the indent of the template
// currently open: the column its first line started at, which a later line has
// to fall short of to close it implicitly.  That is eml.ml's
// `String.length all_whitespace >= indent` test in `at_text_line`.
//
// Whether we are inside a template is deliberately *not* stored.
// `valid_symbols` already says so -- the code-state tokens (`ocaml_block`,
// `_template_start`) and the template-state tokens never appear in the same
// parse state outside error recovery, which is bailed out of below -- and a
// second copy of that fact is a second thing to get out of sync on an
// incremental reparse.
typedef struct {
  uint16_t indent;
} Scanner;

static void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static void mark_end(TSLexer *lexer) { lexer->mark_end(lexer); }

static bool eof(TSLexer *lexer) { return lexer->eof(lexer); }

static int32_t peek(TSLexer *lexer) { return lexer->lookahead; }

static bool finish(TSLexer *lexer, enum TokenType symbol) {
  lexer->result_symbol = symbol;
  return true;
}

// Consume the rest of the line, the newline included.  Every line-oriented
// scanner in eml.ml takes the newline with it.
static void consume_line(TSLexer *lexer) {
  while (!eof(lexer) && peek(lexer) != '\n') {
    advance(lexer);
  }
  if (!eof(lexer)) {
    advance(lexer);
  }
}

// A run of top-level OCaml (or Reason) lines.  Stops before the line that
// opens a template -- `%` in column 0, `[ ]*<`, or `[ ]*%%`, which is
// `is_template_line` in eml.ml.  Note that any number of leading spaces will
// do, zero included; the comment above that function claims two or more, but
// the code does not, and the code is what dream_eml runs.
//
// The opening line's leading whitespace is left for the template to claim, so
// `mark_end` is only ever called on a line boundary.
static bool scan_ocaml_block(TSLexer *lexer) {
  bool consumed = false;
  mark_end(lexer);

  while (!eof(lexer)) {
    // Always at the start of a line here.
    if (peek(lexer) == '%') {
      break;
    }

    while (peek(lexer) == ' ') {
      advance(lexer);
    }

    if (peek(lexer) == '<') {
      break;
    }
    if (peek(lexer) == '%') {
      advance(lexer);
      if (peek(lexer) == '%') {
        break;
      }
      // A lone `%` behind whitespace is ordinary code; keep reading the line.
    }

    consume_line(lexer);
    consumed = true;
    mark_end(lexer);
  }

  return consumed ? finish(lexer, OCAML_BLOCK) : false;
}

// Zero-width.  Records the indent so that the dedent test below has something
// to compare against.  A template opened by a column-0 `%` has indent 0
// (eml.ml returns `true, ""` for that case).
static bool scan_template_start(Scanner *scanner, TSLexer *lexer) {
  mark_end(lexer);

  uint16_t indent = 0;
  if (peek(lexer) != '%') {
    while (peek(lexer) == ' ' && indent < UINT16_MAX) {
      advance(lexer);
      indent++;
    }
  }

  scanner->indent = indent;
  return finish(lexer, TEMPLATE_START);
}

// Template text: through the newline, or up to the `<` of a `<%`, whichever
// comes first.  A `<` followed by anything else is ordinary text.
static bool scan_text(TSLexer *lexer, bool consumed) {
  while (!eof(lexer)) {
    if (peek(lexer) == '\n') {
      advance(lexer);
      consumed = true;
      break;
    }

    if (peek(lexer) == '<') {
      mark_end(lexer);
      advance(lexer);
      if (peek(lexer) == '%') {
        // Stop before the directive.  If that leaves the token empty, the
        // internal lexer takes over and matches `<%` itself.
        return consumed ? finish(lexer, TEXT) : false;
      }
      consumed = true;
      continue;
    }

    advance(lexer);
    consumed = true;
  }

  if (!consumed) {
    return false;
  }
  mark_end(lexer);
  return finish(lexer, TEXT);
}

void *tree_sitter_eml_external_scanner_create(void) {
  return calloc(1, sizeof(Scanner));
}

void tree_sitter_eml_external_scanner_destroy(void *payload) { free(payload); }

unsigned tree_sitter_eml_external_scanner_serialize(void *payload,
                                                    char *buffer) {
  Scanner *scanner = (Scanner *)payload;
  buffer[0] = (char)(scanner->indent & 0xFF);
  buffer[1] = (char)((scanner->indent >> 8) & 0xFF);
  return 2;
}

void tree_sitter_eml_external_scanner_deserialize(void *payload,
                                                   const char *buffer,
                                                   unsigned length) {
  Scanner *scanner = (Scanner *)payload;
  scanner->indent = 0;
  if (length >= 2) {
    scanner->indent = (uint16_t)((uint8_t)buffer[0] |
                                 ((uint16_t)(uint8_t)buffer[1] << 8));
  }
}

bool tree_sitter_eml_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid_symbols) {
  Scanner *scanner = (Scanner *)payload;

  // Error recovery marks every external symbol valid at once.  Stand down: a
  // greedy `ocaml_block` or `text` here would swallow the rest of the file.
  if (valid_symbols[ERROR_SENTINEL]) {
    return false;
  }

  // Only ever valid immediately after a `%` marker, and it owns the rest of
  // the line, so it is tested before anything else that could match here.
  // At end of input there is no body -- a file ending in a bare `%` is a code
  // line with empty code -- so fall through to close the template instead.
  if (valid_symbols[CODE_LINE_BODY] && !eof(lexer)) {
    consume_line(lexer);
    mark_end(lexer);
    return finish(lexer, CODE_LINE_BODY);
  }

  // A template left open at end of input closes there.  Tested before the
  // column check, because a directive can leave us mid-line at EOF.
  if (eof(lexer)) {
    if (valid_symbols[TEMPLATE_DEDENT]) {
      mark_end(lexer);
      return finish(lexer, TEMPLATE_DEDENT);
    }
    return false;
  }

  // Read once, before anything advances.
  bool line_start = lexer->get_column(lexer) == 0;

  if (line_start && valid_symbols[OCAML_BLOCK] && scan_ocaml_block(lexer)) {
    return true;
  }

  // Zero-width, and never emitted at end of input (handled above), so every
  // template contains at least one token and `source_file`'s repeat cannot
  // spin on empty matches.
  if (line_start && valid_symbols[TEMPLATE_START]) {
    return scan_template_start(scanner, lexer);
  }

  if (!line_start) {
    return valid_symbols[TEXT] ? scan_text(lexer, false) : false;
  }

  // Provisional zero-width token at column 0, so that the dedent below can
  // still be emitted here after looking past the line's leading whitespace.
  mark_end(lexer);

  // `%` in column 0 is a code line.  This is tested before `%%`, which is why
  // a `%%` in column 0 is a code line too rather than an options or
  // terminator line (Q2) -- eml.ml's `at_text_line` orders it the same way.
  if (peek(lexer) == '%' && valid_symbols[CODE_MARKER]) {
    advance(lexer);
    mark_end(lexer);
    return finish(lexer, CODE_MARKER);
  }

  uint16_t spaces = 0;
  while (peek(lexer) == ' ') {
    advance(lexer);
    spaces++;
  }
  bool consumed = spaces > 0;

  // eml decides "blank" on the character following the leading whitespace, so
  // this has to be read before the `%%` test consumes anything.
  bool blank = peek(lexer) == '\n';

  if (peek(lexer) == '%') {
    advance(lexer);
    consumed = true;
    if (peek(lexer) == '%') {
      advance(lexer);
      // On a template's first line this is the options line; anywhere else it
      // terminates the template.  `template_options` is valid only in that
      // first position, which is how the two are told apart -- the same job
      // eml.ml's `first` flag does.
      if (valid_symbols[TEMPLATE_OPTIONS] || valid_symbols[TEMPLATE_END]) {
        consume_line(lexer);
        mark_end(lexer);
        return finish(lexer, valid_symbols[TEMPLATE_OPTIONS] ? TEMPLATE_OPTIONS
                                                             : TEMPLATE_END);
      }
      return false;
    }
    // A lone `%` behind whitespace is ordinary text; it has been consumed and
    // the text scan picks up from there.
  }

  // Blank lines never close a template, however far they are outdented (Q6).
  if (spaces >= scanner->indent || blank) {
    return valid_symbols[TEXT] ? scan_text(lexer, consumed) : false;
  }

  // Less indented than the opening line and not blank: the template ends
  // here, and this line belongs to the next code block with its leading
  // whitespace intact.  `mark_end` is still the one set at column 0.
  if (valid_symbols[TEMPLATE_DEDENT]) {
    return finish(lexer, TEMPLATE_DEDENT);
  }
  return false;
}
