; The top-level code and the bodies of the `%` code lines are one Reason
; statement stream, and they have to be captured by a *single* pattern to end
; up in a single combined injection: `let render tasks =` opens a binding that
; `% tasks |> List.iter begin fun ... ->` continues and `% end;` closes.  Split
; across two patterns they would be combined into two separate documents and
; every `begin` would look unbalanced.
([
  (ocaml_block) @injection.content
  (code_line
    (code) @injection.content)
]
  (#set! injection.language "reason")
  (#set! injection.combined))

; Directive bodies are self-contained: `<% x %>` is a statement and
; `<%s name %>` an expression.  They are injected one at a time, on purpose --
; splicing `name` into the statement stream above would land it in the middle
; of a `begin ... end` and derail the parse from there to the end of the block.
(directive
  (code) @injection.content
  (#set! injection.language "reason"))

(output_directive
  (code) @injection.content
  (#set! injection.language "reason"))

; Template text, with the directives cut out of it, as one HTML document: the
; per-line `text` nodes are fragments of a single page, not pages of their own.
((text) @injection.content
  (#set! injection.language "html")
  (#set! injection.combined))
