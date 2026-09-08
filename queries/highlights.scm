; Dream Embedded ML.  Capture names are from the tree-sitter standard set so
; that Neovim and Helix themes pick them up without extra configuration.

; Directive delimiters, and the `%` that opens a code line.
[
  "<%"
  "%>"
] @punctuation.special

(code_line
  "%" @punctuation.special)

; The `%%` lines: template options where a template opens explicitly, and the
; terminator where it closes explicitly.
[
  (template_options)
  (template_end)
] @keyword.directive

; The Printf conversion of an output directive -- the `s` of `<%s x %>`.
(format) @string.special

; `!` suppresses `Dream.html_escape`, so unescaped output is worth seeing.
(raw) @keyword.directive
