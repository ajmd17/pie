#pragma once

#include <bcparse/source_stream.hpp>
#include <bcparse/source_location.hpp>
#include <bcparse/token.hpp>

namespace bcparse {
  class SourceStream;
  class TokenStream;
  class CompilationUnit;

  class Lexer {
  public:
    Lexer(const SourceStream &sourceStream,
      TokenStream *tokenStream,
      CompilationUnit *compilationUnit);

    void analyze();

  private:
    bool skipWhitespace();
    bool hasNext();
    Token nextToken();
    utf::u32char readEscapeCode();
    Token readStringLiteral();
    Token readNumberLiteral();
    Token readHexNumberLiteral();
    Token readComment();
    Token readIdentifier();
    Token readDirective();
    Token readRegister();
    Token readLocal();
    Token readDataLocation(Token::TokenClass type);
    Token readInterpolation();

    SourceStream m_sourceStream;
    SourceLocation m_sourceLocation;
    TokenStream *m_tokenStream;
    CompilationUnit *m_compilationUnit;

  };
}
