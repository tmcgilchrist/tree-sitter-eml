import XCTest
import SwiftTreeSitter
import TreeSitterEml

final class TreeSitterEmlTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_eml())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Eml grammar")
    }
}
