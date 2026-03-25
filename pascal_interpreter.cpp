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
#include <iomanip>
#include <set>
#include <map>
#include <cstdarg>

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
        // Variable not found anywhere — throw a clear error
        // (was: silently create, which caused mysterious <nil> return values)
        throw std::runtime_error("Undefined variable: " + name);
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
static bool compilePascal(const std::string&, const std::string&, std::string&);

int main(int argc, char* argv[]) {
    // Usage:
    //   pascal file.pas              — interpret
    //   pascal --compile file.pas    — compile to file (no extension) via gcc
    //   pascal -c file.pas -o out    — compile to named output
    bool doCompile = false;
    std::string inputFile, outputFile;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--compile" || arg == "-c") { doCompile = true; }
        else if (arg == "-o" && i+1 < argc)  { outputFile = argv[++i]; }
        else                                   { inputFile = arg; }
    }

    std::string source;
    if (!inputFile.empty()) {
        std::ifstream f(inputFile);
        if (!f) { std::cerr << "Cannot open file: " << inputFile << "\n"; return 1; }
        source = std::string(std::istreambuf_iterator<char>(f), {});
    } else if (!doCompile) {
        std::cout << "Pascal Interpreter - reading from stdin...\n";
        source = std::string(std::istreambuf_iterator<char>(std::cin), {});
    } else {
        std::cerr << "Usage: pascal [--compile [-o output]] file.pas\n";
        return 1;
    }

    if (doCompile) {
        // Derive output name from input filename if not specified
        if (outputFile.empty()) {
            outputFile = inputFile;
            // Strip extension
            auto dot = outputFile.rfind('.');
            if (dot != std::string::npos) outputFile = outputFile.substr(0, dot);
        }
        std::string errMsg;
        std::cerr << "Compiling " << inputFile << " -> " << outputFile << " ...\n";
        try {
            if (compilePascal(source, outputFile, errMsg)) {
                std::cerr << "OK\n";
                return 0;
            } else {
                std::cerr << "Compilation failed:\n" << errMsg << "\n";
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    } else {
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
}

// ═════════════════════════════════════════════
//  Pascal → C Compiler
//  Walks the same AST, emits C99 source,
//  then invokes gcc to produce a native binary.
// ═════════════════════════════════════════════

class Compiler {
    std::ostringstream out;   // generated C source
    int indent = 0;
    int tmpCount = 0;
    std::string currentFunc; // name of function being compiled (for return var)
    std::set<std::string> userProcs; // user-defined proc/func names (lower-case)
    std::map<std::string, std::string> varTypes; // varname(lower) -> pascal type

    // ── Type helpers ──────────────────────────
    std::string cType(const std::string& pas) {
        std::string t = pas;
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        if (t == "integer" || t == "longint" || t == "word" || t == "byte") return "long long";
        if (t == "real" || t == "double" || t == "single")                   return "double";
        if (t == "boolean")                                                    return "int";
        if (t == "char")                                                        return "char";
        if (t == "string")                                                      return "char*";
        return "long long"; // default
    }

    std::string safeId(const std::string& name) {
        // Prefix pascal identifiers to avoid clashing with C keywords/builtins
        std::string lo = name;
        std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
        // C keywords that might clash
        static const std::set<std::string> ckw = {
            "int","long","short","char","float","double","void","return","if","else",
            "while","for","do","switch","case","break","continue","goto","struct",
            "union","enum","typedef","extern","static","auto","register","const",
            "volatile","sizeof","default","printf","scanf","exit","abs","sin","cos",
            "exp","log","sqrt","rand","time","true","false","and","or","not","div"
        };
        if (ckw.count(lo)) return "pas_" + lo;
        return "pas_" + lo;  // always prefix to be safe
    }

    std::string ind() { return std::string(indent * 4, ' '); }

    void emit(const std::string& s) { out << s; }
    void emitln(const std::string& s = "") { out << s << "\n"; }
    void emitind(const std::string& s) { out << ind() << s << "\n"; }

    // ── Expression codegen ────────────────────

    // Returns true if this expression is real-valued (real var, real literal,
    // function returning real, or binary op with a real operand)
    bool isRealExpr(const ASTPtr& node) {
        if (!node) return false;
        if (auto* n = dynamic_cast<NumberNode*>(node.get())) return !n->is_int;
        if (auto* n = dynamic_cast<VarNode*>(node.get())) {
            std::string lo = n->name;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            auto it = varTypes.find(lo);
            if (it != varTypes.end()) {
                std::string t = it->second;
                std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                return t == "real" || t == "double" || t == "single";
            }
            return false;
        }
        if (auto* n = dynamic_cast<BinOpNode*>(node.get())) {
            if (n->op == "/") return true;
            return isRealExpr(n->left) || isRealExpr(n->right);
        }
        if (auto* n = dynamic_cast<UnaryOpNode*>(node.get()))
            return isRealExpr(n->operand);
        if (auto* n = dynamic_cast<CallNode*>(node.get())) {
            // Check if function is declared as returning real
            std::string lo = n->name;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            // math functions always return real
            static const std::set<std::string> realFns = {
                "sqrt","sin","cos","exp","ln","log","pi","frac","random",
                "arctan","arcsin","arccos","tan","int",
                "power" // common user function name
            };
            if (realFns.count(lo)) return true;
            // Check user proc return type
            auto it = varTypes.find(lo + "__ret");
            if (it != varTypes.end()) {
                std::string t = it->second;
                std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                return t == "real" || t == "double" || t == "single";
            }
        }
        return false;
    }

    std::string genExpr(const ASTPtr& node) {
        if (auto* n = dynamic_cast<NumberNode*>(node.get())) {
            if (n->is_int) return std::to_string((long long)n->value) + "LL";
            // format real without losing precision
            std::ostringstream ss; ss << std::setprecision(17) << n->value;
            std::string s = ss.str();
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
                s += ".0";
            return s;
        }
        if (auto* n = dynamic_cast<BoolNode*>(node.get()))
            return n->value ? "1" : "0";
        if (auto* n = dynamic_cast<CharNode*>(node.get())) {
            std::string s = "'"; 
            if (n->value == '\'') s += "\\'";
            else if (n->value == '\\') s += "\\\\";
            else s += n->value;
            s += "'";
            return s;
        }
        if (auto* n = dynamic_cast<StringNode*>(node.get())) {
            // String literals: generate a temp char array
            std::string escaped;
            for (char c : n->value) {
                if (c == '"')  escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else escaped += c;
            }
            return "\"" + escaped + "\"";
        }
        if (auto* n = dynamic_cast<VarNode*>(node.get())) {
            std::string lo = n->name;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            if (!currentFunc.empty()) {
                std::string cf = currentFunc;
                std::transform(cf.begin(), cf.end(), cf.begin(), ::tolower);
                if (lo == cf) return "_ret_";
            }
            // Zero-argument graphics functions arrive as VarNodes — emit as calls
            static const std::set<std::string> zeroArgGfx = {
                "graphresult","getmaxx","getmaxy","getx","gety",
                "getcolor","getbkcolor","readkey"
            };
            if (zeroArgGfx.count(lo))
                return genCall(lo, {}, true);
            return safeId(n->name);
        }
        if (auto* n = dynamic_cast<IndexNode*>(node.get())) {
            auto* vn = dynamic_cast<VarNode*>(n->array.get());
            if (!vn) return "/*bad index*/0";
            std::string base = safeId(vn->name);
            std::string idx  = genExpr(n->index);
            return base + "[" + idx + " - " + base + "_lo]";
        }
        if (auto* n = dynamic_cast<UnaryOpNode*>(node.get())) {
            if (n->op == "-") return "(-(" + genExpr(n->operand) + "))";
            if (n->op == "not") return "(!(" + genExpr(n->operand) + "))";
        }
        if (auto* n = dynamic_cast<BinOpNode*>(node.get())) {
            std::string l = genExpr(n->left), r = genExpr(n->right);
            if (n->op == "+")   return "(" + l + " + "  + r + ")";
            if (n->op == "-")   return "(" + l + " - "  + r + ")";
            if (n->op == "*")   return "(" + l + " * "  + r + ")";
            if (n->op == "/")   return "((double)(" + l + ") / (double)(" + r + "))";
            if (n->op == "div") return "((" + l + ") / (" + r + "))";
            if (n->op == "mod") return "((" + l + ") % (" + r + "))";
            if (n->op == "and") return "(" + l + " && " + r + ")";
            if (n->op == "or")  return "(" + l + " || " + r + ")";
            if (n->op == "xor") return "(" + l + " ^ "  + r + ")";
            if (n->op == "=")   return "(" + l + " == " + r + ")";
            if (n->op == "<>")  return "(" + l + " != " + r + ")";
            if (n->op == "<")   return "(" + l + " < "  + r + ")";
            if (n->op == "<=")  return "(" + l + " <= " + r + ")";
            if (n->op == ">")   return "(" + l + " > "  + r + ")";
            if (n->op == ">=")  return "(" + l + " >= " + r + ")";
        }
        if (auto* n = dynamic_cast<CallNode*>(node.get()))
            return genCall(n->name, n->args, true);
        return "0/*?*/";
    }

    // ── Built-in call codegen ─────────────────
    std::string genCall(const std::string& raw, const std::vector<ASTPtr>& args, bool asExpr) {
        std::string name = raw;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        // If the user defined a proc/func with this name, skip built-in/graphics handling
        // and go straight to user-defined call
        if (userProcs.count(name)) {
            std::string call = safeId(raw) + "(";
            for (size_t i = 0; i < args.size(); i++) {
                if (i) call += ", ";
                call += genExpr(args[i]);
            }
            return call + ")";
        }

        auto a = [&](int i) { return genExpr(args[i]); };

        // Math
        if (name == "sqr")    return "( (" + a(0) + ") * (" + a(0) + ") )";
        if (name == "sqrt")   return "sqrt((double)(" + a(0) + "))";
        if (name == "abs")    return "llabs(" + a(0) + ")";
        if (name == "sin")    return "sin((double)(" + a(0) + "))";
        if (name == "cos")    return "cos((double)(" + a(0) + "))";
        if (name == "arctan") return "atan((double)(" + a(0) + "))";
        if (name == "arcsin") return "asin((double)(" + a(0) + "))";
        if (name == "arccos") return "acos((double)(" + a(0) + "))";
        if (name == "tan")    return "tan((double)(" + a(0) + "))";
        if (name == "exp")    return "exp((double)(" + a(0) + "))";
        if (name == "ln")     return "log((double)(" + a(0) + "))";
        if (name == "log")    return "log10((double)(" + a(0) + "))";
        if (name == "pi")     return "3.14159265358979323846";
        if (name == "round")  return "(long long)round((double)(" + a(0) + "))";
        if (name == "trunc")  return "(long long)(" + a(0) + ")";
        if (name == "frac")   { auto v = a(0); return "fmod((double)(" + v + "), 1.0)"; }
        if (name == "int")    return "(long long)(" + a(0) + ")";
        if (name == "odd")    return "((" + a(0) + ") % 2 != 0)";
        if (name == "succ")   return "((" + a(0) + ") + 1)";
        if (name == "pred")   return "((" + a(0) + ") - 1)";
        if (name == "max")    { auto l=a(0),r=a(1); return "((" + l + ") >= (" + r + ") ? (" + l + ") : (" + r + "))"; }
        if (name == "min")    { auto l=a(0),r=a(1); return "((" + l + ") <= (" + r + ") ? (" + l + ") : (" + r + "))"; }
        if (name == "ord")    return "(long long)(" + a(0) + ")";
        if (name == "chr")    return "(char)(" + a(0) + ")";
        if (name == "random") {
            if (args.empty()) return "((double)rand() / RAND_MAX)";
            return "(rand() % (int)(" + a(0) + "))";
        }
        if (name == "randomize") return asExpr ? "(srand(time(NULL)),0)" : "srand(time(NULL))";

        // String
        if (name == "length")    return "(long long)strlen(" + a(0) + ")";
        if (name == "upcase")    return "toupper((unsigned char)(" + a(0) + "))";
        if (name == "lowercase") return "tolower((unsigned char)(" + a(0) + "))";
        if (name == "pos") {
            return "(strstr(" + a(1) + ", " + a(0) + ") ? "
                   "(long long)(strstr(" + a(1) + ", " + a(0) + ") - (" + a(1) + ")) + 1 : 0LL)";
        }
        if (name == "copy") {
            // Emit as a runtime helper call
            return "pas_copy_str(" + a(0) + ", " + a(1) + ", " + a(2) + ")";
        }
        if (name == "concat") {
            std::string s = "pas_concat_str(" + std::to_string(args.size());
            for (auto& arg : args) s += ", " + genExpr(arg);
            s += ")";
            return s;
        }

        // I/O — these are statements; when used as expr return 0
        if (name == "halt") return asExpr ? "0" : "exit(0)";

        // Graphics (pass through to SDL if compiled with HAVE_SDL2,
        // otherwise map to no-ops or stubs via the pascal_graphics_c.h shim)
        // For the compiler we emit direct C calls to our graphics shim
        if (name == "initgraph")   return "pas_initgraph()";
        if (name == "closegraph")  return "pas_closegraph()";
        if (name == "graphresult") return "pas_graphresult()";
        if (name == "cleardevice") return "pas_cleardevice()";
        if (name == "getmaxx")     return "pas_getmaxx()";
        if (name == "getmaxy")     return "pas_getmaxy()";
        if (name == "getx")        return "pas_getx()";
        if (name == "gety")        return "pas_gety()";
        if (name == "getcolor")    return "pas_getcolor()";
        if (name == "getbkcolor")  return "pas_getbkcolor()";
        if (name == "readkey")     return "pas_readkey()";
        if (name == "delay")       return "pas_delay(" + a(0) + ")";
        if (name == "putpixel")    return "pas_putpixel(" + a(0) + "," + a(1) + "," + a(2) + ")";
        if (name == "getpixel")    return "pas_getpixel(" + a(0) + "," + a(1) + ")";
        if (name == "line")        return "pas_line(" + a(0)+"," + a(1)+"," + a(2)+"," + a(3) + ")";
        if (name == "lineto")      return "pas_lineto(" + a(0) + "," + a(1) + ")";
        if (name == "linerel")     return "pas_linerel(" + a(0) + "," + a(1) + ")";
        if (name == "moveto")      return "pas_moveto(" + a(0) + "," + a(1) + ")";
        if (name == "moverel")     return "pas_moverel(" + a(0) + "," + a(1) + ")";
        if (name == "rectangle")   return "pas_rectangle(" + a(0)+"," + a(1)+"," + a(2)+"," + a(3) + ")";
        if (name == "bar")         return "pas_bar(" + a(0)+"," + a(1)+"," + a(2)+"," + a(3) + ")";
        if (name == "circle")      return "pas_circle(" + a(0)+"," + a(1)+"," + a(2) + ")";
        if (name == "arc")         return "pas_arc(" + a(0)+"," + a(1)+"," + a(2)+"," + a(3)+"," + a(4) + ")";
        if (name == "ellipse")     return "pas_ellipse(" + a(0)+"," + a(1)+"," + a(2)+"," + a(3)+"," + a(4)+"," + a(5) + ")";
        if (name == "fillellipse") return "pas_fillellipse(" + a(0)+"," + a(1)+"," + a(2)+"," + a(3) + ")";
        if (name == "floodfill")   return "pas_floodfill(" + a(0)+"," + a(1)+"," + a(2) + ")";
        if (name == "setcolor")    return "pas_setcolor(" + a(0) + ")";
        if (name == "setbkcolor")  return "pas_setbkcolor(" + a(0) + ")";
        if (name == "setrgbcolor") return "pas_setrgbcolor(" + a(0)+"," + a(1)+"," + a(2) + ")";
        if (name == "setrgbbkcolor") return "pas_setrgbbkcolor(" + a(0)+"," + a(1)+"," + a(2) + ")";
        if (name == "setfillstyle") return "pas_setfillstyle(" + a(0)+"," + a(1) + ")";
        if (name == "outtextxy")   return "pas_outtextxy(" + a(0)+"," + a(1)+"," + a(2) + ")";
        if (name == "outtext")     return "pas_outtext(" + a(0) + ")";
        if (name == "textwidth")   return "pas_textwidth(" + a(0) + ")";
        if (name == "textheight")  return "pas_textheight(" + a(0) + ")";
        if (name == "grapherrormsg") return "\"Graphics error\"";

        // User-defined call
        std::string call = safeId(name) + "(";
        for (size_t i = 0; i < args.size(); i++) {
            if (i) call += ", ";
            call += genExpr(args[i]);
        }
        call += ")";
        return call;
    }

    // ── Statement codegen ─────────────────────
    void genStmt(const ASTPtr& node) {
        if (!node) return;

        if (auto* n = dynamic_cast<CompoundNode*>(node.get())) {
            for (auto& s : n->statements) genStmt(s);
            return;
        }
        if (auto* n = dynamic_cast<AssignNode*>(node.get())) {
            // Check for array element assignment
            if (auto* idx = dynamic_cast<IndexNode*>(n->target.get())) {
                auto* vn = dynamic_cast<VarNode*>(idx->array.get());
                std::string base = safeId(vn->name);
                std::string i    = genExpr(idx->index);
                std::string val  = genExpr(n->value);
                emitind(base + "[" + i + " - " + base + "_lo] = " + val + ";");
            } else if (auto* vn = dynamic_cast<VarNode*>(n->target.get())) {
                std::string vlo = vn->name;
                std::transform(vlo.begin(), vlo.end(), vlo.begin(), ::tolower);
                std::string cf = currentFunc;
                std::transform(cf.begin(), cf.end(), cf.begin(), ::tolower);
                std::string lhs = (!currentFunc.empty() && vlo == cf) ? "_ret_" : safeId(vn->name);
                emitind(lhs + " = " + genExpr(n->value) + ";");
            }
            return;
        }
        if (auto* n = dynamic_cast<IfNode*>(node.get())) {
            emitind("if (" + genExpr(n->cond) + ") {");
            indent++;
            auto* tb = dynamic_cast<CompoundNode*>(n->then_branch.get());
            if (tb) { for (auto& s : tb->statements) genStmt(s); }
            else genStmt(n->then_branch);
            indent--;
            if (n->else_branch) {
                emitind("} else {");
                indent++;
                auto* eb = dynamic_cast<CompoundNode*>(n->else_branch.get());
                if (eb) { for (auto& s : eb->statements) genStmt(s); }
                else genStmt(n->else_branch);
                indent--;
            }
            emitind("}");
            return;
        }
        if (auto* n = dynamic_cast<WhileNode*>(node.get())) {
            emitind("while (" + genExpr(n->cond) + ") {");
            indent++;
            auto* body = dynamic_cast<CompoundNode*>(n->body.get());
            if (body) { for (auto& s : body->statements) genStmt(s); }
            else genStmt(n->body);
            indent--;
            emitind("}");
            return;
        }
        if (auto* n = dynamic_cast<ForNode*>(node.get())) {
            std::string v    = safeId(n->var);
            std::string from = genExpr(n->from);
            std::string to   = genExpr(n->to);
            std::string op   = n->downto ? ">=" : "<=";
            std::string inc  = n->downto ? "--" : "++";
            emitind("for (" + v + " = " + from + "; " + v + " " + op + " " + to + "; " + v + inc + ") {");
            indent++;
            auto* body = dynamic_cast<CompoundNode*>(n->body.get());
            if (body) { for (auto& s : body->statements) genStmt(s); }
            else genStmt(n->body);
            indent--;
            emitind("}");
            return;
        }
        if (auto* n = dynamic_cast<RepeatNode*>(node.get())) {
            emitind("do {");
            indent++;
            auto* body = dynamic_cast<CompoundNode*>(n->body.get());
            if (body) { for (auto& s : body->statements) genStmt(s); }
            else genStmt(n->body);
            indent--;
            emitind("} while (!(" + genExpr(n->cond) + "));");
            return;
        }
        if (auto* n = dynamic_cast<WriteNode*>(node.get())) {
            for (auto& arg : n->args) {
                std::string e = genExpr(arg);
                if (dynamic_cast<StringNode*>(arg.get()))
                    emitind("pas_print_s(" + e + ");");
                else if (dynamic_cast<CharNode*>(arg.get()))
                    emitind("pas_print_c(" + e + ");");
                else if (auto* num = dynamic_cast<NumberNode*>(arg.get())) {
                    if (num->is_int)
                        emitind("pas_print_ll((long long)(" + e + "));");
                    else
                        emitind("pas_print_d((double)(" + e + "));");
                } else if (auto* vn = dynamic_cast<VarNode*>(arg.get())) {
                    // Variable — look up type and use correct print function
                    std::string lo = vn->name;
                    std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                    auto it = varTypes.find(lo);
                    std::string vtype = (it != varTypes.end()) ? it->second : "";
                    std::transform(vtype.begin(), vtype.end(), vtype.begin(), ::tolower);
                    if (vtype == "real" || vtype == "double" || vtype == "single")
                        emitind("pas_print_d((double)(" + e + "));");
                    else if (vtype == "char")
                        emitind("pas_print_c((char)(" + e + "));");
                    else if (vtype == "string")
                        emitind("pas_print_s((char*)(" + e + "));");
                    else
                        emitind("pas_print_ll((long long)(" + e + "));");
                } else {
                    // BinOp, CallNode, or other expression
                    // Use isRealExpr to decide format
                    if (isRealExpr(arg))
                        emitind("pas_print_d((double)(" + e + "));");
                    else
                        emitind("pas_print_ll((long long)(" + e + "));");
                }
            }
            if (n->newline) emitind("printf(\"\\n\");");
            return;
        }
        if (auto* n = dynamic_cast<ReadNode*>(node.get())) {
            for (auto& var : n->vars) {
                if (auto* vn = dynamic_cast<VarNode*>(var.get())) {
                    std::string lo = vn->name;
                    std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                    auto it = varTypes.find(lo);
                    std::string vtype = (it != varTypes.end()) ? it->second : "";
                    std::transform(vtype.begin(), vtype.end(), vtype.begin(), ::tolower);
                    if (vtype == "real" || vtype == "double" || vtype == "single")
                        emitind("scanf(\"%lf\", &" + safeId(vn->name) + ");");
                    else if (vtype == "char")
                        emitind("scanf(\"%c\", &" + safeId(vn->name) + ");");
                    else if (vtype == "string")
                        emitind("scanf(\"%1023s\", " + safeId(vn->name) + ");");
                    else
                        emitind("scanf(\"%lld\", &" + safeId(vn->name) + ");");
                }
            }
            if (n->newline) emitind("{ char _nl[256]; fgets(_nl, sizeof(_nl), stdin); }");
            return;
        }
        if (auto* n = dynamic_cast<CallNode*>(node.get())) {
            std::string name = n->name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name == "randomize") { emitind("srand((unsigned)time(NULL));"); return; }
            if (name == "halt") { emitind("exit(0);"); return; }
            emitind(genCall(n->name, n->args, false) + ";");
            return;
        }
    }

    // ── Declaration codegen ───────────────────
    void genDecl(const ASTPtr& node, bool global) {
        if (auto* vd = dynamic_cast<VarDecl*>(node.get())) {
            for (auto& nm : vd->names) {
                std::string id = safeId(nm);
                // Record type for print dispatch
                std::string lo = nm; std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                varTypes[lo] = vd->type_name;
                if (vd->is_array) {
                    int sz = vd->array_hi - vd->array_lo + 1;
                    emitind(cType(vd->type_name) + " " + id + "[" + std::to_string(sz) + "];");
                    emitind("const int " + id + "_lo = " + std::to_string(vd->array_lo) + ";");
                } else {
                    std::string ct = cType(vd->type_name);
                    if (ct == "char*")
                        emitind("char " + id + "[1024] = \"\";");
                    else
                        emitind(ct + " " + id + " = 0;");
                }
            }
        } else if (auto* cd = dynamic_cast<ConstDecl*>(node.get())) {
            emitind("const long long " + safeId(cd->name) + " = " + genExpr(cd->value) + ";");
        }
    }

    // ── Function/procedure forward declarations ──
    void genForwardDecl(const ASTPtr& node) {
        auto* pd = dynamic_cast<ProcDef*>(node.get());
        if (!pd) return;
        std::string ret = pd->is_function ? cType(pd->return_type) : "void";
        std::string sig = ret + " " + safeId(pd->name) + "(";
        for (size_t i = 0; i < pd->params.size(); i++) {
            if (i) sig += ", ";
            std::string ct = cType(pd->params[i].type);
            if (ct == "char*") ct = "char*";
            if (pd->params[i].by_ref) ct += "*";
            sig += ct + " " + safeId(pd->params[i].name);
        }
        sig += ");";
        emitind(sig);
    }

    // ── Function/procedure full definition ───
    void genProcDef(const ASTPtr& node) {
        auto* pd = dynamic_cast<ProcDef*>(node.get());
        if (!pd) return;

        std::string prevFunc = currentFunc;
        currentFunc = pd->is_function ? pd->name : "";
        // Record parameter types and return type
        for (auto& p : pd->params) {
            std::string lo = p.name; std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            varTypes[lo] = p.type;
        }
        if (pd->is_function) {
            std::string lo = pd->name; std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            varTypes[lo + "__ret"] = pd->return_type;
        }
        std::string ret = pd->is_function ? cType(pd->return_type) : "void";
        std::string sig = ret + " " + safeId(pd->name) + "(";
        for (size_t i = 0; i < pd->params.size(); i++) {
            if (i) sig += ", ";
            std::string ct = cType(pd->params[i].type);
            if (ct == "char*") ct = "char*";
            if (pd->params[i].by_ref) ct += "*";
            sig += ct + " " + safeId(pd->params[i].name);
        }
        sig += ")";
        emitind(sig);
        emitind("{");
        indent++;

        // Return variable for functions — use _ret_ to avoid shadowing function name
        if (pd->is_function) {
            std::string rt = cType(pd->return_type);
            if (rt == "char*")
                emitind("char _ret_[1024] = \"\";");
            else
                emitind(rt + " _ret_ = 0;");
        }

        // Local declarations
        for (auto& d : pd->decls) {
            if (!dynamic_cast<ProcDef*>(d.get()))
                genDecl(d, false);
        }
        // Nested proc/func definitions
        for (auto& d : pd->decls) {
            if (dynamic_cast<ProcDef*>(d.get()))
                genProcDef(d);
        }

        // Body
        if (pd->body) {
            auto* compound = dynamic_cast<CompoundNode*>(pd->body.get());
            if (compound) {
                for (auto& s : compound->statements) genStmt(s);
            } else {
                genStmt(pd->body);
            }
        }

        if (pd->is_function)
            emitind("return _ret_;");

        indent--;
        emitind("}");
        emitln();
        currentFunc = prevFunc;
    }

public:
    std::string compile(const ASTPtr& ast) {
        auto* prog = dynamic_cast<ProgramNode*>(ast.get());
        if (!prog) throw std::runtime_error("Not a program node");

        // Collect user-defined proc/func names to avoid stub conflicts
        std::set<std::string> userProcs;
        {
            std::string pn = prog->name;
            std::transform(pn.begin(), pn.end(), pn.begin(), ::tolower);
            userProcs.insert(pn);
        }
        std::function<void(const std::vector<ASTPtr>&)> collectProcs =
            [&](const std::vector<ASTPtr>& decls) {
                for (auto& d : decls) {
                    if (auto* pd = dynamic_cast<ProcDef*>(d.get())) {
                        std::string n = pd->name;
                        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                        userProcs.insert(n);
                        collectProcs(pd->decls);
                    }
                }
            };
        collectProcs(prog->declarations);
        // Store in member so genCall can access it
        this->userProcs = userProcs;

        // Only emit a stub if the user hasn't defined a proc/func with that name
        auto emitStub = [&](const std::string& gfxName, const std::string& stub) {
            std::string lo = gfxName;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            if (!userProcs.count(lo)) emitln(stub);
        };

        // ── Preamble ──────────────────────────
        emitln("/* Generated by Pascal Compiler */");
        emitln("#include <stdio.h>");
        emitln("#include <stdlib.h>");
        emitln("#include <string.h>");
        emitln("#include <math.h>");
        emitln("#include <time.h>");
        emitln("#include <ctype.h>");
        emitln("#include <stdarg.h>");
        emitln();

        // Graphics — full SDL2 implementation inline, or stubs if no SDL2
        emitln("#ifdef HAVE_SDL2");
        emitln("#include <SDL.h>");
        emitln("#include <math.h>");
        // Emit the full C graphics implementation inline
        emitln("typedef struct { SDL_Window* w; SDL_Renderer* r; SDL_Texture* t;");
        emitln("  int* px; int W; int H; int cx; int cy;");
        emitln("  int fg; int bg; int fill; int gresult; int open; } PasGfx;");
        emitln("static PasGfx _gfx = {0};");
        emitln("static const int _TP_PAL[16] = {");
        emitln("  0xFF000000,0xFF0000AA,0xFF00AA00,0xFF00AAAA,");
        emitln("  0xFFAA0000,0xFFAA00AA,0xFFAA5500,0xFFAAAAAA,");
        emitln("  0xFF555555,0xFF5555FF,0xFF55FF55,0xFF55FFFF,");
        emitln("  0xFFFF5555,0xFFFF55FF,0xFFFFFF55,0xFFFFFFFF};");
        emitln("static const unsigned char _FONT[96][8] = {");
        emitln("{0,0,0,0,0,0,0,0},{24,24,24,24,0,24,0,0},{108,108,0,0,0,0,0,0},{108,108,254,108,254,108,108,0},");
        emitln("{24,126,192,124,6,252,24,0},{0,198,204,24,48,102,198,0},{56,108,56,118,220,204,118,0},");
        emitln("{48,48,96,0,0,0,0,0},{24,48,96,96,96,48,24,0},{96,48,24,24,24,48,96,0},");
        emitln("{0,102,60,255,60,102,0,0},{0,24,24,126,24,24,0,0},{0,0,0,0,0,24,24,48},");
        emitln("{0,0,0,126,0,0,0,0},{0,0,0,0,0,24,24,0},{6,12,24,48,96,192,128,0},");
        emitln("{124,198,206,222,246,230,124,0},{48,112,48,48,48,48,252,0},{120,204,12,56,96,204,252,0},");
        emitln("{120,204,12,56,12,204,120,0},{28,60,108,204,254,12,30,0},{252,192,248,12,12,204,120,0},");
        emitln("{56,96,192,248,204,204,120,0},{252,204,12,24,48,48,48,0},{120,204,204,120,204,204,120,0},");
        emitln("{120,204,204,124,12,24,112,0},{0,24,24,0,24,24,0,0},{0,24,24,0,24,24,48,0},");
        emitln("{24,48,96,192,96,48,24,0},{0,0,126,0,126,0,0,0},{96,48,24,12,24,48,96,0},");
        emitln("{60,102,12,24,24,0,24,0},{124,198,222,222,222,192,120,0},{48,120,204,204,252,204,204,0},");
        emitln("{252,102,102,124,102,102,252,0},{60,102,192,192,192,102,60,0},{248,108,102,102,102,108,248,0},");
        emitln("{254,98,104,120,104,98,254,0},{254,98,104,120,104,96,240,0},{60,102,192,192,206,102,62,0},");
        emitln("{204,204,204,252,204,204,204,0},{120,48,48,48,48,48,120,0},{30,12,12,12,204,204,120,0},");
        emitln("{230,102,108,120,108,102,230,0},{240,96,96,96,98,102,254,0},{198,238,254,254,214,198,198,0},");
        emitln("{198,230,246,222,206,198,198,0},{56,108,198,198,198,108,56,0},{252,102,102,124,96,96,240,0},");
        emitln("{120,204,204,204,220,120,28,0},{252,102,102,124,108,102,230,0},{120,204,224,112,28,204,120,0},");
        emitln("{252,180,48,48,48,48,120,0},{204,204,204,204,204,204,252,0},{204,204,204,204,204,120,48,0},");
        emitln("{198,198,198,214,254,238,198,0},{198,198,108,56,56,108,198,0},{204,204,204,120,48,48,120,0},");
        emitln("{254,198,140,24,50,102,254,0},{120,96,96,96,96,96,120,0},{192,96,48,24,12,6,2,0},");
        emitln("{120,24,24,24,24,24,120,0},{16,56,108,198,0,0,0,0},{0,0,0,0,0,0,254,0},");
        emitln("{48,48,24,0,0,0,0,0},{0,0,120,12,124,204,118,0},{224,96,96,124,102,102,220,0},");
        emitln("{0,0,120,204,192,204,120,0},{28,12,12,124,204,204,118,0},{0,0,120,204,252,192,120,0},");
        emitln("{56,108,96,240,96,96,240,0},{0,0,118,204,204,124,12,248},{224,96,108,118,102,102,230,0},");
        emitln("{48,0,112,48,48,48,120,0},{12,0,12,12,12,204,204,120},{224,96,102,108,120,108,230,0},");
        emitln("{112,48,48,48,48,48,120,0},{0,0,204,254,254,214,198,0},{0,0,248,204,204,204,204,0},");
        emitln("{0,0,120,204,204,204,120,0},{0,0,220,102,102,124,96,240},{0,0,118,204,204,124,12,30},");
        emitln("{0,0,220,118,102,96,240,0},{0,0,124,192,120,12,248,0},{16,48,124,48,48,52,24,0},");
        emitln("{0,0,204,204,204,204,118,0},{0,0,204,204,204,120,48,0},{0,0,198,214,254,254,108,0},");
        emitln("{0,0,198,108,56,108,198,0},{0,0,204,204,204,124,12,248},{0,0,252,152,48,100,252,0},");
        emitln("{28,48,48,224,48,48,28,0},{24,24,24,0,24,24,24,0},{224,48,48,28,48,48,224,0},{118,220,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}};");
        emitln("static void _pump(void){SDL_Event e;while(SDL_PollEvent(&e)){}}");
        emitln("static void _flush(void){");
        emitln("  if(!_gfx.t)return;");
        emitln("  SDL_UpdateTexture(_gfx.t,NULL,_gfx.px,_gfx.W*4);");
        emitln("  SDL_RenderClear(_gfx.r);SDL_RenderCopy(_gfx.r,_gfx.t,NULL,NULL);SDL_RenderPresent(_gfx.r);}");
        emitln("static void _setpx(int x,int y,int c){if(x>=0&&y>=0&&x<_gfx.W&&y<_gfx.H)_gfx.px[y*_gfx.W+x]=c;}");
        emitln("static int _col(long long i){if(i>=0&&i<16)return _TP_PAL[i];");
        emitln("  return (int)(0xFF000000|((unsigned long long)i&0xFFFFFF));}");
        emitln("static void _line(int x1,int y1,int x2,int y2,int c){");
        emitln("  int dx=abs(x2-x1),sx=x1<x2?1:-1,dy=-abs(y2-y1),sy=y1<y2?1:-1,e=dx+dy;");
        emitln("  for(;;){_setpx(x1,y1,c);if(x1==x2&&y1==y2)break;");
        emitln("    int e2=2*e;if(e2>=dy){e+=dy;x1+=sx;}if(e2<=dx){e+=dx;y1+=sy;}}}");
        emitln("static void _drawchar(int x,int y,char ch,int c){");
        emitln("  int idx=(unsigned char)ch-32;if(idx<0||idx>=96)idx=0;");
        emitln("  const unsigned char*g=_FONT[idx];");
        emitln("  for(int r=0;r<8;r++){unsigned char b=g[r];for(int col=0;col<8;col++)if(b&(0x80>>col))_setpx(x+col,y+r,c);}}");
        emitln("static void pas_initgraph(void){");
        emitln("  if(_gfx.open)return;");
        emitln("  SDL_Init(SDL_INIT_VIDEO);");
        emitln("  _gfx.W=640;_gfx.H=480;_gfx.fg=_TP_PAL[15];_gfx.bg=_TP_PAL[0];_gfx.fill=_TP_PAL[15];");
        emitln("  _gfx.w=SDL_CreateWindow(\"Pascal\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,SDL_WINDOW_SHOWN);");
        emitln("  _gfx.r=SDL_CreateRenderer(_gfx.w,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);");
        emitln("  _gfx.t=SDL_CreateTexture(_gfx.r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,640,480);");
        emitln("  _gfx.px=(int*)calloc(640*480,4);");
        emitln("  for(int i=0;i<640*480;i++)_gfx.px[i]=_gfx.bg;");
        emitln("  _gfx.open=1;_gfx.gresult=0;");
        emitln("  for(int i=0;i<10;i++){_pump();_flush();SDL_Delay(10);}}");
        emitln("static void pas_closegraph(void){");
        emitln("  if(!_gfx.open)return;_pump();_flush();SDL_Delay(200);");
        emitln("  SDL_DestroyTexture(_gfx.t);SDL_DestroyRenderer(_gfx.r);SDL_DestroyWindow(_gfx.w);");
        emitln("  SDL_Quit();free(_gfx.px);memset(&_gfx,0,sizeof(_gfx));}");
        emitln("static int  pas_graphresult(void){int r=_gfx.gresult;_gfx.gresult=0;return r;}");
        emitln("static void pas_cleardevice(void){");
        emitln("  if(!_gfx.open)return;for(int i=0;i<_gfx.W*_gfx.H;i++)_gfx.px[i]=_gfx.bg;");
        emitln("  _gfx.cx=_gfx.cy=0;_pump();_flush();_pump();}");
        emitln("static int  pas_getmaxx(void){return _gfx.W-1;}");
        emitln("static int  pas_getmaxy(void){return _gfx.H-1;}");
        emitln("static int  pas_getx(void){return _gfx.cx;}");
        emitln("static int  pas_gety(void){return _gfx.cy;}");
        emitln("static int  pas_getcolor(void){return _gfx.fg;}");
        emitln("static int  pas_getbkcolor(void){return _gfx.bg;}");
        if (!userProcs.count("readkey")) {
            emitln("static char pas_readkey(void){");
            emitln("  if(_gfx.open)_flush();");
            emitln("  SDL_Event e;while(SDL_WaitEvent(&e)){");
            emitln("    if(e.type==SDL_KEYDOWN){SDL_Keycode k=e.key.keysym.sym;");
            emitln("      if(k>=32&&k<127)return(char)k;");
            emitln("      if(k==SDLK_RETURN)return'\\r';if(k==SDLK_ESCAPE)return 27;return 0;}");
            emitln("    if(e.type==SDL_QUIT)return 27;}return 0;}");
        }
               if (!userProcs.count("delay")) {
     emitln("static void pas_delay(long long ms){SDL_Delay((int)ms);_pump();}");
        }

        emitln("static void pas_setcolor(long long c){_gfx.fg=_col(c);}");
        emitln("static void pas_setbkcolor(long long c){_gfx.bg=_col(c);}");
        emitln("static void pas_setrgbcolor(long long r,long long g,long long b){_gfx.fg=0xFF000000|(r<<16)|(g<<8)|b;}");
        emitln("static void pas_setrgbbkcolor(long long r,long long g,long long b){_gfx.bg=0xFF000000|(r<<16)|(g<<8)|b;}");
        emitln("static void pas_setfillstyle(long long p,long long c){(void)p;_gfx.fill=_col(c);}");
        emitln("static void pas_putpixel(long long x,long long y,long long c){");
        emitln("  _setpx((int)x,(int)y,_col(c));");
        emitln("  static int n=0;n++;if(n%1000==0)_flush();if(n%5000==0)_pump();}");
        emitln("static long long pas_getpixel(long long x,long long y){");
        emitln("  if(x<0||y<0||x>=_gfx.W||y>=_gfx.H)return 0;return _gfx.px[(int)y*_gfx.W+(int)x];}");
        if (!userProcs.count("line")) {
            emitln("static void pas_line(long long x1,long long y1,long long x2,long long y2){");
            emitln("  _line((int)x1,(int)y1,(int)x2,(int)y2,_gfx.fg);_flush();}");
        }
        emitln("static void pas_lineto(long long x,long long y){");
        emitln("  _line(_gfx.cx,_gfx.cy,(int)x,(int)y,_gfx.fg);_gfx.cx=(int)x;_gfx.cy=(int)y;_flush();}");
        emitln("static void pas_linerel(long long dx,long long dy){pas_lineto(_gfx.cx+dx,_gfx.cy+dy);}");
        emitln("static void pas_moveto(long long x,long long y){_gfx.cx=(int)x;_gfx.cy=(int)y;}");
        emitln("static void pas_moverel(long long dx,long long dy){_gfx.cx+=(int)dx;_gfx.cy+=(int)dy;}");
        emitln("static void pas_rectangle(long long x1,long long y1,long long x2,long long y2){");
        emitln("  int c=_gfx.fg;_line((int)x1,(int)y1,(int)x2,(int)y1,c);_line((int)x2,(int)y1,(int)x2,(int)y2,c);");
        emitln("  _line((int)x2,(int)y2,(int)x1,(int)y2,c);_line((int)x1,(int)y2,(int)x1,(int)y1,c);_flush();}");
        if (!userProcs.count("bar")) {
            emitln("static void pas_bar(long long x1,long long y1,long long x2,long long y2){");
            emitln("  for(int y=(int)y1;y<=(int)y2;y++)for(int x=(int)x1;x<=(int)x2;x++)_setpx(x,y,_gfx.fill);_flush();}");
        }
        if (!userProcs.count("circle")) {
            emitln("static void pas_circle(long long cx,long long cy,long long r){");
            emitln("  int x=0,y=(int)r,d=1-(int)r;");
            emitln("  while(x<=y){_setpx((int)cx+x,(int)cy+y,_gfx.fg);_setpx((int)cx-x,(int)cy+y,_gfx.fg);");
            emitln("    _setpx((int)cx+x,(int)cy-y,_gfx.fg);_setpx((int)cx-x,(int)cy-y,_gfx.fg);");
            emitln("    _setpx((int)cx+y,(int)cy+x,_gfx.fg);_setpx((int)cx-y,(int)cy+x,_gfx.fg);");
            emitln("    _setpx((int)cx+y,(int)cy-x,_gfx.fg);_setpx((int)cx-y,(int)cy-x,_gfx.fg);");
            emitln("    if(d<0)d+=2*x+3;else{d+=2*(x-y)+5;y--;}x++;}");
            emitln("  _flush();}");
        }
        if (!userProcs.count("arc")) {
            emitln("static void pas_arc(long long cx,long long cy,long long sa,long long ea,long long r){");
            emitln("  if(ea<sa)ea+=360;int steps=(int)r*4;if(steps<36)steps=36;");
            emitln("  double px0=-1e9,py0=-1e9;");
            emitln("  for(int i=0;i<=steps;i++){double a=M_PI*(sa+(ea-sa)*i/(double)steps)/180.0;");
            emitln("    double px=cx+r*cos(a),py=cy-r*sin(a);");
            emitln("    if(px0>-1e8)_line((int)px0,(int)py0,(int)px,(int)py,_gfx.fg);px0=px;py0=py;}_flush();}");
        }
        if (!userProcs.count("ellipse")) {
            emitln("static void pas_ellipse(long long cx,long long cy,long long sa,long long ea,long long rx,long long ry){");
            emitln("  if(ea<sa)ea+=360;int steps=(int)(rx+ry)*2;if(steps<36)steps=36;");
            emitln("  double px0=-1e9,py0=-1e9;");
            emitln("  for(int i=0;i<=steps;i++){double a=M_PI*(sa+(ea-sa)*i/(double)steps)/180.0;");
            emitln("    double px=cx+rx*cos(a),py=cy-ry*sin(a);");
            emitln("    if(px0>-1e8)_line((int)px0,(int)py0,(int)px,(int)py,_gfx.fg);px0=px;py0=py;}_flush();}");
        }
        emitln("static void pas_fillellipse(long long cx,long long cy,long long rx,long long ry){");
        emitln("  for(int y=-(int)ry;y<=(int)ry;y++){double t=(double)y/ry;");
        emitln("    int hw=(int)(rx*sqrt(1.0-t*t<0?0:1.0-t*t));");
        emitln("    for(int x=-hw;x<=hw;x++)_setpx((int)cx+x,(int)cy+y,_gfx.fill);}");
        emitln("  _flush();}");
        emitln("static void pas_floodfill(long long x,long long y,long long bc){");
        emitln("  int border=_col(bc),fill=_gfx.fill;");
        emitln("  if(x<0||y<0||x>=_gfx.W||y>=_gfx.H)return;");
        emitln("  int target=_gfx.px[(int)y*_gfx.W+(int)x];");
        emitln("  if(target==border||target==fill)return;");
        emitln("  int stack[640*480][2];int top=0;stack[top][0]=(int)x;stack[top][1]=(int)y;top++;");
        emitln("  while(top>0){top--;int cx=stack[top][0],cy=stack[top][1];");
        emitln("    if(cx<0||cy<0||cx>=_gfx.W||cy>=_gfx.H)continue;");
        emitln("    int p=_gfx.px[cy*_gfx.W+cx];if(p==border||p==fill)continue;");
        emitln("    _gfx.px[cy*_gfx.W+cx]=fill;");
        emitln("    if(top+4<640*480){stack[top][0]=cx+1;stack[top][1]=cy;top++;");
        emitln("      stack[top][0]=cx-1;stack[top][1]=cy;top++;");
        emitln("      stack[top][0]=cx;stack[top][1]=cy+1;top++;");
        emitln("      stack[top][0]=cx;stack[top][1]=cy-1;top++;}}_flush();}");
        emitln("static void pas_outtextxy(long long x,long long y,char*s){");
        emitln("  int cx=(int)x;for(;*s;s++){_drawchar(cx,(int)y,*s,_gfx.fg);cx+=8;}_pump();_flush();}");
        emitln("static void pas_outtext(char*s){");
        emitln("  for(;*s;s++){_drawchar(_gfx.cx,_gfx.cy,*s,_gfx.fg);_gfx.cx+=8;}_pump();_flush();}");
        emitln("static long long pas_textwidth(char*s){return (long long)strlen(s)*8;}");
        emitln("static long long pas_textheight(char*s){(void)s;return 8;}");
        emitln("#else");
        // Stubs — skip if user defined same name
        emitStub("initgraph",    "static inline void pas_initgraph(void){}");
        emitStub("closegraph",   "static inline void pas_closegraph(void){}");
        emitStub("graphresult",  "static inline int  pas_graphresult(void){ return 0; }");
        emitStub("cleardevice",  "static inline void pas_cleardevice(void){}");
        emitStub("getmaxx",      "static inline int  pas_getmaxx(void){ return 639; }");
        emitStub("getmaxy",      "static inline int  pas_getmaxy(void){ return 479; }");
        emitStub("getx",         "static inline int  pas_getx(void){ return 0; }");
        emitStub("gety",         "static inline int  pas_gety(void){ return 0; }");
        emitStub("getcolor",     "static inline int  pas_getcolor(void){ return 15; }");
        emitStub("getbkcolor",   "static inline int  pas_getbkcolor(void){ return 0; }");
        emitStub("readkey",      "static inline char pas_readkey(void){ return getchar(); }");
        emitStub("delay",        "static inline void pas_delay(long long ms){ (void)ms; }");
        emitStub("putpixel",     "static inline void pas_putpixel(long long x,long long y,long long c){(void)x;(void)y;(void)c;}");
        emitStub("getpixel",     "static inline long long pas_getpixel(long long x,long long y){(void)x;(void)y;return 0;}");
        emitStub("line",         "static inline void pas_line(long long x1,long long y1,long long x2,long long y2){(void)x1;(void)y1;(void)x2;(void)y2;}");
        emitStub("lineto",       "static inline void pas_lineto(long long x,long long y){(void)x;(void)y;}");
        emitStub("linerel",      "static inline void pas_linerel(long long x,long long y){(void)x;(void)y;}");
        emitStub("moveto",       "static inline void pas_moveto(long long x,long long y){(void)x;(void)y;}");
        emitStub("moverel",      "static inline void pas_moverel(long long x,long long y){(void)x;(void)y;}");
        emitStub("rectangle",    "static inline void pas_rectangle(long long x1,long long y1,long long x2,long long y2){(void)x1;(void)y1;(void)x2;(void)y2;}");
        emitStub("bar",          "static inline void pas_bar(long long x1,long long y1,long long x2,long long y2){(void)x1;(void)y1;(void)x2;(void)y2;}");
        emitStub("circle",       "static inline void pas_circle(long long x,long long y,long long r){(void)x;(void)y;(void)r;}");
        emitStub("arc",          "static inline void pas_arc(long long x,long long y,long long s,long long e,long long r){(void)x;(void)y;(void)s;(void)e;(void)r;}");
        emitStub("ellipse",      "static inline void pas_ellipse(long long x,long long y,long long s,long long e,long long rx,long long ry){(void)x;(void)y;(void)s;(void)e;(void)rx;(void)ry;}");
        emitStub("fillellipse",  "static inline void pas_fillellipse(long long x,long long y,long long rx,long long ry){(void)x;(void)y;(void)rx;(void)ry;}");
        emitStub("floodfill",    "static inline void pas_floodfill(long long x,long long y,long long c){(void)x;(void)y;(void)c;}");
        emitStub("setcolor",     "static inline void pas_setcolor(long long c){(void)c;}");
        emitStub("setbkcolor",   "static inline void pas_setbkcolor(long long c){(void)c;}");
        emitStub("setrgbcolor",  "static inline void pas_setrgbcolor(long long r,long long g,long long b){(void)r;(void)g;(void)b;}");
        emitStub("setrgbbkcolor","static inline void pas_setrgbbkcolor(long long r,long long g,long long b){(void)r;(void)g;(void)b;}");
        emitStub("setfillstyle", "static inline void pas_setfillstyle(long long p,long long c){(void)p;(void)c;}");
        emitStub("outtextxy",    "static inline void pas_outtextxy(long long x,long long y,char*s){(void)x;(void)y;printf(\"%s\",s);}");
        emitStub("outtext",      "static inline void pas_outtext(char*s){printf(\"%s\",s);}");
        emitStub("textwidth",    "static inline long long pas_textwidth(char*s){return (long long)strlen(s)*8;}");
        emitStub("textheight",   "static inline long long pas_textheight(char*s){(void)s;return 8;}");
        emitln("#endif");
        emitln();

        // ── Runtime helpers ───────────────────
        emitln("/* Runtime helpers */");
        emitln("static void pas_print_ll(long long v)  { printf(\"%lld\", v); }");
        emitln("static void pas_print_d(double v)      { printf(\"%g\", v); }");
        emitln("static void pas_print_s(const char* v) { printf(\"%s\", v); }");
        emitln("static void pas_print_c(char v)        { printf(\"%c\", v); }");
        emitln("static void pas_print_val(double v)    { printf(\"%g\", v); }");
        out << "#define PAS_PRINT(x) pas_print_ll((long long)(x))\n";
        emitln();
        // String helpers
        emitln("static char* pas_copy_str(const char* s, long long start, long long len) {");
        emitln("    static char buf[1024];");
        emitln("    int st = (int)start - 1; if (st < 0) st = 0;");
        emitln("    int n  = (int)len;");
        emitln("    int slen = (int)strlen(s);");
        emitln("    if (st >= slen) { buf[0]='\\0'; return buf; }");
        emitln("    if (st + n > slen) n = slen - st;");
        emitln("    strncpy(buf, s + st, n); buf[n] = '\\0';");
        emitln("    return buf;");
        emitln("}");
        emitln();
        emitln("static char _concat_buf[4096];");
        emitln("static char* pas_concat_str(int n, ...) {");
        emitln("    va_list ap; va_start(ap, n);");
        emitln("    _concat_buf[0] = '\\0';");
        emitln("    for (int i = 0; i < n; i++) strcat(_concat_buf, va_arg(ap, char*));");
        emitln("    va_end(ap); return _concat_buf;");
        emitln("}");
        emitln();

        // ── Forward declarations ──────────────
        emitln("/* Forward declarations */");
        for (auto& d : prog->declarations)
            if (dynamic_cast<ProcDef*>(d.get()))
                genForwardDecl(d);
        emitln();

        // ── Global variables & constants ──────
        emitln("/* Global variables */");
        for (auto& d : prog->declarations)
            if (!dynamic_cast<ProcDef*>(d.get()))
                genDecl(d, true);
        emitln();

        // ── Procedure/function definitions ────
        for (auto& d : prog->declarations)
            if (dynamic_cast<ProcDef*>(d.get()))
                genProcDef(d);

        // ── main ──────────────────────────────
        emitln("int main(void) {");
        indent++;
        auto* body = dynamic_cast<CompoundNode*>(prog->body.get());
        if (body) {
            for (auto& s : body->statements) genStmt(s);
        } else {
            genStmt(prog->body);
        }
        emitln("    return 0;");
        indent--;
        emitln("}");

        return out.str();
    }
};

// ─────────────────────────────────────────────
//  Compile Pascal → native binary via gcc
// ─────────────────────────────────────────────
static bool compilePascal(const std::string& source,
                           const std::string& outBin,
                           std::string& errorMsg) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto ast = parser.parse();

    Compiler compiler;
    std::string cSource = compiler.compile(ast);

    std::string cFile = "/tmp/_pascal_compiled.c";
    {
        std::ofstream f(cFile);
        if (!f) { errorMsg = "Cannot write temp file"; return false; }
        f << cSource;
    }

    // Find SDL2 flags — try multiple known paths
    std::string sdlFlags;
    {
        std::vector<std::string> candidates = {
            "sdl2-config",
            "/opt/homebrew/bin/sdl2-config",
            "/usr/local/bin/sdl2-config",
            "/opt/local/bin/sdl2-config",
        };
        auto run = [](const std::string& cmd) {
            std::string out;
            FILE* p = popen(cmd.c_str(), "r");
            if (!p) return out;
            char buf[1024];
            while (fgets(buf, sizeof(buf), p)) out += buf;
            pclose(p);
            while (!out.empty() && (out.back()=='\n'||out.back()=='\r'||out.back()==' '))
                out.pop_back();
            return out;
        };
        for (auto& cfg : candidates) {
            std::string cflags = run("\"" + cfg + "\" --cflags 2>/dev/null");
            std::string libs   = run("\"" + cfg + "\" --libs   2>/dev/null");
            if (!cflags.empty() && !libs.empty()) {
                sdlFlags = " -DHAVE_SDL2 " + cflags + " " + libs;
                break;
            }
        }
    }

    std::string cmd = "gcc -O2 -x c \"" + cFile + "\" -o \"" + outBin + "\""
                    + sdlFlags
                    + " -lm 2>/tmp/_pascal_compile_err.txt";
    std::cerr << "[SDL2 flags: " << (sdlFlags.empty() ? "(none)" : sdlFlags) << "]\n";
    std::cerr << "[gcc cmd: " << cmd << "]\n";
    int ret = system(cmd.c_str());

    if (ret != 0) {
        std::ifstream ef("/tmp/_pascal_compile_err.txt");
        errorMsg = std::string(std::istreambuf_iterator<char>(ef), {});
        std::ofstream dbg("/tmp/_pascal_compiled_debug.c");
        dbg << cSource;
        errorMsg += "\n[Generated C saved to /tmp/_pascal_compiled_debug.c]";
        return false;
    }
    return true;
}
