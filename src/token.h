#pragma once
#include <string>

namespace lev::token {

  enum class TokenType {
    RightArrow,
    Function,
    Public,
    Identifier,
    Colon,

    Integer,
    Float,
    String,

    For,
    While,
    Return,
    If,
    Else,
    Let,
    Mutable,
    
    Bang,
    BangEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Equal,
    EqualEqual,

    Plus,
    PlusEqual,
    Star,
    StarEqual,
    Minus,
    MinusEqual,
    Slash,
    SlashEqual,

    EndOfFile,
  };

  struct Token {
    TokenType type;
    std::string_view lexeme;
    Token(TokenType type, std::string_view lexeme) : type(type), lexeme(lexeme) {}
    Token() = default;
  };

  enum class Type {
    UserDefined,

    i8,
    i16,
    i32,
    i64,

    u8,
    u16,
    u32,
    u64,

    f32,
    f64,
  };

}
