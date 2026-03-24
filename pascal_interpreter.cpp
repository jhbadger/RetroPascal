/*
 * Pascal Interpreter in C++
 *
 * Supports:
 *  - Integer, Real, Boolean, String, Char types
 *  - Variables (VAR declarations)
 *  - Constants (CONST declarations)
 *  - Arithmetic and logical expressions
 *  - Assignment (:=)
 *  - IF / THEN / ELSE
 *  - WHILE / DO
 *  - FOR / TO / DOWNTO / DO
 *  - REPEAT / UNTIL
 *  - BEGIN / END blocks
 *  - WRITE / WRITELN / READ / READLN
 *  - Procedures and Functions (with parameters)
 *  - Recursion
 *  - Arrays (1D)
 *  - String operations
 *  - Built-ins: SQR, SQRT, ABS, ODD, TRUNC, ROUND, ORD, CHR, LENGTH, UPCASE, LOWERCASE
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <variant>
#include <functional>

// ─────────────────────────────────────────────
//  Tokens
// ─────────────────────────────────────────────
enum class TokenType {
    // Literals
    INTEGER_LITERAL, REAL_LITERAL, STRING_LITERAL, CHAR_LITERAL,
    // Identifiers / keywords
    IDENTIFIER,
    // Keywords
    PROGRAM, VAR, CONST, BEGIN, END, IF, THEN, ELSE,
    WHILE, DO, FOR, TO, DOWNTO, REPEAT, UNTIL,
    PROCEDURE, FUNCTION, ARRAY, OF, RECORD,
    DIV, MOD, AND, OR, NOT, XOR,
    TRUE, FALSE,
    INTEGER, REAL, BOOLEAN, STRING, CHAR,
    WRITE, WRITELN, READ, READLN,
    // Operators
    PLUS, MINUS, STAR, SLASH, ASSIGN, COLON, SEMICOLON, COMMA, DOT, DOTDOT,
    LPAREN, RPAREN, LBRACKET, RBRACKET,
    EQ, NEQ, LT, LE, GT, GE,
    // Special
    EOF_TOKEN
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"program",   TokenType::PROGRAM},   {"var",       TokenType::VAR},
    {"const",     TokenType::CONST},     {"begin",     TokenType::BEGIN},
    {"end",       TokenType::END},       {"if",        TokenType::IF},
    {"then",      TokenType::THEN},      {"else",      TokenType::ELSE},
    {"while",     TokenType::WHILE},     {"do",        TokenType::DO},
    {"for",       TokenType::FOR},       {"to",        TokenType::TO},
    {"downto",    TokenType::DOWNTO},    {"repeat",    TokenType::REPEAT},
    {"until",     TokenType::UNTIL},     {"procedure", TokenType::PROCEDURE},
    {"function",  TokenType::FUNCTION},  {"array",     TokenType::ARRAY},
    {"of",        TokenType::OF},        {"record",    TokenType::RECORD},
    {"div",       TokenType::DIV},       {"mod",       TokenType::MOD},
    {"and",       TokenType::AND},       {"or",        TokenType::OR},
    {"not",       TokenType::NOT},       {"xor",       TokenType::XOR},
    {"true",      TokenType::TRUE},      {"false",     TokenType::FALSE},
    {"integer",   TokenType::INTEGER},   {"real",      TokenType::REAL},
    {"boolean",   TokenType::BOOLEAN},   {"string",    TokenType::STRING},
    {"char",      TokenType::CHAR},
    {"write",     TokenType::WRITE},     {"writeln",   TokenType::WRITELN},
    {"read",      TokenType::READ},      {"readln",    TokenType::READLN},
};

// ─────────────────────────────────────────────
//  Lexer
// ─────────────────────────────────────────────
class Lexer {
    std::string src;
    size_t pos = 0;
    int line = 1;

    char peek(int offset = 0) {
        size_t p = pos + offset;
        return p < src.size() ? src[p] : '\0';
    }
    char advance() {
        char c = src[pos++];
        if (c == '\n') line++;
        return c;
    }
    void skipWhitespaceAndComments() {
        while (pos < src.size()) {
            char c = peek();
            if (std::isspace(c)) { advance(); }
            else if (c == '{') {
                while (pos < src.size() && peek() != '}') advance();
                if (pos < src.size()) advance();
            }
            else if (c == '(' && peek(1) == '*') {
                advance(); advance();
                while (pos < src.size() && !(peek() == '*' && peek(1) == ')')) advance();
                if (pos < src.size()) { advance(); advance(); }
            }
            else if (c == '/' && peek(1) == '/') {
                while (pos < src.size() && peek() != '\n') advance();
            }
            else break;
        }
    }

public:
    Lexer(const std::string& source) : src(source) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (true) {
            skipWhitespaceAndComments();
            if (pos >= src.size()) {
                tokens.push_back({TokenType::EOF_TOKEN, "", line});
                break;
            }
            int tok_line = line;
            char c = peek();

            // Number
            if (std::isdigit(c)) {
                std::string num;
                bool is_real = false;
                while (pos < src.size() && std::isdigit(peek())) num += advance();
                if (peek() == '.' && peek(1) != '.') {
                    is_real = true; num += advance();
                    while (pos < src.size() && std::isdigit(peek())) num += advance();
                }
                if (peek() == 'e' || peek() == 'E') {
                    is_real = true; num += advance();
                    if (peek() == '+' || peek() == '-') num += advance();
                    while (pos < src.size() && std::isdigit(peek())) num += advance();
                }
                tokens.push_back({is_real ? TokenType::REAL_LITERAL : TokenType::INTEGER_LITERAL, num, tok_line});
                continue;
            }
            // String
            if (c == '\'') {
                advance();
                std::string s;
                while (pos < src.size()) {
                    if (peek() == '\'' && peek(1) == '\'') { s += '\''; advance(); advance(); }
                    else if (peek() == '\'') { advance(); break; }
                    else s += advance();
                }
                if (s.size() == 1)
                    tokens.push_back({TokenType::CHAR_LITERAL, s, tok_line});
                else
                    tokens.push_back({TokenType::STRING_LITERAL, s, tok_line});
                continue;
            }
            // Identifier / keyword
            if (std::isalpha(c) || c == '_') {
                std::string id;
                while (pos < src.size() && (std::isalnum(peek()) || peek() == '_')) id += advance();
                std::string lower = id;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                auto it = KEYWORDS.find(lower);
                if (it != KEYWORDS.end())
                    tokens.push_back({it->second, lower, tok_line});
                else
                    tokens.push_back({TokenType::IDENTIFIER, id, tok_line});
                continue;
            }
            // Operators / punctuation
            advance();
            switch (c) {
                case '+': tokens.push_back({TokenType::PLUS,      "+", tok_line}); break;
                case '-': tokens.push_back({TokenType::MINUS,     "-", tok_line}); break;
                case '*': tokens.push_back({TokenType::STAR,      "*", tok_line}); break;
                case '/': tokens.push_back({TokenType::SLASH,     "/", tok_line}); break;
                case '(': tokens.push_back({TokenType::LPAREN,    "(", tok_line}); break;
                case ')': tokens.push_back({TokenType::RPAREN,    ")", tok_line}); break;
                case '[': tokens.push_back({TokenType::LBRACKET,  "[", tok_line}); break;
                case ']': tokens.push_back({TokenType::RBRACKET,  "]", tok_line}); break;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";", tok_line}); break;
                case ',': tokens.push_back({TokenType::COMMA,     ",", tok_line}); break;
                case '=': tokens.push_back({TokenType::EQ,        "=", tok_line}); break;
                case '^': tokens.push_back({TokenType::XOR,       "xor", tok_line}); break;
                case ':':
                    if (peek() == '=') { advance(); tokens.push_back({TokenType::ASSIGN, ":=", tok_line}); }
                    else tokens.push_back({TokenType::COLON, ":", tok_line});
                    break;
                case '.':
                    if (peek() == '.') { advance(); tokens.push_back({TokenType::DOTDOT, "..", tok_line}); }
                    else tokens.push_back({TokenType::DOT, ".", tok_line});
                    break;
                case '<':
                    if (peek() == '=') { advance(); tokens.push_back({TokenType::LE,  "<=", tok_line}); }
                    else if (peek() == '>') { advance(); tokens.push_back({TokenType::NEQ, "<>", tok_line}); }
                    else tokens.push_back({TokenType::LT, "<", tok_line});
                    break;
                case '>':
                    if (peek() == '=') { advance(); tokens.push_back({TokenType::GE, ">=", tok_line}); }
                    else tokens.push_back({TokenType::GT, ">", tok_line});
                    break;
                default:
                    throw std::runtime_error("Unknown character '" + std::string(1,c) + "' at line " + std::to_string(tok_line));
            }
        }
        return tokens;
    }
};

// ─────────────────────────────────────────────
//  AST Nodes
// ─────────────────────────────────────────────
struct ASTNode;
using ASTPtr = std::shared_ptr<ASTNode>;

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct NumberNode     : ASTNode { double value; bool is_int; };
struct BoolNode       : ASTNode { bool value; };
struct StringNode     : ASTNode { std::string value; };
struct CharNode       : ASTNode { char value; };
struct VarNode        : ASTNode { std::string name; };
struct AssignNode     : ASTNode { ASTPtr target; ASTPtr value; };
struct BinOpNode      : ASTNode { std::string op; ASTPtr left, right; };
struct UnaryOpNode    : ASTNode { std::string op; ASTPtr operand; };
struct CompoundNode   : ASTNode { std::vector<ASTPtr> statements; };
struct IfNode         : ASTNode { ASTPtr cond, then_branch, else_branch; };
struct WhileNode      : ASTNode { ASTPtr cond, body; };
struct ForNode        : ASTNode { std::string var; ASTPtr from, to; bool downto; ASTPtr body; };
struct RepeatNode     : ASTNode { ASTPtr body, cond; };
struct WriteNode      : ASTNode { std::vector<ASTPtr> args; bool newline; };
struct ReadNode       : ASTNode { std::vector<ASTPtr> vars; bool newline; };
struct CallNode       : ASTNode { std::string name; std::vector<ASTPtr> args; };
struct IndexNode      : ASTNode { ASTPtr array; ASTPtr index; };
struct ProgramNode    : ASTNode {
    std::string name;
    std::vector<ASTPtr> declarations;  // procs/funcs/vars/consts
    ASTPtr body;
};

struct ParamDef { std::string name, type; bool by_ref; };
struct ProcDef : ASTNode {
    std::string name;
    std::vector<ParamDef> params;
    std::vector<ASTPtr> decls;
    ASTPtr body;
    bool is_function;
    std::string return_type;
};

struct VarDecl : ASTNode {
    std::vector<std::string> names;
    std::string type_name;
    int array_lo = 0, array_hi = 0;
    bool is_array = false;
};

struct ConstDecl : ASTNode { std::string name; ASTPtr value; };

// ─────────────────────────────────────────────
//  Parser
// ─────────────────────────────────────────────
class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;

    Token& peek(int offset = 0) { return tokens[std::min(pos + offset, tokens.size()-1)]; }
    Token consume() { return tokens[pos++]; }
    Token expect(TokenType t) {
        if (peek().type != t)
            throw std::runtime_error("Expected token type " + std::to_string((int)t) +
                " got '" + peek().value + "' at line " + std::to_string(peek().line));
        return consume();
    }
    bool check(TokenType t) { return peek().type == t; }
    bool match(TokenType t) { if (check(t)) { consume(); return true; } return false; }

public:
    Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}

    ASTPtr parse() {
        auto prog = std::make_shared<ProgramNode>();
        if (match(TokenType::PROGRAM)) {
            prog->name = expect(TokenType::IDENTIFIER).value;
            match(TokenType::SEMICOLON);
        }
        parseDeclarations(prog->declarations);
        prog->body = parseCompound();
        match(TokenType::DOT);
        return prog;
    }

private:
    void parseDeclarations(std::vector<ASTPtr>& decls) {
        while (true) {
            if (check(TokenType::VAR)) {
                consume();
                parseVarSection(decls);
            } else if (check(TokenType::CONST)) {
                consume();
                parseConstSection(decls);
            } else if (check(TokenType::PROCEDURE) || check(TokenType::FUNCTION)) {
                decls.push_back(parseProcOrFunc());
            } else break;
        }
    }

    void parseVarSection(std::vector<ASTPtr>& decls) {
        while (check(TokenType::IDENTIFIER)) {
            auto vd = std::make_shared<VarDecl>();
            vd->names.push_back(consume().value);
            while (match(TokenType::COMMA))
                vd->names.push_back(expect(TokenType::IDENTIFIER).value);
            expect(TokenType::COLON);
            // Array type
            if (match(TokenType::ARRAY)) {
                vd->is_array = true;
                expect(TokenType::LBRACKET);
                vd->array_lo = std::stoi(expect(TokenType::INTEGER_LITERAL).value);
                expect(TokenType::DOTDOT);
                vd->array_hi = std::stoi(expect(TokenType::INTEGER_LITERAL).value);
                expect(TokenType::RBRACKET);
                expect(TokenType::OF);
            }
            vd->type_name = consume().value;
            expect(TokenType::SEMICOLON);
            decls.push_back(vd);
        }
    }

    void parseConstSection(std::vector<ASTPtr>& decls) {
        while (check(TokenType::IDENTIFIER)) {
            auto cd = std::make_shared<ConstDecl>();
            cd->name = consume().value;
            expect(TokenType::EQ);
            cd->value = parseExpr();
            expect(TokenType::SEMICOLON);
            decls.push_back(cd);
        }
    }

    ASTPtr parseProcOrFunc() {
        bool is_func = check(TokenType::FUNCTION);
        consume();
        auto pd = std::make_shared<ProcDef>();
        pd->is_function = is_func;
        pd->name = expect(TokenType::IDENTIFIER).value;
        // params
        if (match(TokenType::LPAREN)) {
            while (!check(TokenType::RPAREN)) {
                // handle VAR params
                bool is_var = false;
                if (peek().type == TokenType::VAR) { consume(); is_var = true; }
                std::vector<std::string> pnames;
                pnames.push_back(expect(TokenType::IDENTIFIER).value);
                while (match(TokenType::COMMA))
                    pnames.push_back(expect(TokenType::IDENTIFIER).value);
                expect(TokenType::COLON);
                std::string ptype = consume().value;
                for (auto& n : pnames)
                    pd->params.push_back({n, ptype, is_var});
                if (!match(TokenType::SEMICOLON)) break;
            }
            expect(TokenType::RPAREN);
        }
        if (is_func) {
            expect(TokenType::COLON);
            pd->return_type = consume().value;
        }
        expect(TokenType::SEMICOLON);
        parseDeclarations(pd->decls);
        pd->body = parseCompound();
        expect(TokenType::SEMICOLON);
        return pd;
    }

    ASTPtr parseCompound() {
        expect(TokenType::BEGIN);
        auto node = std::make_shared<CompoundNode>();
        while (!check(TokenType::END) && !check(TokenType::EOF_TOKEN)) {
            node->statements.push_back(parseStatement());
            match(TokenType::SEMICOLON);
        }
        expect(TokenType::END);
        return node;
    }

    ASTPtr parseStatement() {
        if (check(TokenType::BEGIN))     return parseCompound();
        if (check(TokenType::IF))        return parseIf();
        if (check(TokenType::WHILE))     return parseWhile();
        if (check(TokenType::FOR))       return parseFor();
        if (check(TokenType::REPEAT))    return parseRepeat();
        if (check(TokenType::WRITE) || check(TokenType::WRITELN)) return parseWrite();
        if (check(TokenType::READ) || check(TokenType::READLN))   return parseRead();
        if (check(TokenType::IDENTIFIER)) return parseIdentStatement();
        return std::make_shared<CompoundNode>(); // empty
    }

    ASTPtr parseIf() {
        expect(TokenType::IF);
        auto node = std::make_shared<IfNode>();
        node->cond = parseExpr();
        expect(TokenType::THEN);
        node->then_branch = parseStatement();
        if (match(TokenType::ELSE))
            node->else_branch = parseStatement();
        return node;
    }

    ASTPtr parseWhile() {
        expect(TokenType::WHILE);
        auto node = std::make_shared<WhileNode>();
        node->cond = parseExpr();
        expect(TokenType::DO);
        node->body = parseStatement();
        return node;
    }

    ASTPtr parseFor() {
        expect(TokenType::FOR);
        auto node = std::make_shared<ForNode>();
        node->var = expect(TokenType::IDENTIFIER).value;
        expect(TokenType::ASSIGN);
        node->from = parseExpr();
        node->downto = check(TokenType::DOWNTO);
        if (!match(TokenType::TO)) expect(TokenType::DOWNTO);
        node->to = parseExpr();
        expect(TokenType::DO);
        node->body = parseStatement();
        return node;
    }

    ASTPtr parseRepeat() {
        expect(TokenType::REPEAT);
        auto node = std::make_shared<RepeatNode>();
        auto body = std::make_shared<CompoundNode>();
        while (!check(TokenType::UNTIL) && !check(TokenType::EOF_TOKEN)) {
            body->statements.push_back(parseStatement());
            match(TokenType::SEMICOLON);
        }
        node->body = body;
        expect(TokenType::UNTIL);
        node->cond = parseExpr();
        return node;
    }

    ASTPtr parseWrite() {
        auto node = std::make_shared<WriteNode>();
        node->newline = check(TokenType::WRITELN);
        consume();
        if (match(TokenType::LPAREN)) {
            node->args.push_back(parseExpr());
            while (match(TokenType::COMMA)) node->args.push_back(parseExpr());
            expect(TokenType::RPAREN);
        }
        return node;
    }

    ASTPtr parseRead() {
        auto node = std::make_shared<ReadNode>();
        node->newline = check(TokenType::READLN);
        consume();
        if (match(TokenType::LPAREN)) {
            node->vars.push_back(parsePrimary());
            while (match(TokenType::COMMA)) node->vars.push_back(parsePrimary());
            expect(TokenType::RPAREN);
        }
        return node;
    }

    ASTPtr parseIdentStatement() {
        std::string name = consume().value;
        // assignment or array index assignment
        if (check(TokenType::ASSIGN) || check(TokenType::LBRACKET)) {
            ASTPtr target;
            if (check(TokenType::LBRACKET)) {
                auto idx = std::make_shared<IndexNode>();
                auto varNode = std::make_shared<VarNode>(); varNode->name = name;
                idx->array = varNode;
                consume();
                idx->index = parseExpr();
                expect(TokenType::RBRACKET);
                target = idx;
            } else {
                auto v = std::make_shared<VarNode>(); v->name = name; target = v;
            }
            expect(TokenType::ASSIGN);
            auto assign = std::make_shared<AssignNode>();
            assign->target = target;
            assign->value = parseExpr();
            return assign;
        }
        // procedure/function call
        auto call = std::make_shared<CallNode>();
        call->name = name;
        if (match(TokenType::LPAREN)) {
            call->args.push_back(parseExpr());
            while (match(TokenType::COMMA)) call->args.push_back(parseExpr());
            expect(TokenType::RPAREN);
        }
        return call;
    }

    // Expression parsing with precedence
    ASTPtr parseExpr()     { return parseOr(); }
    ASTPtr parseOr() {
        auto left = parseAnd();
        while (check(TokenType::OR) || check(TokenType::XOR)) {
            std::string op = consume().value;
            auto node = std::make_shared<BinOpNode>();
            node->op = op; node->left = left; node->right = parseAnd();
            left = node;
        }
        return left;
    }
    ASTPtr parseAnd() {
        auto left = parseNot();
        while (check(TokenType::AND)) {
            consume();
            auto node = std::make_shared<BinOpNode>();
            node->op = "and"; node->left = left; node->right = parseNot();
            left = node;
        }
        return left;
    }
    ASTPtr parseNot() {
        if (check(TokenType::NOT)) {
            consume();
            auto node = std::make_shared<UnaryOpNode>();
            node->op = "not"; node->operand = parseNot();
            return node;
        }
        return parseComparison();
    }
    ASTPtr parseComparison() {
        auto left = parseAddSub();
        while (check(TokenType::EQ) || check(TokenType::NEQ) ||
               check(TokenType::LT) || check(TokenType::LE) ||
               check(TokenType::GT) || check(TokenType::GE)) {
            std::string op = consume().value;
            auto node = std::make_shared<BinOpNode>();
            node->op = op; node->left = left; node->right = parseAddSub();
            left = node;
        }
        return left;
    }
    ASTPtr parseAddSub() {
        auto left = parseMulDiv();
        while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
            std::string op = consume().value;
            auto node = std::make_shared<BinOpNode>();
            node->op = op; node->left = left; node->right = parseMulDiv();
            left = node;
        }
        return left;
    }
    ASTPtr parseMulDiv() {
        auto left = parseUnary();
        while (check(TokenType::STAR) || check(TokenType::SLASH) ||
               check(TokenType::DIV) || check(TokenType::MOD)) {
            std::string op = consume().value;
            auto node = std::make_shared<BinOpNode>();
            node->op = op; node->left = left; node->right = parseUnary();
            left = node;
        }
        return left;
    }
    ASTPtr parseUnary() {
        if (check(TokenType::MINUS)) {
            consume();
            auto node = std::make_shared<UnaryOpNode>();
            node->op = "-"; node->operand = parsePrimary();
            return node;
        }
        if (check(TokenType::PLUS)) { consume(); return parsePrimary(); }
        return parsePrimary();
    }
    ASTPtr parsePrimary() {
        if (check(TokenType::INTEGER_LITERAL)) {
            auto n = std::make_shared<NumberNode>();
            n->value = std::stod(consume().value); n->is_int = true;
            return n;
        }
        if (check(TokenType::REAL_LITERAL)) {
            auto n = std::make_shared<NumberNode>();
            n->value = std::stod(consume().value); n->is_int = false;
            return n;
        }
        if (check(TokenType::STRING_LITERAL)) {
            auto n = std::make_shared<StringNode>(); n->value = consume().value; return n;
        }
        if (check(TokenType::CHAR_LITERAL)) {
            auto n = std::make_shared<CharNode>(); n->value = consume().value[0]; return n;
        }
        if (check(TokenType::TRUE))  { consume(); auto n = std::make_shared<BoolNode>(); n->value = true;  return n; }
        if (check(TokenType::FALSE)) { consume(); auto n = std::make_shared<BoolNode>(); n->value = false; return n; }
        if (check(TokenType::LPAREN)) {
            consume();
            auto e = parseExpr();
            expect(TokenType::RPAREN);
            return e;
        }
        if (check(TokenType::IDENTIFIER)) {
            std::string name = consume().value;
            // Array index
            if (check(TokenType::LBRACKET)) {
                auto idx = std::make_shared<IndexNode>();
                auto v = std::make_shared<VarNode>(); v->name = name;
                idx->array = v;
                consume();
                idx->index = parseExpr();
                expect(TokenType::RBRACKET);
                return idx;
            }
            // Function call
            if (check(TokenType::LPAREN)) {
                auto call = std::make_shared<CallNode>();
                call->name = name;
                consume();
                if (!check(TokenType::RPAREN)) {
                    call->args.push_back(parseExpr());
                    while (match(TokenType::COMMA)) call->args.push_back(parseExpr());
                }
                expect(TokenType::RPAREN);
                return call;
            }
            auto v = std::make_shared<VarNode>(); v->name = name; return v;
        }
        throw std::runtime_error("Unexpected token '" + peek().value + "' at line " + std::to_string(peek().line));
    }
};

// ─────────────────────────────────────────────
//  Runtime Value
// ─────────────────────────────────────────────
struct Value {
    enum class Type { NIL, INT, REAL, BOOL, STRING, CHAR, ARRAY } vtype = Type::NIL;
    long long   ival = 0;
    double      rval = 0;
    bool        bval = false;
    std::string sval;
    char        cval = 0;
    std::vector<Value> arr;

    static Value from_int(long long v)    { Value x; x.vtype=Type::INT;    x.ival=v; return x; }
    static Value from_real(double v)      { Value x; x.vtype=Type::REAL;   x.rval=v; return x; }
    static Value from_bool(bool v)        { Value x; x.vtype=Type::BOOL;   x.bval=v; return x; }
    static Value from_string(const std::string& v) { Value x; x.vtype=Type::STRING; x.sval=v; return x; }
    static Value from_char(char v)        { Value x; x.vtype=Type::CHAR;   x.cval=v; return x; }
    static Value make_array(int lo, int hi) {
        Value x; x.vtype=Type::ARRAY;
        x.arr.resize(hi - lo + 1);
        x.ival = lo; // store lo in ival
        return x;
    }

    double as_real() const {
        if (vtype == Type::INT)  return (double)ival;
        if (vtype == Type::REAL) return rval;
        throw std::runtime_error("Not a number");
    }
    long long as_int() const {
        if (vtype == Type::INT)  return ival;
        if (vtype == Type::REAL) return (long long)rval;
        throw std::runtime_error("Not an integer");
    }
    bool as_bool() const {
        if (vtype == Type::BOOL) return bval;
        throw std::runtime_error("Not a boolean");
    }
    std::string as_string() const {
        if (vtype == Type::STRING) return sval;
        if (vtype == Type::CHAR)   return std::string(1, cval);
        throw std::runtime_error("Not a string");
    }
    std::string to_string() const {
        switch (vtype) {
            case Type::INT:    return std::to_string(ival);
            case Type::REAL: {
                // Format nicely
                std::ostringstream oss;
                oss << rval;
                return oss.str();
            }
            case Type::BOOL:   return bval ? "TRUE" : "FALSE";
            case Type::STRING: return sval;
            case Type::CHAR:   return std::string(1, cval);
            default: return "<nil>";
        }
    }
    bool is_numeric() const { return vtype == Type::INT || vtype == Type::REAL; }
};

// ─────────────────────────────────────────────
//  Environment (scope)
// ─────────────────────────────────────────────
struct Environment {
    std::unordered_map<std::string, Value> vars;
    std::shared_ptr<Environment> parent;
    std::unordered_map<std::string, std::string> var_types; // for array bounds etc.

    Environment(std::shared_ptr<Environment> p = nullptr) : parent(p) {}

    Value& get_ref(const std::string& name) {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        if (parent) return parent->get_ref(name);
        throw std::runtime_error("Undefined variable: " + name);
    }
    Value get(const std::string& name) { return get_ref(name); }
    void set(const std::string& name, const Value& v) {
        auto it = vars.find(name);
        if (it != vars.end()) { it->second = v; return; }
        if (parent) {
            try { parent->set(name, v); return; } catch (...) {}
        }
        vars[name] = v;
    }
    void set_local(const std::string& name, const Value& v) { vars[name] = v; }
    bool has_local(const std::string& name) { return vars.count(name) > 0; }
};

#include "pascal_graphics.h"

// ─────────────────────────────────────────────
//  Interpreter
// ─────────────────────────────────────────────
struct ReturnException { Value val; };

class Interpreter {
    std::unordered_map<std::string, std::shared_ptr<ProcDef>> procs;
    std::shared_ptr<Environment> global_env;

    // Evaluate an expression
    Value eval(const ASTPtr& node, std::shared_ptr<Environment> env) {
        if (!node) return Value{};

        if (auto* n = dynamic_cast<NumberNode*>(node.get())) {
            return n->is_int ? Value::from_int((long long)n->value) : Value::from_real(n->value);
        }
        if (auto* n = dynamic_cast<BoolNode*>(node.get()))   return Value::from_bool(n->value);
        if (auto* n = dynamic_cast<StringNode*>(node.get())) return Value::from_string(n->value);
        if (auto* n = dynamic_cast<CharNode*>(node.get()))   return Value::from_char(n->value);

        if (auto* n = dynamic_cast<VarNode*>(node.get())) {
            std::string lo = n->name;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            // Zero-argument graphics functions arrive as VarNodes (no parentheses)
            // Try graphics dispatch first for known zero-arg names
            {
                auto [handled, gval] = evalGraphCall(lo, {},
                    [&](const ASTPtr& n2) { return eval(n2, env); });
                if (handled) return gval;
            }
            return env->get(lo);
        }

        if (auto* n = dynamic_cast<IndexNode*>(node.get())) {
            Value arr = eval(n->array, env);
            Value idx = eval(n->index, env);
            if (arr.vtype != Value::Type::ARRAY)
                throw std::runtime_error("Indexing non-array");
            int i = (int)idx.as_int() - (int)arr.ival;
            if (i < 0 || i >= (int)arr.arr.size())
                throw std::runtime_error("Array index out of bounds");
            return arr.arr[i];
        }

        if (auto* n = dynamic_cast<UnaryOpNode*>(node.get())) {
            Value v = eval(n->operand, env);
            if (n->op == "-") {
                if (v.vtype == Value::Type::INT)  return Value::from_int(-v.ival);
                if (v.vtype == Value::Type::REAL) return Value::from_real(-v.rval);
            }
            if (n->op == "not") return Value::from_bool(!v.as_bool());
            throw std::runtime_error("Unknown unary op: " + n->op);
        }

        if (auto* n = dynamic_cast<BinOpNode*>(node.get())) {
            return evalBinOp(n->op, n->left, n->right, env);
        }

        if (auto* n = dynamic_cast<CallNode*>(node.get())) {
            return evalCall(n->name, n->args, env);
        }

        throw std::runtime_error("Cannot evaluate node");
    }

    Value evalBinOp(const std::string& op, const ASTPtr& lhs, const ASTPtr& rhs,
                    std::shared_ptr<Environment> env) {
        // Short-circuit for boolean ops
        if (op == "and") {
            Value l = eval(lhs, env);
            if (!l.as_bool()) return Value::from_bool(false);
            return Value::from_bool(eval(rhs, env).as_bool());
        }
        if (op == "or") {
            Value l = eval(lhs, env);
            if (l.as_bool()) return Value::from_bool(true);
            return Value::from_bool(eval(rhs, env).as_bool());
        }

        Value l = eval(lhs, env);
        Value r = eval(rhs, env);

        // String concatenation
        if (op == "+" && (l.vtype == Value::Type::STRING || l.vtype == Value::Type::CHAR)) {
            return Value::from_string(l.to_string() + r.to_string());
        }

        // Arithmetic
        if (op == "+" || op == "-" || op == "*") {
            bool both_int = l.vtype == Value::Type::INT && r.vtype == Value::Type::INT;
            if (both_int) {
                if (op == "+") return Value::from_int(l.ival + r.ival);
                if (op == "-") return Value::from_int(l.ival - r.ival);
                if (op == "*") return Value::from_int(l.ival * r.ival);
            }
            double a = l.as_real(), b = r.as_real();
            if (op == "+") return Value::from_real(a + b);
            if (op == "-") return Value::from_real(a - b);
            if (op == "*") return Value::from_real(a * b);
        }
        if (op == "/") {
            double a = l.as_real(), b = r.as_real();
            if (b == 0) throw std::runtime_error("Division by zero");
            return Value::from_real(a / b);
        }
        if (op == "div") {
            long long b = r.as_int();
            if (b == 0) throw std::runtime_error("Division by zero");
            return Value::from_int(l.as_int() / b);
        }
        if (op == "mod") {
            long long b = r.as_int();
            if (b == 0) throw std::runtime_error("Division by zero");
            return Value::from_int(l.as_int() % b);
        }
        if (op == "xor") return Value::from_bool(l.as_bool() != r.as_bool());

        // Comparisons
        auto cmp = [&]() -> int {
            if (l.vtype == Value::Type::STRING || l.vtype == Value::Type::CHAR ||
                r.vtype == Value::Type::STRING || r.vtype == Value::Type::CHAR) {
                return l.to_string().compare(r.to_string());
            }
            if (l.vtype == Value::Type::BOOL && r.vtype == Value::Type::BOOL) {
                return (int)l.bval - (int)r.bval;
            }
            double a = l.as_real(), b = r.as_real();
            return (a < b) ? -1 : (a > b) ? 1 : 0;
        };
        if (op == "=")  return Value::from_bool(cmp() == 0);
        if (op == "<>") return Value::from_bool(cmp() != 0);
        if (op == "<")  return Value::from_bool(cmp() <  0);
        if (op == "<=") return Value::from_bool(cmp() <= 0);
        if (op == ">")  return Value::from_bool(cmp() >  0);
        if (op == ">=") return Value::from_bool(cmp() >= 0);

        throw std::runtime_error("Unknown operator: " + op);
    }

    Value evalCall(const std::string& raw_name, const std::vector<ASTPtr>& arg_nodes,
                   std::shared_ptr<Environment> env) {
        std::string name = raw_name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        // ── Built-in functions ──
        if (name == "sqr")  { auto v = eval(arg_nodes[0], env); return v.vtype==Value::Type::INT ? Value::from_int(v.ival*v.ival) : Value::from_real(v.rval*v.rval); }
        if (name == "sqrt") { return Value::from_real(std::sqrt(eval(arg_nodes[0],env).as_real())); }
        if (name == "abs")  { auto v = eval(arg_nodes[0],env); return v.vtype==Value::Type::INT ? Value::from_int(std::abs(v.ival)) : Value::from_real(std::abs(v.rval)); }
        if (name == "odd")  { return Value::from_bool(eval(arg_nodes[0],env).as_int() % 2 != 0); }
        if (name == "trunc"){ return Value::from_int((long long)eval(arg_nodes[0],env).as_real()); }
        if (name == "round"){ return Value::from_int((long long)std::round(eval(arg_nodes[0],env).as_real())); }
        if (name == "ord")  { auto v=eval(arg_nodes[0],env); return v.vtype==Value::Type::CHAR ? Value::from_int(v.cval) : Value::from_int(v.as_int()); }
        if (name == "chr")  { return Value::from_char((char)eval(arg_nodes[0],env).as_int()); }
        if (name == "succ") { return Value::from_int(eval(arg_nodes[0],env).as_int()+1); }
        if (name == "pred") { return Value::from_int(eval(arg_nodes[0],env).as_int()-1); }
        if (name == "length") { return Value::from_int(eval(arg_nodes[0],env).as_string().size()); }
        if (name == "upcase") {
            auto v = eval(arg_nodes[0],env);
            if (v.vtype==Value::Type::CHAR) return Value::from_char(std::toupper(v.cval));
            std::string s=v.as_string(); std::transform(s.begin(),s.end(),s.begin(),::toupper); return Value::from_string(s);
        }
        if (name == "lowercase") {
            auto v = eval(arg_nodes[0],env);
            if (v.vtype==Value::Type::CHAR) return Value::from_char(std::tolower(v.cval));
            std::string s=v.as_string(); std::transform(s.begin(),s.end(),s.begin(),::tolower); return Value::from_string(s);
        }
        if (name == "copy") {
            std::string s = eval(arg_nodes[0],env).as_string();
            int start = (int)eval(arg_nodes[1],env).as_int() - 1;
            int len   = (int)eval(arg_nodes[2],env).as_int();
            if (start < 0) start = 0;
            return Value::from_string(s.substr(start, len));
        }
        if (name == "pos") {
            std::string needle = eval(arg_nodes[0],env).as_string();
            std::string hay    = eval(arg_nodes[1],env).as_string();
            auto p = hay.find(needle);
            return Value::from_int(p == std::string::npos ? 0 : (long long)p+1);
        }
        if (name == "concat") {
            std::string res;
            for (auto& a : arg_nodes) res += eval(a,env).as_string();
            return Value::from_string(res);
        }
        if (name == "random") {
            if (arg_nodes.empty()) return Value::from_real((double)rand() / RAND_MAX);
            long long n = eval(arg_nodes[0], env).as_int();
            return Value::from_int(n > 0 ? rand() % n : 0);
        }
        if (name == "randomize") { srand((unsigned)time(nullptr)); return Value{}; }
        if (name == "sin")  return Value::from_real(std::sin(eval(arg_nodes[0],env).as_real()));
        if (name == "cos")  return Value::from_real(std::cos(eval(arg_nodes[0],env).as_real()));
        if (name == "exp")  return Value::from_real(std::exp(eval(arg_nodes[0],env).as_real()));
        if (name == "ln")   return Value::from_real(std::log(eval(arg_nodes[0],env).as_real()));
        if (name == "int")  return Value::from_int((long long)eval(arg_nodes[0],env).as_real());
        if (name == "frac") { double v=eval(arg_nodes[0],env).as_real(); return Value::from_real(v - (long long)v); }
        if (name == "pi")   return Value::from_real(3.14159265358979323846);
        if (name == "max")  { auto a=eval(arg_nodes[0],env), b=eval(arg_nodes[1],env); return a.as_real()>=b.as_real()?a:b; }
        if (name == "min")  { auto a=eval(arg_nodes[0],env), b=eval(arg_nodes[1],env); return a.as_real()<=b.as_real()?a:b; }
        if (name == "str") {
            auto v = eval(arg_nodes[0], env);
            // str(value, var) — assign string form to second arg
            std::string s = v.to_string();
            if (arg_nodes.size() > 1) {
                if (auto* vn = dynamic_cast<VarNode*>(arg_nodes[1].get())) {
                    std::string lo = vn->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                    env->set(lo, Value::from_string(s));
                }
            }
            return Value::from_string(s);
        }
        if (name == "val") {
            std::string s = eval(arg_nodes[0],env).as_string();
            if (auto* vn = dynamic_cast<VarNode*>(arg_nodes[1].get())) {
                std::string lo = vn->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                try { env->set(lo, Value::from_int(std::stoll(s))); }
                catch (...) { env->set(lo, Value::from_real(std::stod(s))); }
            }
            return Value{};
        }

        // ── Graphics built-ins (SDL2, optional) ──
        {
            auto [handled, gval] = evalGraphCall(name, arg_nodes,
                [&](const ASTPtr& n) { return eval(n, env); });
            if (handled) return gval;
        }

        // ── User-defined procedure/function ──
        auto it = procs.find(name);
        if (it == procs.end())
            throw std::runtime_error("Undefined procedure/function: " + name);

        auto& pd = it->second;
        auto call_env = std::make_shared<Environment>(global_env);

        // bind parameters
        for (size_t i = 0; i < pd->params.size(); i++) {
            std::string pname = pd->params[i].name;
            std::transform(pname.begin(), pname.end(), pname.begin(), ::tolower);
            Value arg_val = i < arg_nodes.size() ? eval(arg_nodes[i], env) : Value{};
            call_env->set_local(pname, arg_val);
        }

        // result variable for functions
        if (pd->is_function) {
            std::string rname = name;
            call_env->set_local(rname, Value{});
        }

        // process local declarations
        processDeclarations(pd->decls, call_env);

        try {
            exec(pd->body, call_env);
        } catch (ReturnException&) {}

        if (pd->is_function) {
            return call_env->get(name);
        }

        // propagate var (by-ref) parameters back
        for (size_t i = 0; i < pd->params.size(); i++) {
            if (pd->params[i].by_ref && i < arg_nodes.size()) {
                if (auto* vn = dynamic_cast<VarNode*>(arg_nodes[i].get())) {
                    std::string lo = vn->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                    std::string plo = pd->params[i].name; std::transform(plo.begin(),plo.end(),plo.begin(),::tolower);
                    env->set(lo, call_env->get(plo));
                }
            }
        }

        return Value{};
    }

    // Execute a statement
    void exec(const ASTPtr& node, std::shared_ptr<Environment> env) {
        if (!node) return;

        if (auto* n = dynamic_cast<CompoundNode*>(node.get())) {
            for (auto& s : n->statements) exec(s, env);
            return;
        }
        if (auto* n = dynamic_cast<AssignNode*>(node.get())) {
            Value val = eval(n->value, env);
            if (auto* idx = dynamic_cast<IndexNode*>(n->target.get())) {
                // array element assignment
                auto* vn = dynamic_cast<VarNode*>(idx->array.get());
                if (!vn) throw std::runtime_error("Invalid array target");
                std::string lo = vn->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                Value& arr = env->get_ref(lo);
                if (arr.vtype != Value::Type::ARRAY) throw std::runtime_error("Not an array: " + lo);
                int i = (int)eval(idx->index, env).as_int() - (int)arr.ival;
                if (i < 0 || i >= (int)arr.arr.size()) throw std::runtime_error("Array index out of bounds");
                arr.arr[i] = val;
            } else if (auto* vn = dynamic_cast<VarNode*>(n->target.get())) {
                std::string lo = vn->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                env->set(lo, val);
            }
            return;
        }
        if (auto* n = dynamic_cast<IfNode*>(node.get())) {
            if (eval(n->cond, env).as_bool())
                exec(n->then_branch, env);
            else if (n->else_branch)
                exec(n->else_branch, env);
            return;
        }
        if (auto* n = dynamic_cast<WhileNode*>(node.get())) {
            while (eval(n->cond, env).as_bool()) exec(n->body, env);
            return;
        }
        if (auto* n = dynamic_cast<ForNode*>(node.get())) {
            std::string lo = n->var; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
            long long from_v = eval(n->from, env).as_int();
            long long to_v   = eval(n->to,   env).as_int();
            if (!n->downto) {
                for (long long i = from_v; i <= to_v; i++) {
                    env->set(lo, Value::from_int(i));
                    exec(n->body, env);
                }
            } else {
                for (long long i = from_v; i >= to_v; i--) {
                    env->set(lo, Value::from_int(i));
                    exec(n->body, env);
                }
            }
            return;
        }
        if (auto* n = dynamic_cast<RepeatNode*>(node.get())) {
            do { exec(n->body, env); } while (!eval(n->cond, env).as_bool());
            return;
        }
        if (auto* n = dynamic_cast<WriteNode*>(node.get())) {
            for (auto& a : n->args) std::cout << eval(a, env).to_string();
            if (n->newline) std::cout << "\n";
            std::cout.flush();
            return;
        }
        if (auto* n = dynamic_cast<ReadNode*>(node.get())) {
            for (auto& var : n->vars) {
                if (auto* vn = dynamic_cast<VarNode*>(var.get())) {
                    std::string lo = vn->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                    Value& target = env->get_ref(lo);
                    if (target.vtype == Value::Type::INT) {
                        long long v; std::cin >> v; target = Value::from_int(v);
                    } else if (target.vtype == Value::Type::REAL) {
                        double v; std::cin >> v; target = Value::from_real(v);
                    } else if (target.vtype == Value::Type::STRING) {
                        std::string v; std::cin >> v; target = Value::from_string(v);
                    } else if (target.vtype == Value::Type::CHAR) {
                        char v; std::cin >> v; target = Value::from_char(v);
                    } else {
                        std::string v; std::cin >> v;
                        try { target = Value::from_int(std::stoll(v)); }
                        catch (...) { target = Value::from_string(v); }
                    }
                }
            }
            if (n->newline) {
                std::string dummy; std::getline(std::cin, dummy);
            }
            return;
        }
        if (auto* n = dynamic_cast<CallNode*>(node.get())) {
            evalCall(n->name, n->args, env);
            return;
        }
        // Ignore unknown node types (e.g. empty compound)
    }

    void processDeclarations(const std::vector<ASTPtr>& decls, std::shared_ptr<Environment> env) {
        for (auto& d : decls) {
            if (auto* vd = dynamic_cast<VarDecl*>(d.get())) {
                for (auto& nm : vd->names) {
                    std::string lo = nm; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                    if (vd->is_array) {
                        env->set_local(lo, Value::make_array(vd->array_lo, vd->array_hi));
                    } else {
                        std::string type = vd->type_name;
                        std::transform(type.begin(),type.end(),type.begin(),::tolower);
                        Value v;
                        if (type=="integer") v = Value::from_int(0);
                        else if (type=="real") v = Value::from_real(0.0);
                        else if (type=="boolean") v = Value::from_bool(false);
                        else if (type=="string") v = Value::from_string("");
                        else if (type=="char") v = Value::from_char('\0');
                        env->set_local(lo, v);
                    }
                }
            } else if (auto* cd = dynamic_cast<ConstDecl*>(d.get())) {
                std::string lo = cd->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                env->set_local(lo, eval(cd->value, env));
            } else if (auto* pd = dynamic_cast<ProcDef*>(d.get())) {
                std::string lo = pd->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                procs[lo] = std::shared_ptr<ProcDef>(pd, [](ProcDef*){});
                // Store raw pointer wrapped — safe because pd lives in the AST
                // Actually we need a proper shared_ptr
                procs[lo] = std::make_shared<ProcDef>(*pd);
            }
        }
    }

public:
    void run(const ASTPtr& ast) {
        auto* prog = dynamic_cast<ProgramNode*>(ast.get());
        if (!prog) throw std::runtime_error("Not a program node");

        global_env = std::make_shared<Environment>();
        processDeclarations(prog->declarations, global_env);
        exec(prog->body, global_env);
    }
};

// ─────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    std::string source;

    if (argc >= 2) {
        std::ifstream f(argv[1]);
        if (!f) { std::cerr << "Cannot open file: " << argv[1] << "\n"; return 1; }
        source = std::string(std::istreambuf_iterator<char>(f), {});
    } else {
        // REPL / stdin mode
        std::cout << "Pascal Interpreter - reading from stdin...\n";
        source = std::string(std::istreambuf_iterator<char>(std::cin), {});
    }

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        Parser parser(std::move(tokens));
        auto ast = parser.parse();

        Interpreter interp;
        interp.run(ast);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
