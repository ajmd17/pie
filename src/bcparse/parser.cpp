#include <typeinfo>

#include <bcparse/parser.hpp>
#include <bcparse/lexer.hpp>
#include <bcparse/analyzer.hpp>
#include <bcparse/source_file.hpp>
#include <bcparse/source_stream.hpp>

#include <bcparse/ast/ast_directive.hpp>
#include <bcparse/ast/ast_label_decl.hpp>
#include <bcparse/ast/ast_label.hpp>
#include <bcparse/ast/ast_string_literal.hpp>
#include <bcparse/ast/ast_integer_literal.hpp>
#include <bcparse/ast/ast_data_location.hpp>
#include <bcparse/ast/ast_identifier.hpp>
#include <bcparse/ast/ast_jmp_statement.hpp>
#include <bcparse/ast/ast_cmp_statement.hpp>

#include <common/my_assert.hpp>

#include <string>
#include <map>

namespace bcparse {
  Parser::Parser(AstIterator *astIterator,
    TokenStream *tokenStream,
    CompilationUnit *compilationUnit)
    : m_astIterator(astIterator),
      m_tokenStream(tokenStream),
      m_compilationUnit(compilationUnit) {
  }

  Parser::Parser(const Parser &other)
    : m_astIterator(other.m_astIterator),
      m_tokenStream(other.m_tokenStream),
      m_compilationUnit(other.m_compilationUnit) {
  }

  Token Parser::match(Token::TokenClass tokenClass, bool read) {
    Token peek = m_tokenStream->peek();

    if (peek && peek.getTokenClass() == tokenClass) {
      if (read && m_tokenStream->hasNext()) {
        m_tokenStream->next();
      }

      return peek;
    }

    return Token::EMPTY;
  }

  Token Parser::matchAhead(Token::TokenClass tokenClass, int n) {
    Token peek = m_tokenStream->peek(n);

    if (peek && peek.getTokenClass() == tokenClass) {
      return peek;
    }

    return Token::EMPTY;
  }

  Token Parser::expect(Token::TokenClass tokenClass, bool read) {
    Token token = match(tokenClass, read);

    if (!token) {
      const SourceLocation location = currentLocation();

      ErrorMessage errorMsg;
      std::string errorStr;

      switch (tokenClass) {
        case Token::TK_IDENT:
          errorMsg = Msg_expected_identifier;
          break;
        default:
          errorMsg = Msg_expected_token;
          errorStr = Token::tokenTypeToString(tokenClass);
      }

      m_compilationUnit->getErrorList().addError(CompilerError(
        LEVEL_ERROR,
        errorMsg,
        location,
        errorStr
      ));
    }

    return token;
  }

  bool Parser::expectEndOfStmt() {
    const SourceLocation location = currentLocation();

    if (!match(Token::TK_NEWLINE, true)) {
      m_compilationUnit->getErrorList().addError(CompilerError(
        LEVEL_ERROR,
        Msg_expected_end_of_statement,
        location
      ));

      // skip until end of statement, end of line, or end of file.
      do {
        m_tokenStream->next();
      } while (m_tokenStream->hasNext() && !match(Token::TK_NEWLINE, true));

      return false;
    }

    return true;
  }

  SourceLocation Parser::currentLocation() const {
    if (m_tokenStream->getSize() != 0 && !m_tokenStream->hasNext()) {
      return m_tokenStream->last().getLocation();
    }

    return m_tokenStream->peek().getLocation();
  }

  void Parser::skipStatementTerminators() {
    // read past statement terminator tokens
    while (match(Token::TK_NEWLINE, true));
  }

  void Parser::parse() {
    skipStatementTerminators();

    // first pass; hoist macro definitions
    std::vector<Pointer<AstStatement>> hoisted;
    std::vector<Pointer<AstStatement>> otherStmts;

    while (m_tokenStream->hasNext()) {
      if (auto stmt = parseStatement()) {
        if (stmt->isHoisted()) {
          hoisted.push_back(stmt);
        } else {
          otherStmts.push_back(stmt);
        }
      } else {
        break;
      }
    }

    for (const auto &stmt : hoisted) {
      m_astIterator->push(stmt);
    }

    for (const auto &stmt : otherStmts) {
      m_astIterator->push(stmt);
    }
  }

  Pointer<AstStatement> Parser::parseStatement() {
    const SourceLocation location = currentLocation();
    Pointer<AstStatement> res;

    skipStatementTerminators();

    if (!m_tokenStream->hasNext()) {
      return nullptr;
    }

    if (match(Token::TK_DIRECTIVE)) {
      res = parseDirective();
    } else if (match(Token::TK_LABEL)) {
      res = parseLabel();
    } else if (match(Token::TK_IDENT)) {
      res = parseCommand();
    } else {
      res = parseExpression();

      // if (!res) {
      //   m_compilationUnit->getErrorList().addError(CompilerError(
      //     LEVEL_ERROR,
      //     Msg_illegal_expression,
      //     location
      //   ));
      // }
    }

    if (res != nullptr && m_tokenStream->hasNext()) {
      expectEndOfStmt();
    }

    return res;
  }

  Pointer<AstExpression> Parser::parseExpression() {
    if (auto term = parseTerm()) {
      return term;
    }

    return nullptr;
  }

  Pointer<AstExpression> Parser::parseTerm() {
    Token token = m_tokenStream->peek();

    if (!token) {
      m_compilationUnit->getErrorList().addError(CompilerError(
        LEVEL_ERROR,
        Msg_unexpected_eof,
        currentLocation()
      ));

      if (m_tokenStream->hasNext()) {
        m_tokenStream->next();
      }

      return nullptr;
    }

    Pointer<AstExpression> expr;

    if (match(Token::TK_IDENT)) {
      expr = parseIdentifier();
    } else if (match(Token::TK_OPEN_PARENTH)) {
      // expr = parseParentheses();
    } else if (match(Token::TK_OPEN_BRACKET)) {
      // expr = parseArrayExpression();
    } else if (match(Token::TK_INTEGER)) {
      expr = parseIntegerLiteral();
    } else if (match(Token::TK_FLOAT)) {
      // expr = parseFloatLiteral();
    } else if (match(Token::TK_STRING)) {
      expr = parseStringLiteral();
    } else if (match(Token::TK_INTERPOLATION)) {
      expr = parseInterpolation();
    } else if (match(Token::TK_REG)) {
      expr = parseRegister();
    } else if (match(Token::TK_LOCAL)) {
      expr = parseLocal();
    } else {
      if (token.getTokenClass() == Token::TK_NEWLINE) {
        m_compilationUnit->getErrorList().addError(CompilerError(
          LEVEL_ERROR,
          Msg_unexpected_eol,
          token.getLocation()
        ));
      } else {
        m_compilationUnit->getErrorList().addError(CompilerError(
          LEVEL_ERROR,
          Msg_unexpected_token,
          token.getLocation(),
          token.getValue()
        ));
      }

      if (m_tokenStream->hasNext()) {
          m_tokenStream->next();
      }

      return nullptr;
    }

    return expr;
  }

  Pointer<AstIdentifier> Parser::parseIdentifier() {
    if (Token token = expect(Token::TK_IDENT, true)) {
      return Pointer<AstIdentifier>(new AstIdentifier(
        token.getValue(),
        token.getLocation()
      ));
    }

    return nullptr;
  }

  Pointer<AstIntegerLiteral> Parser::parseIntegerLiteral() {
    if (Token token = expect(Token::TK_INTEGER, true)) {
      std::istringstream ss(token.getValue());
      int64_t value;
      ss >> value;

      return Pointer<AstIntegerLiteral>(new AstIntegerLiteral(
        value,
        token.getLocation()
      ));
    }

    return nullptr;
  }

  Pointer<AstStringLiteral> Parser::parseStringLiteral() {
    if (Token token = expect(Token::TK_STRING, true)) {
      return Pointer<AstStringLiteral>(new AstStringLiteral(
        token.getValue(),
        token.getLocation()
      ));
    }

    return nullptr;
  }

  Pointer<AstDirective> Parser::parseDirective() {
    if (Token token = expect(Token::TK_DIRECTIVE, true)) {
      std::vector<Pointer<AstExpression>> arguments;
      std::stringstream body;

      while (m_tokenStream->hasNext() &&
        !match(Token::TK_NEWLINE) &&
        !match(Token::TK_OPEN_BRACE)) {
        if (auto expr = parseTerm()) {
          arguments.emplace_back(expr);
        } else {
          break;
        }
      }

      if (match(Token::TK_OPEN_BRACE, true)) {
        int parenCounter = 1;

        while (m_tokenStream->hasNext()) {
          if (match(Token::TK_OPEN_BRACE)) {
            ++parenCounter;
          } else if (match(Token::TK_CLOSE_BRACE)) {
            --parenCounter;

            if (parenCounter == 0) {
              m_tokenStream->next();
              break;
            }
          }

          body << Token::getRepr(m_tokenStream->peek());
          body << ' ';

          m_tokenStream->next();
        }
      }

      return Pointer<AstDirective>(new AstDirective(
        token.getValue(),
        arguments,
        body.str(),
        token.getLocation()
      ));
    }

    return nullptr;
  }

  Pointer<AstLabelDecl> Parser::parseLabel() {
    if (Token token = expect(Token::TK_LABEL, true)) {
      // add forward declaration for labels
      std::cout << "add label " << token.getValue() << "\n";
      Pointer<AstLabel> astLabel(new AstLabel(token.getValue(), token.getLocation()));
      m_compilationUnit->getBoundGlobals().set(token.getValue(), astLabel);

      return Pointer<AstLabelDecl>(new AstLabelDecl(
        token.getValue(),
        astLabel,
        token.getLocation()
      ));
    }

    return nullptr;
  }

  Pointer<AstStatement> Parser::parseCommand() {
    if (Token token = expect(Token::TK_IDENT, true)) {
      const std::map<std::string, AstJmpStatement::JumpMode> jumpModeStrings = {
        { "jmp", AstJmpStatement::JumpMode::None },
        { "je", AstJmpStatement::JumpMode::JumpIfEqual },
        { "jne", AstJmpStatement::JumpMode::JumpIfNotEqual },
        { "jg", AstJmpStatement::JumpMode::JumpIfGreater },
        { "jge", AstJmpStatement::JumpMode::JumpIfGreaterOrEqual },
      };

      if (jumpModeStrings.find(token.getValue()) != jumpModeStrings.end()) {
        // m_tokenStream->next();

        auto expr = parseExpression();

        if (!expr) {
          return nullptr;
        }

        return Pointer<AstJmpStatement>(new AstJmpStatement(
          expr,
          jumpModeStrings.find(token.getValue())->second,
          token.getLocation()
        ));
      } else if (token.getValue() == "cmp") {
        // m_tokenStream->next();

        auto left = parseExpression();

        if (!left) {
          return nullptr;
        }

        auto right = parseExpression();

        if (!right) {
          return nullptr;
        }

        return Pointer<AstCmpStatement>(new AstCmpStatement(
          left,
          right,
          token.getLocation()
        ));
      } else {
        m_tokenStream->rewind();
        // TODO: a flag to allow identifiers at top level? (for interpolations)
        return parseIdentifier();

        // m_compilationUnit->getErrorList().addError(CompilerError(
        //   LEVEL_ERROR,
        //   Msg_unknown_opcode,
        //   token.getLocation(),
        //   token.getValue()
        // ));
      } // .. more
    }

    m_tokenStream->next();

    return nullptr;
  }

  Pointer<AstExpression> Parser::parseInterpolation() {
    Token token = expect(Token::TK_INTERPOLATION, true);
    if (token.empty()) return nullptr;

    Pointer<AstExpression> res;

    // @TODO evaluate in-place and return the transformed result.
    // @macro directives build their own lexers+parsers with variables that are needed in place.
    // eventually, use reverse polish notation to evaluate, allowing simple operations
    std::string body = token.getValue();

    SourceFile sourceFile(token.getLocation().getFileName(), body.size());
    std::memcpy(sourceFile.getBuffer(), body.data(), body.size());

    SourceStream sourceStream(&sourceFile);
    TokenStream tokenStream(TokenStreamInfo { token.getLocation().getFileName() });

    CompilationUnit subUnit(m_compilationUnit->getDataStorage());
    subUnit.getBoundGlobals().setParent(&m_compilationUnit->getBoundGlobals());

    Lexer lexer(sourceStream, &tokenStream, &subUnit);
    lexer.analyze();

    while (tokenStream.hasNext()) {
      const Token token = tokenStream.next();

      // mini-parser
      switch (token.getTokenClass()) {
      case Token::TK_IDENT:
        // return parseIdentifier();
        if (auto value = m_compilationUnit->getBoundGlobals().get(token.getValue())) {
          return value;
        } else {
          m_compilationUnit->getErrorList().addError(CompilerError(
            LEVEL_ERROR,
            Msg_undeclared_identifier,
            token.getLocation(),
            token.getValue()
          ));
        }

        break;
      default:
        m_compilationUnit->getErrorList().addError(CompilerError(
          LEVEL_ERROR,
          Msg_expected_identifier,
          token.getLocation()
        ));
      }
    }

    return nullptr;

#if 0
    AstIterator iterator;
    Parser subparser(&iterator, &tokenStream, &subUnit);
    subparser.parse();

    iterator.resetPosition();

    Analyzer analyzer(&iterator, &subUnit);
    analyzer.analyze();

    for (auto &error : subUnit.getErrorList().getErrors()) {
      m_compilationUnit->getErrorList().addError(error);
    }

    if (!subUnit.getErrorList().hasFatalErrors()) {
      iterator.resetPosition();

      if (iterator.hasNext()) {
        res = std::dynamic_pointer_cast<AstExpression>(iterator.next());
      }

      if (res == nullptr) {
        m_compilationUnit->getErrorList().addError(CompilerError(
          LEVEL_ERROR,
          Msg_illegal_expression,
          token.getLocation()
        ));
      }
    }

    return res;
#endif
  }

  Pointer<AstDataLocation> Parser::parseRegister() {
    if (Token token = expect(Token::TK_REG, true)) {
      std::istringstream ss(token.getValue());
      int value;
      ss >> value;

      return Pointer<AstDataLocation>(new AstDataLocation(
        value,
        ObjLoc::DataStoreLocation::RegisterDataStore,
        token.getLocation()
      ));
    }

    return nullptr;
  }

  Pointer<AstDataLocation> Parser::parseLocal() {
    if (Token token = expect(Token::TK_LOCAL, true)) {
      std::istringstream ss(token.getValue());
      int value;
      ss >> value;

      return Pointer<AstDataLocation>(new AstDataLocation(
        value,
        ObjLoc::DataStoreLocation::LocalDataStore,
        token.getLocation()
      ));
    }

    return nullptr;
  }
}
