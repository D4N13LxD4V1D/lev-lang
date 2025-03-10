#include "parser.h"
#include "scanner.h"
#include "utils.h"

#include <print>
#include <memory>
#include <ranges>
#include <format>
#include <map>

using namespace lev::parser;
using namespace lev::scanner;
using namespace lev::token;

Parser::Parser(std::string_view source) {
  Scanner scanner(source);
  auto tokens = scanner.scan();
  if (not tokens) {
    Scanner::printError(tokens.error());
    return;
  }
  mTokens = std::move(*tokens);
};

Parser::Parser(std::vector<Token> tokens) : mTokens(std::move(tokens)) {};

static Token lastToken(TokenType::EndOfFile, "");

auto Parser::advance() -> const Token& {
  if (isAtEnd()) {
    return lastToken;
  }
  return mTokens[mCurrent++];
}

auto Parser::isAtEnd() const -> bool {
  return mCurrent >= mTokens.size();
}

auto Parser::peek() const -> const Token& {
  if (isAtEnd()) return lastToken;
  return mTokens[mCurrent];
}

auto Parser::peekPrev() const -> const Token& {
  if (mCurrent == 0) return mTokens[0];
  return mTokens[mCurrent - 1];
}

auto Parser::match(TokenType type) -> bool {
  if (isAtEnd()) return false;
  if (type == peek().type) {
    mCurrent += 1;
    return true;
  }
  return false;
}

auto Parser::check(TokenType type) const -> bool {
  if (isAtEnd()) return false;
  if (type == peek().type) {
    return true;
  }
  return false;
}

auto Parser::expect(TokenType type) -> std::optional<Token> {
  if (isAtEnd()) return std::nullopt;

  if (type == peek().type) {
    return mTokens[mCurrent++];
  } else {
    return std::nullopt;
  }
}

auto Parser::parse() -> std::expected<std::vector<Stmt>, ParserError> {
  std::vector<Stmt> statements;
  while (not isAtEnd()) {
    while (match(TokenType::Newline)) {
      continue;
    }
    auto stmt = parseDeclaration();
    if (not stmt) {
      return std::unexpected(stmt.error());
    }
    statements.push_back(std::move(*stmt));
  }
  return std::move(statements);
}


auto Parser::parseDeclaration() -> std::expected<Stmt, ParserError> {
  if (match(TokenType::Function)) {
    return parseFunctionDeclaration();
  }
  if (match(TokenType::Let)) {
    return parseVariableDeclaration();
  }
  return parseStmt();
}

auto Parser::parseType() -> std::expected<Type, ParserError>{

  static constexpr auto lexemes = {
    "i8",
    "i16",
    "i32",
    "i64",
    "u8",
    "u16",
    "u32",
    "u64",
    "f32",
    "f64",
    "bool",
  };

  static constexpr auto types = {
    Type::i8,
    Type::i16,
    Type::i32,
    Type::i64,
    Type::u8,
    Type::u16,
    Type::u32,
    Type::u64,
    Type::f32,
    Type::f64,
    Type::Bool,
  };


  if (peek().type != TokenType::Identifier) {
    return std::unexpected(UnexpectedToken(TokenType::Identifier, peek().type));
  }

  for (auto [lexeme, type] : std::views::zip(lexemes, types)) {
    if (peek().lexeme == lexeme) {
      advance();
      return type;
    }
  }

  return Type::UserDefined;
}

auto Parser::parseVariableDeclaration() -> std::expected<Stmt, ParserError> {
  bool isMutable = false;
  if (match(TokenType::Mutable)) {
    isMutable = true;
  }

  auto identifier = expect(TokenType::Identifier);
  if (not identifier) {
    return std::unexpected(UnexpectedToken(TokenType::Identifier, peek().type));
  }

  if (not match(TokenType::Colon)) {
    return std::unexpected(UnexpectedToken(TokenType::Colon, peek().type));
  }

  auto type = parseType();
  if (not type) {
    return std::unexpected(type.error());
  }

  if (not match(TokenType::Equal)) {
    return std::unexpected(UnexpectedToken(TokenType::Equal, peek().type));
  }

  auto expr = parseExpr();
  if (not expr) {
    return std::unexpected(expr.error());
  }

  return VariableDeclaration(*identifier, isMutable, std::move(*expr), *type);
}


auto Parser::parseFunctionDeclaration() -> std::expected<Stmt, ParserError> {
  auto identifier = expect(TokenType::Identifier);
  if (not identifier) {
    return std::unexpected(UnexpectedToken(TokenType::Identifier, peek().type));
  }

  auto args = std::vector<FunctionArg>{};
  if (not match(TokenType::LeftParen)) {
    return std::unexpected(UnexpectedToken(TokenType::LeftParen, peek().type));
  }

  while (not match(TokenType::RightParen)) {
    auto argIdentifier = expect(TokenType::Identifier);
    if (not argIdentifier) {
      return std::unexpected(UnexpectedToken(TokenType::Identifier, peek().type));
    }

    if (not match(TokenType::Colon)) {
      return std::unexpected(UnexpectedToken(TokenType::Colon, peek().type));
    }

    auto type = parseType();
    if (not type) {
      return std::unexpected(type.error());
    }

    args.push_back({argIdentifier->lexeme, *type});

    if (not match(TokenType::Comma)) {
      if (not match(TokenType::RightParen)) {
        return std::unexpected(UnexpectedToken(TokenType::RightParen, peek().type));
      }
      break;
    }
  }

  if (not match(TokenType::RightArrow)) {
    return std::unexpected(UnexpectedToken(TokenType::RightArrow, peek().type));
  }

  auto returnType = parseType();

  if (not returnType) {
    return std::unexpected(returnType.error());
  }

  if (not match(TokenType::Colon)) {
    return std::unexpected(UnexpectedToken(TokenType::Colon, peek().type));
  }

  if (not match(TokenType::Indent)) {
    return std::unexpected(UnexpectedToken(TokenType::Indent, peek().type));
  }

  auto body = parseBlock();
  if (not body) {
    return body;
  }

  return FunctionDeclaration(identifier->lexeme, args, *returnType, std::move(*body));
}

auto Parser::parseBlock() -> std::expected<Stmt, ParserError> {
  auto statements = std::vector<Stmt>{};
  while (not match(TokenType::Dedent) and not isAtEnd()) {
    while (match(TokenType::Newline)) {
      continue;
    }

    auto stmt = parseStmt();
    if (not stmt) {
      return stmt;
    }
    statements.push_back(std::move(*stmt));
  }

  return BlockStmt(std::move(statements));
}

auto Parser::parseStmt() -> std::expected<Stmt, ParserError> {

  if (match(TokenType::Let)) {
    return parseVariableDeclaration();
  }

  if (match(TokenType::If)) {
    return parseIfStmt();
  }

  if (match(TokenType::For)) {
    return std::unexpected(Unimplemented{});
  }

  if (match(TokenType::Identifier)) {
    return parseAssignmentStmt();
  }

  if (match(TokenType::Return)) {
    return parseReturnStmt();
  }
  
  return std::unexpected(Unimplemented{});
}
auto Parser::parseReturnStmt() -> std::expected<Stmt, ParserError> {
  if (match(TokenType::Newline)) {
    return ReturnStmt(std::nullopt);
  }

  auto expr = parseExpr();
  if (not expr) {
    return std::unexpected(expr.error());
  }
  return ReturnStmt(std::move(*expr));
}
auto Parser::parseIfStmt() -> std::expected<Stmt, ParserError> {
  using Branch = IfStmt::Branch;

  auto condition = parseExpr();
  if (not condition) {
    return std::unexpected(condition.error());
  }

  if (not match(TokenType::Colon)) {
    return std::unexpected(UnexpectedToken(TokenType::Colon, peek().type));
  }

  if (not match(TokenType::Indent)) {
    return std::unexpected(UnexpectedToken(TokenType::Indent, peek().type));
  }


  auto body = parseBlock();
  if (not body) {
    return std::unexpected(body.error());
  }

  auto ifBranch = Branch(std::move(*condition), std::move(*body));
  std::vector<Branch> elseIfBranches;
  while (match(TokenType::Else) and match(TokenType::If)) {
    auto condition = parseExpr();
    if (not condition) {
      return std::unexpected(condition.error());
    }

    if (not match(TokenType::Colon)) {
      return std::unexpected(UnexpectedToken(TokenType::Colon, peek().type));
    }

    if (not match(TokenType::Indent)) {
      return std::unexpected(UnexpectedToken(TokenType::Indent, peek().type));
    }

    auto body = parseBlock();
    if (not body) {
      return std::unexpected(body.error());
    }

    elseIfBranches.push_back(Branch(std::move(*condition), std::move(*body)));
  }

  std::optional<Stmt> elseBody = std::nullopt;
  if (peekPrev().type == TokenType::Else) {

    if (not match(TokenType::Colon)) {
      return std::unexpected(UnexpectedToken(TokenType::Colon, peek().type));
    }

    if (not match(TokenType::Indent)) {
      return std::unexpected(UnexpectedToken(TokenType::Indent, peek().type));
    }

    auto body = parseBlock();
    if (not body) {
      return std::unexpected(body.error());
    }
    elseBody = std::move(*body);
  }

  return IfStmt(std::move(ifBranch), std::move(elseIfBranches), std::move(elseBody));
}

auto Parser::parseAssignmentStmt() -> std::expected<Stmt, ParserError> {
  const auto identifier = peekPrev();

  if (not match(TokenType::Equal)) {
    return std::unexpected(UnexpectedToken(TokenType::Equal, peek().type));
  }

  auto value = parseExpr();
  if (not value) {
    return std::unexpected(value.error());
  }

  return AssignStmt(identifier, std::move(*value));
}

auto Parser::parseExpr() -> std::expected<Expr, ParserError> {
  auto lhs = parsePrimaryExpr();
  if (not lhs) {
    return lhs;
  }
  return parseBinaryOpRHS(0, std::move(*lhs));
}

auto Parser::parseBinaryOpRHS(int exprPrec, Expr lhs) -> std::expected<Expr, ParserError> {

  static constexpr auto getTokenPrecedence = [](TokenType tokenType) -> int {
    switch (tokenType) {
      case TokenType::Less:
      case TokenType::LessEqual:
      case TokenType::Greater:
      case TokenType::GreaterEqual:
      case TokenType::EqualEqual:
      case TokenType::BangEqual:
        return 10;
      case TokenType::Plus:
      case TokenType::PlusEqual:
      case TokenType::Minus:
      case TokenType::MinusEqual:
        return 20;
      case TokenType::Star:
      case TokenType::StarEqual:
      case TokenType::Slash:
      case TokenType::SlashEqual:
        return 40;
      default:
        return -1;
    }
  };

  while (true) {
    int tokenPrec = getTokenPrecedence(peek().type);
    if (tokenPrec < exprPrec) {
      return lhs;
    }
    auto binOp = advance();
    auto rhs = parsePrimaryExpr();
    if (not rhs) {
      return rhs;
    }

    int nextPrec = getTokenPrecedence(peek().type);
    if (tokenPrec < nextPrec) {
      rhs = parseBinaryOpRHS(tokenPrec + 1, std::move(*rhs));

      if (not rhs) {
        return rhs;
      }
    }

    lhs = BinaryExpr(binOp, std::move(lhs), std::move(*rhs));
  }

}

auto Parser::parsePrimaryExpr() -> std::expected<Expr, ParserError> {
  if (match(TokenType::Float) or match(TokenType::Integer) or match(TokenType::String)) {
    return LiteralExpr(peekPrev());
  }

  if (match(TokenType::False) or match(TokenType::True)) {
    return LiteralExpr(peekPrev());
  }

  if (match(TokenType::Identifier)) {
    return VariableExpr(peekPrev());
  }

  return std::unexpected(Unimplemented{});
}

auto Parser::printError(ParserError error) -> void {
  static const auto visitor = overloaded {
    [](const UnexpectedToken &err) -> void {
      std::println(stderr, "PARSER ERROR: Got an unexpected token `{}` expected `{}",
        tokenTypeToString(err.found), tokenTypeToString(err.required));
    },
    [](const Unimplemented &err) -> void {
      std::println(stderr, "PARSER ERROR: Unimplemented!");
    }
  };
  std::visit(visitor, error);
}

