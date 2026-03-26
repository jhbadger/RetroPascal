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
#include <unistd.h>
#include <chrono>
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
    PROCEDURE, FUNCTION, ARRAY, OF, RECORD, TYPE, OBJECT, USES, LABEL, GOTO, CASE, OTHERWISE,
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
    {"of",        TokenType::OF},        {"record",    TokenType::RECORD},  {"type",      TokenType::TYPE},
    {"object",    TokenType::OBJECT},
    {"uses",      TokenType::USES},
    {"label",     TokenType::LABEL},
    {"goto",      TokenType::GOTO},
    {"case",      TokenType::CASE},
    {"of",        TokenType::OF},
    {"otherwise", TokenType::OTHERWISE},
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
                case '#': {
                    // #N  — Turbo Pascal numeric char literal e.g. #13 #27
                    std::string num;
                    while (pos < src.size() && isdigit(src[pos]))
                        num += src[pos++];
                    if (num.empty()) throw std::runtime_error("Expected digit after '#'");
                    char ch = (char)std::stoi(num);
                    tokens.push_back({TokenType::CHAR_LITERAL, std::string(1, ch), tok_line});
                    break;
                }
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
struct WriteNode      : ASTNode { std::vector<ASTPtr> args; std::vector<int> widths; std::vector<int> decimals; bool newline; };
struct ReadNode       : ASTNode { std::vector<ASTPtr> vars; bool newline; };
struct CallNode       : ASTNode { std::string name; std::vector<ASTPtr> args; };
struct IndexNode      : ASTNode { ASTPtr array; ASTPtr index; std::vector<ASTPtr> indices; };
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

struct ArrayDim { int lo, hi; };
struct VarDecl : ASTNode {
    std::vector<std::string> names;
    std::string type_name;          // pascal type name
    int array_lo = 0, array_hi = 0;
    bool is_array = false;
    std::vector<ArrayDim> array_dims; // multi-dim support
    bool is_record = false;         // inline record type
    std::vector<std::pair<std::string,std::string>> record_fields;
};

struct ConstDecl : ASTNode { std::string name; ASTPtr value; };
struct FieldAccessNode : ASTNode { ASTPtr record; std::string field; };
struct GotoNode        : ASTNode { std::string label; };
struct LabelNode       : ASTNode { std::string label; ASTPtr stmt; };
struct SetMemberNode   : ASTNode { ASTPtr expr; std::vector<ASTPtr> elements; };
struct CaseBranch { std::vector<ASTPtr> values; ASTPtr body; };
struct CaseNode : ASTNode { ASTPtr expr; std::vector<CaseBranch> branches; ASTPtr else_branch; };
struct MethodCallNode  : ASTNode {
    ASTPtr object;
    std::string method;
    std::vector<ASTPtr> args;
    std::string typeName;
};

// Object/record type definition
struct ObjTypeDef {
    std::string parent;  // lower-case parent type name, or ""
    std::vector<std::pair<std::string,std::string>> fields; // (name, type)
    std::unordered_map<std::string, std::shared_ptr<struct ProcDef>> methods;
};
static std::unordered_map<std::string, ObjTypeDef> g_recordTypes;

// Helper: get all fields including inherited ones
static std::vector<std::pair<std::string,std::string>> getAllFields(const std::string& typeName) {
    std::vector<std::pair<std::string,std::string>> result;
    std::string cur = typeName;
    std::vector<std::string> chain;
    while (!cur.empty()) {
        auto it = g_recordTypes.find(cur);
        if (it == g_recordTypes.end()) break;
        chain.push_back(cur);
        cur = it->second.parent;
    }
    // Emit parent fields first
    std::reverse(chain.begin(), chain.end());
    for (auto& t : chain)
        for (auto& f : g_recordTypes[t].fields)
            result.push_back(f);
    return result;
}

// Helper: find a method in type or any ancestor
static std::shared_ptr<ProcDef> findMethod(const std::string& typeName, const std::string& methodName) {
    std::string cur = typeName;
    while (!cur.empty()) {
        auto it = g_recordTypes.find(cur);
        if (it == g_recordTypes.end()) break;
        auto mit = it->second.methods.find(methodName);
        if (mit != it->second.methods.end()) return mit->second;
        cur = it->second.parent;
    }
    return nullptr;
}


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
    // Parse a record field list: returns list of (fieldName, typeName)
    std::vector<std::pair<std::string,std::string>> parseRecordFields() {
        std::vector<std::pair<std::string,std::string>> fields;
        while (!check(TokenType::END) && !check(TokenType::EOF_TOKEN)) {
            if (!check(TokenType::IDENTIFIER)) break;
            std::vector<std::string> fnames;
            fnames.push_back(consume().value);
            while (match(TokenType::COMMA))
                fnames.push_back(expect(TokenType::IDENTIFIER).value);
            expect(TokenType::COLON);
            std::string ftype = consume().value;
            match(TokenType::SEMICOLON);
            for (auto& fn : fnames)
                fields.push_back({fn, ftype});
        }
        return fields;
    }

    void parseDeclarations(std::vector<ASTPtr>& decls) {
        while (true) {
            if (check(TokenType::USES)) {
                // uses wincrt, winprocs; — just skip it
                consume();
                while (!check(TokenType::SEMICOLON) && !check(TokenType::EOF_TOKEN))
                    consume();
                match(TokenType::SEMICOLON);
            } else if (check(TokenType::LABEL)) {
                // label back, new_game, continue; — parse and ignore (labels handled at stmt level)
                consume();
                while (!check(TokenType::SEMICOLON) && !check(TokenType::EOF_TOKEN))
                    consume();
                match(TokenType::SEMICOLON);
            } else if (check(TokenType::VAR)) {
                consume();
                parseVarSection(decls);
            } else if (check(TokenType::CONST)) {
                consume();
                parseConstSection(decls);
            } else if (check(TokenType::TYPE)) {
                // TYPE section: type Foo = record ... end;
                consume(); // eat "type"
                while (check(TokenType::IDENTIFIER)) {
                    std::string typeName = consume().value;
                    std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
                    expect(TokenType::EQ);
                    if (check(TokenType::RECORD) || check(TokenType::OBJECT)) {
                        bool isObj = check(TokenType::OBJECT);
                        consume(); // eat 'record' or 'object'
                        // Check for parent type: object(TParent)
                        std::string parent;
                        if (isObj && match(TokenType::LPAREN)) {
                            parent = consume().value;
                            std::transform(parent.begin(), parent.end(), parent.begin(), ::tolower);
                            expect(TokenType::RPAREN);
                        }
                        // Parse fields and method declarations
                        auto fields = parseRecordFields();
                        // Parse method declarations (procedure/function headers only)
                        while (check(TokenType::PROCEDURE) || check(TokenType::FUNCTION)) {
                            bool isFn = check(TokenType::FUNCTION);
                            consume();
                            std::string mname = expect(TokenType::IDENTIFIER).value;
                            std::transform(mname.begin(), mname.end(), mname.begin(), ::tolower);
                            auto pd = std::make_shared<ProcDef>();
                            pd->name = mname;
                            pd->is_function = isFn;
                            if (match(TokenType::LPAREN)) {
                                while (!check(TokenType::RPAREN)) {
                                    bool isVar = false;
                                    if (peek().type == TokenType::VAR) { consume(); isVar=true; }
                                    std::vector<std::string> pnames;
                                    pnames.push_back(expect(TokenType::IDENTIFIER).value);
                                    while (match(TokenType::COMMA))
                                        pnames.push_back(expect(TokenType::IDENTIFIER).value);
                                    expect(TokenType::COLON);
                                    std::string ptype = consume().value;
                                    for (auto& n : pnames)
                                        pd->params.push_back({n, ptype, isVar});
                                    if (!match(TokenType::SEMICOLON)) break;
                                }
                                expect(TokenType::RPAREN);
                            }
                            if (isFn) {
                                expect(TokenType::COLON);
                                pd->return_type = consume().value;
                            }
                            match(TokenType::SEMICOLON);
                            // Store as forward declaration — body parsed elsewhere
                            g_recordTypes[typeName].methods[mname] = pd;
                        }
                        expect(TokenType::END);
                        match(TokenType::SEMICOLON);
                        g_recordTypes[typeName].fields = fields;
                        g_recordTypes[typeName].parent = parent;
                    } else {
                        // type alias — skip for now
                        consume();
                        match(TokenType::SEMICOLON);
                    }
                }
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
            // Inline record type: var p: record x,y: integer; end;
            if (check(TokenType::RECORD)) {
                consume();
                vd->is_record = true;
                vd->type_name = "__inline_record__";
                vd->record_fields = parseRecordFields();
                expect(TokenType::END);
                // Register with a synthetic type name based on first field
                // so the interpreter can create instances
                static int anonCount = 0;
                std::string anonName = "__anon_record_" + std::to_string(anonCount++) + "__";
                g_recordTypes[anonName].fields = vd->record_fields;
                vd->type_name = anonName;
                match(TokenType::SEMICOLON);
            } else if (match(TokenType::ARRAY)) {
                // Array type — may be multi-dimensional: array[1..16, 1..61]
                vd->is_array = true;
                expect(TokenType::LBRACKET);
                // First dimension
                vd->array_lo = std::stoi(expect(TokenType::INTEGER_LITERAL).value);
                expect(TokenType::DOTDOT);
                vd->array_hi = std::stoi(expect(TokenType::INTEGER_LITERAL).value);
                vd->array_dims.push_back({vd->array_lo, vd->array_hi});
                // Additional dimensions: , lo..hi
                while (match(TokenType::COMMA)) {
                    ArrayDim d;
                    d.lo = std::stoi(expect(TokenType::INTEGER_LITERAL).value);
                    expect(TokenType::DOTDOT);
                    d.hi = std::stoi(expect(TokenType::INTEGER_LITERAL).value);
                    vd->array_dims.push_back(d);
                    // Extend the flat size: total = product of all dimensions
                    vd->array_hi = vd->array_lo + 
                        (vd->array_dims[0].hi - vd->array_dims[0].lo + 1) *
                        (d.hi - d.lo + 1) - 1;
                }
                expect(TokenType::RBRACKET);
                expect(TokenType::OF);
                vd->type_name = consume().value;
                match(TokenType::SEMICOLON);
            } else {
                vd->type_name = consume().value;
                // Check if this is a known record type name
                std::string lo = vd->type_name;
                std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                if (g_recordTypes.count(lo)) {
                    vd->is_record = true;
                    vd->record_fields = getAllFields(lo);
                }
                expect(TokenType::SEMICOLON);
            }
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
        // Method implementation: procedure TAnimal.init
        if (check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER) {
            consume(); // eat '.'
            std::string methodName = consume().value;
            // Store as "TypeName.MethodName" so interpreter can register it
            pd->name = pd->name + "." + methodName;
        }
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
            size_t prevPos = pos;
            node->statements.push_back(parseStatement());
            match(TokenType::SEMICOLON);
            if (pos == prevPos) consume(); // skip unrecognized token
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

        // CASE statement
        if (check(TokenType::CASE)) {
            consume(); // 'case'
            auto node = std::make_shared<CaseNode>();
            node->expr = parseExpr();
            expect(TokenType::OF);
            while (!check(TokenType::END) && !check(TokenType::OTHERWISE) &&
                   !check(TokenType::EOF_TOKEN)) {
                if (check(TokenType::SEMICOLON)) { consume(); continue; }
                if (check(TokenType::END) || check(TokenType::OTHERWISE)) break;
                CaseBranch branch;
                branch.values.push_back(parseExpr());
                while (match(TokenType::COMMA))
                    branch.values.push_back(parseExpr());
                expect(TokenType::COLON);
                branch.body = parseStatement();
                match(TokenType::SEMICOLON);
                node->branches.push_back(std::move(branch));
            }
            if (check(TokenType::OTHERWISE)) {
                consume();
                match(TokenType::COLON); // optional colon after 'otherwise'
                node->else_branch = parseStatement();
                match(TokenType::SEMICOLON);
            }
            expect(TokenType::END);
            return node;
        }

        // GOTO statement
        if (check(TokenType::GOTO)) {
            consume();
            auto g = std::make_shared<GotoNode>();
            g->label = consume().value;
            return g;
        }

        // Numeric label definition: 100: stmt
        if (check(TokenType::INTEGER_LITERAL) && peek(1).type == TokenType::COLON) {
            auto lbl = std::make_shared<LabelNode>();
            lbl->label = consume().value;
            consume(); // ':'
            if (!check(TokenType::SEMICOLON) && !check(TokenType::END) &&
                !check(TokenType::UNTIL) && !check(TokenType::EOF_TOKEN))
                lbl->stmt = parseStatement();
            return lbl;
        }
        // IDENTIFIER: (label) or identifier statement
        if (check(TokenType::IDENTIFIER)) {
            // Label definition: name: stmt
            if (peek(1).type == TokenType::COLON) {
                auto lbl = std::make_shared<LabelNode>();
                lbl->label = consume().value;
                consume(); // ':'
                if (!check(TokenType::SEMICOLON) && !check(TokenType::END) &&
                    !check(TokenType::UNTIL) && !check(TokenType::EOF_TOKEN))
                    lbl->stmt = parseStatement();
                return lbl;
            }
            return parseIdentStatement();
        }
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
            auto parseOneArg = [&]() {
                node->args.push_back(parseExpr());
                int w = -1, d = -1;
                if (match(TokenType::COLON)) {
                    if (check(TokenType::INTEGER_LITERAL)) w = (int)std::stoll(consume().value);
                    else if (check(TokenType::MINUS)) { consume(); w = 0; } // negative width — ignore
                    if (match(TokenType::COLON)) {
                        if (check(TokenType::INTEGER_LITERAL)) d = (int)std::stoll(consume().value);
                    }
                }
                node->widths.push_back(w);
                node->decimals.push_back(d);
            };
            parseOneArg();
            while (match(TokenType::COMMA)) parseOneArg();
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
        std::string nameLo = name;
        std::transform(nameLo.begin(), nameLo.end(), nameLo.begin(), ::tolower);

        // Special: str(expr:width, var) — Turbo Pascal format
        if (nameLo == "str" && check(TokenType::LPAREN)) {
            consume(); // '('
            auto valExpr = parseExpr();
            ASTPtr widthExpr;
            if (match(TokenType::COLON))
                widthExpr = parseExpr();
            expect(TokenType::COMMA);
            auto destExpr = parseExpr(); // should be VarNode
            expect(TokenType::RPAREN);
            auto call = std::make_shared<CallNode>();
            call->name = "str";
            call->args.push_back(valExpr);
            if (widthExpr) call->args.push_back(widthExpr);
            else call->args.push_back(std::make_shared<NumberNode>()); // placeholder 0
            call->args.push_back(destExpr);
            return call;
        }

        // Build target: handle field access (p.x, p.a.b) and array index
        // First accumulate any leading dot-field chains
        ASTPtr target;
        {
            auto varNode = std::make_shared<VarNode>(); varNode->name = name;
            target = varNode;
            // Handle dot-field access: p.x.y etc.
            while (check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER) {
                consume(); // eat '.'
                std::string fieldName = expect(TokenType::IDENTIFIER).value;
                auto fa = std::make_shared<FieldAccessNode>();
                fa->record = target;
                fa->field  = fieldName;
                target = fa;
            }
            // Handle array index: arr[i] or arr[i, j]
            if (check(TokenType::LBRACKET)) {
                auto idx = std::make_shared<IndexNode>();
                idx->array = target;
                consume();
                idx->indices.push_back(parseExpr());
                while (match(TokenType::COMMA))
                    idx->indices.push_back(parseExpr());
                idx->index = idx->indices[0];
                expect(TokenType::RBRACKET);
                target = idx;
                // Handle field access after array index: arr[i].field
                while (check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER) {
                    consume(); // eat '.'
                    std::string fieldName = expect(TokenType::IDENTIFIER).value;
                    auto fa = std::make_shared<FieldAccessNode>();
                    fa->record = target;
                    fa->field  = fieldName;
                    target = fa;
                }
            }
        }

        if (check(TokenType::ASSIGN)) {
            expect(TokenType::ASSIGN);
            auto assign = std::make_shared<AssignNode>();
            assign->target = target;
            assign->value = parseExpr();
            return assign;
        }

        // Method call: obj.method(args) or obj.method
        if (auto* fa = dynamic_cast<FieldAccessNode*>(target.get())) {
            auto mc = std::make_shared<MethodCallNode>();
            mc->object = fa->record;
            mc->method = fa->field;
            std::transform(mc->method.begin(), mc->method.end(), mc->method.begin(), ::tolower);
            if (match(TokenType::LPAREN)) {
                if (!check(TokenType::RPAREN)) {
                    mc->args.push_back(parseExpr());
                    while (match(TokenType::COMMA)) mc->args.push_back(parseExpr());
                }
                expect(TokenType::RPAREN);
            }
            return mc;
        }

        // Plain procedure/function call
        auto* vn = dynamic_cast<VarNode*>(target.get());
        auto call = std::make_shared<CallNode>();
        call->name = vn ? vn->name : name;
        if (match(TokenType::LPAREN)) {
            if (!check(TokenType::RPAREN)) {
                call->args.push_back(parseExpr());
                while (match(TokenType::COMMA)) call->args.push_back(parseExpr());
            }
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
        // Check for 'in [...]' set membership: expr in [a, b, c]
        if (peek().type == TokenType::IDENTIFIER && peek().value == "in") {
            consume(); // eat 'in'
            expect(TokenType::LBRACKET);
            auto set = std::make_shared<SetMemberNode>();
            set->expr = left;
            if (!check(TokenType::RBRACKET)) {
                set->elements.push_back(parseExpr());
                while (match(TokenType::COMMA))
                    set->elements.push_back(parseExpr());
            }
            expect(TokenType::RBRACKET);
            return set;
        }
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
            // Array index — may be multi-dimensional: a[i, j]
            if (check(TokenType::LBRACKET)) {
                auto v = std::make_shared<VarNode>(); v->name = name;
                ASTPtr result = v;
                // Handle chained indexing: a[i][j] (e.g. string array element char)
                while (check(TokenType::LBRACKET)) {
                    auto idx = std::make_shared<IndexNode>();
                    idx->array = result;
                    consume(); // '['
                    idx->indices.push_back(parseExpr());
                    while (match(TokenType::COMMA))
                        idx->indices.push_back(parseExpr());
                    idx->index = idx->indices[0];
                    expect(TokenType::RBRACKET);
                    result = idx;
                }
                // Handle field access after array index: k[i].field or k[i].method(...)
                while (check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER) {
                    consume(); // eat '.'
                    std::string fieldName = expect(TokenType::IDENTIFIER).value;
                    if (check(TokenType::LPAREN)) {
                        auto mc = std::make_shared<MethodCallNode>();
                        mc->object = result;
                        mc->method = fieldName;
                        std::transform(mc->method.begin(), mc->method.end(), mc->method.begin(), ::tolower);
                        consume(); // eat '('
                        if (!check(TokenType::RPAREN)) {
                            mc->args.push_back(parseExpr());
                            while (match(TokenType::COMMA)) mc->args.push_back(parseExpr());
                        }
                        expect(TokenType::RPAREN);
                        result = mc;
                    } else {
                        auto fa = std::make_shared<FieldAccessNode>();
                        fa->record = result;
                        fa->field  = fieldName;
                        result = fa;
                    }
                }
                return result;
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
            // Field access: rec.field (only when DOT followed by identifier)
            {
                ASTPtr result = std::make_shared<VarNode>();
                dynamic_cast<VarNode*>(result.get())->name = name;
                while (check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER) {
                    consume(); // eat '.'
                    std::string fieldName = expect(TokenType::IDENTIFIER).value;
                    // If followed by '(' it's a method call expression
                    if (check(TokenType::LPAREN)) {
                        auto mc = std::make_shared<MethodCallNode>();
                        mc->object = result;
                        mc->method = fieldName;
                        std::transform(mc->method.begin(), mc->method.end(), mc->method.begin(), ::tolower);
                        consume(); // eat '('
                        if (!check(TokenType::RPAREN)) {
                            mc->args.push_back(parseExpr());
                            while (match(TokenType::COMMA)) mc->args.push_back(parseExpr());
                        }
                        expect(TokenType::RPAREN);
                        result = mc;
                    } else {
                        auto fa = std::make_shared<FieldAccessNode>();
                        fa->record = result;
                        fa->field  = fieldName;
                        result = fa;
                    }
                }
                return result;
            }
        }
        throw std::runtime_error("Unexpected token '" + peek().value + "' at line " + std::to_string(peek().line));
    }
};


// ─────────────────────────────────────────────
//  Terminal support (gotoxy, clrscr, keypressed, readkey)
//  Uses ANSI escape sequences + raw mode
// ─────────────────────────────────────────────
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>

static struct termios _orig_termios;
static bool _raw_mode = false;

static void _disable_raw_mode() {
    if (_raw_mode) {
        tcsetattr(STDIN_FILENO, TCSANOW, &_orig_termios);
        _raw_mode = false;
    }
}

static void _enable_raw_mode() {
    if (_raw_mode) return;
    tcgetattr(STDIN_FILENO, &_orig_termios);
    atexit(_disable_raw_mode);
    struct termios raw = _orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    _raw_mode = true;
}

static void _pas_gotoxy(int x, int y) {
    printf("\033[%d;%dH", y, x);
    fflush(stdout);
}

static void _pas_clrscr() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

static bool _pas_keypressed() {
    _enable_raw_mode();
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}

// Arrow key codes we return as single chars (above ASCII range mapped to safe values)
// We use chars that won't collide with normal printable keys:
// Up='\x01', Down='\x02', Left='\x03', Right='\x04'
#define KEY_UP    '\x01'
#define KEY_DOWN  '\x02'
#define KEY_LEFT  '\x03'
#define KEY_RIGHT '\x04'

static char _pas_readkey() {
    _enable_raw_mode();
    char ch = 0;
    while (true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = {0, 50000};
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0) {
            if (read(STDIN_FILENO, &ch, 1) != 1) continue;
            if (ch == 27) {
                // Possible escape sequence — check for [ with short timeout
                struct timeval tv2 = {0, 10000};
                fd_set fds2; FD_ZERO(&fds2); FD_SET(STDIN_FILENO, &fds2);
                if (select(STDIN_FILENO + 1, &fds2, nullptr, nullptr, &tv2) > 0) {
                    char seq[2] = {0, 0};
                    if (read(STDIN_FILENO, &seq[0], 1) == 1 && seq[0] == '[') {
                        if (read(STDIN_FILENO, &seq[1], 1) == 1) {
                            switch (seq[1]) {
                                case 'A': return KEY_UP;
                                case 'B': return KEY_DOWN;
                                case 'C': return KEY_RIGHT;
                                case 'D': return KEY_LEFT;
                            }
                        }
                    }
                }
                return 27; // bare ESC
            }
            // Normalise Enter: both CR and LF -> #13
            if (ch == '\n') ch = '\r';
            return ch;
        }
    }
}

// ─────────────────────────────────────────────
//  Runtime Value
// ─────────────────────────────────────────────
struct Value {
    enum class Type { NIL, INT, REAL, BOOL, STRING, CHAR, ARRAY, RECORD } vtype = Type::NIL;
    long long   ival = 0;
    double      rval = 0;
    bool        bval = false;
    std::string sval;
    char        cval = 0;
    std::vector<Value> arr;
    std::vector<ArrayDim> arr_dims; // dimension info for multi-dim arrays
    std::shared_ptr<std::unordered_map<std::string,Value>> fields; // for RECORD

    static Value from_int(long long v)    { Value x; x.vtype=Type::INT;    x.ival=v; return x; }
    static Value from_real(double v)      { Value x; x.vtype=Type::REAL;   x.rval=v; return x; }
    static Value from_bool(bool v)        { Value x; x.vtype=Type::BOOL;   x.bval=v; return x; }
    static Value from_string(const std::string& v) { Value x; x.vtype=Type::STRING; x.sval=v; return x; }
    static Value from_char(char v)        { Value x; x.vtype=Type::CHAR;   x.cval=v; return x; }
    static Value make_array(int lo, int hi, std::vector<ArrayDim> dims = {}) {
        Value x; x.vtype=Type::ARRAY;
        x.arr.resize(hi - lo + 1);
        x.ival = lo; // store lo in ival
        x.arr_dims = std::move(dims);
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
            case Type::RECORD: return "<record>";
            case Type::STRING: return sval;
            case Type::CHAR:   return std::string(1, cval);
            default: return "<nil>";
        }
    }
    static Value make_record() {
        Value x; x.vtype = Type::RECORD;
        x.fields = std::make_shared<std::unordered_map<std::string,Value>>();
        return x;
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
struct GotoException   { std::string label; };

class Interpreter {
    std::unordered_map<std::string, std::shared_ptr<ProcDef>> procs;
    std::shared_ptr<Environment> global_env;
    std::vector<std::pair<Value*, std::string>> selfStack; // current object during method execution

    // Evaluate an expression
    Value eval(const ASTPtr& node, std::shared_ptr<Environment> env) {
        if (!node) return Value{};

        if (auto* n = dynamic_cast<NumberNode*>(node.get())) {
            return n->is_int ? Value::from_int((long long)n->value) : Value::from_real(n->value);
        }
        if (auto* n = dynamic_cast<BoolNode*>(node.get()))   return Value::from_bool(n->value);
        if (auto* n = dynamic_cast<StringNode*>(node.get())) return Value::from_string(n->value);
        if (auto* n = dynamic_cast<CharNode*>(node.get()))   return Value::from_char(n->value);

        if (auto* n = dynamic_cast<SetMemberNode*>(node.get())) {
            Value v = eval(n->expr, env);
            for (auto& elem : n->elements) {
                Value e = eval(elem, env);
                if (v.vtype == Value::Type::CHAR && e.vtype == Value::Type::CHAR)
                    { if (v.cval == e.cval) return Value::from_bool(true); }
                else if (v.vtype == Value::Type::STRING && e.vtype == Value::Type::CHAR)
                    { if (!v.sval.empty() && v.sval[0] == e.cval) return Value::from_bool(true); }
                else if (v.as_int() == e.as_int()) return Value::from_bool(true);
            }
            return Value::from_bool(false);
        }
        if (auto* n = dynamic_cast<MethodCallNode*>(node.get())) {
            return evalMethodCall(n, env);
        }
        if (auto* n = dynamic_cast<SetMemberNode*>(node.get())) {
            Value v = eval(n->expr, env);
            for (auto& elem : n->elements) {
                Value e = eval(elem, env);
                if (v.vtype == Value::Type::CHAR && e.vtype == Value::Type::CHAR)
                    { if (v.cval == e.cval) return Value::from_bool(true); }
                else if (v.vtype == Value::Type::STRING && e.vtype == Value::Type::CHAR)
                    { if (!v.sval.empty() && v.sval[0] == e.cval) return Value::from_bool(true); }
                else if (v.as_int() == e.as_int()) return Value::from_bool(true);
            }
            return Value::from_bool(false);
        }
        if (auto* n = dynamic_cast<MethodCallNode*>(node.get())) {
            return evalMethodCall(n, env);
        }
        if (auto* n = dynamic_cast<FieldAccessNode*>(node.get())) {
            // Fast path: arr[i].field — access field directly without copying record
            if (auto* idx = dynamic_cast<IndexNode*>(n->record.get())) {
                if (auto* vn = dynamic_cast<VarNode*>(idx->array.get())) {
                    std::string alo = vn->name;
                    std::transform(alo.begin(), alo.end(), alo.begin(), ::tolower);
                    std::string flo = n->field;
                    std::transform(flo.begin(), flo.end(), flo.begin(), ::tolower);
                    try {
                        // Evaluate index first
                        int rawIdx = (int)eval(idx->index, env).as_int();
                        Value& arr = env->get_ref(alo);
                        if (arr.vtype == Value::Type::ARRAY) {
                            int flat = rawIdx - (int)arr.ival;
                            if (flat >= 0 && flat < (int)arr.arr.size()) {
                                Value& elem = arr.arr[flat];
                                if (elem.fields) {
                                    auto it = elem.fields->find(flo);
                                    if (it != elem.fields->end()) return it->second;
                                }
                            }
                        }
                    } catch (...) {}
                }
            }
            // General path: evaluate record expression, then access field
            Value rec = eval(n->record, env);
            if (rec.vtype != Value::Type::RECORD || !rec.fields)
                throw std::runtime_error("Not a record");
            std::string flo = n->field;
            std::transform(flo.begin(), flo.end(), flo.begin(), ::tolower);
            auto it = rec.fields->find(flo);
            if (it != rec.fields->end()) return it->second;
            // Not a field — try as zero-arg method call
            std::string tn2 = getObjTypeName(rec);
            if (!tn2.empty() && findMethod(tn2, flo)) {
                MethodCallNode mc2; mc2.object = n->record; mc2.method = flo;
                return evalMethodCall(&mc2, env);
            }
            throw std::runtime_error("Unknown record field: " + n->field);
        }
        if (auto* n = dynamic_cast<VarNode*>(node.get())) {
            std::string lo = n->name;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            // Zero-argument functions that arrive as VarNodes (no parentheses)
            {
                auto [handled, gval] = evalGraphCall(lo, {},
                    [&](const ASTPtr& n2) { return eval(n2, env); });
                if (handled) return gval;
            }
            // Terminal/timer zero-arg functions
            static const std::set<std::string> zeroArgBuiltins = {
                "keypressed","gettickcount","readkey","clrscr","donewincrt","halt","exit"
            };
            if (zeroArgBuiltins.count(lo))
                return evalCall(lo, {}, env);
            // Inside a method: try env first (has up-to-date copies of self fields)
            // then fall back to selfStack for fields not yet in env
            if (!selfStack.empty()) {
                // Try normal env lookup first (call_env has copies of self fields)
                try { return env->get(lo); } catch (...) {}
                // Not in env — check selfPtr fields and zero-arg methods
                auto& [selfPtr, selfType] = selfStack.back();
                if (selfPtr && selfPtr->fields && lo != "__type__") {
                    auto fit = selfPtr->fields->find(lo);
                    if (fit != selfPtr->fields->end()) return fit->second;
                }
                std::string fullName = selfType + "." + lo;
                if (procs.count(fullName)) {
                    auto pd2 = findMethod(selfType, lo);
                    if (pd2 && pd2->is_function) return evalCall(fullName, {}, env);
                }
                throw std::runtime_error("Undefined variable: " + lo);
            }
            // Zero-arg user function called without parens
            if (procs.count(lo)) {
                auto& pd = procs[lo];
                if (pd && pd->is_function) return evalCall(lo, {}, env);
            } else {
            }
            return env->get(lo);
        }

        if (auto* n = dynamic_cast<IndexNode*>(node.get())) {
            Value arr = eval(n->array, env);
            // String character indexing: s[i] returns char (1-based)
            if (arr.vtype == Value::Type::STRING) {
                int idx = (int)eval(n->index, env).as_int() - 1;
                if (idx >= 0 && idx < (int)arr.sval.size())
                    return Value::from_char(arr.sval[idx]);
                return Value::from_char('\0');
            }
            if (arr.vtype != Value::Type::ARRAY)
                throw std::runtime_error("Indexing non-array");
            int flat = 0;
            if (n->indices.size() > 1 && !arr.arr_dims.empty()) {
                int idx0 = (int)eval(n->indices[0], env).as_int() - arr.arr_dims[0].lo;
                int idx1 = (int)eval(n->indices[1], env).as_int() - arr.arr_dims[1].lo;
                int cols  = arr.arr_dims[1].hi - arr.arr_dims[1].lo + 1;
                flat = idx0 * cols + idx1;
            } else {
                flat = (int)eval(n->index, env).as_int() - (int)arr.ival;
            }
            if (flat < 0 || flat >= (int)arr.arr.size())
                throw std::runtime_error("Array index out of bounds");
            return arr.arr[flat];
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

    // Get the type name stored in an object's fields map

    // Evaluate a method call: obj.method(args)
    // The object is passed by reference so field assignments persist

    // Execute MethodCallNode as a statement
    void execMethodCall(const MethodCallNode* n, std::shared_ptr<Environment> env) {
        evalMethodCall(n, env);
    }

    std::string getObjTypeName(const Value& obj) {
        if (obj.vtype != Value::Type::RECORD || !obj.fields) return "";
        auto it = obj.fields->find("__type__");
        return (it != obj.fields->end()) ? it->second.sval : "";
    }

    // Create a default-initialized object of the given record/object type
    Value makeDefaultObject(const std::string& tlo) {
        Value rec = Value::make_record();
        (*rec.fields)["__type__"] = Value::from_string(tlo);
        for (auto& [fn, ft] : getAllFields(tlo)) {
            std::string flo = fn; std::transform(flo.begin(), flo.end(), flo.begin(), ::tolower);
            std::string ftlo = ft; std::transform(ftlo.begin(), ftlo.end(), ftlo.begin(), ::tolower);
            Value fv;
            if (ftlo=="integer"||ftlo=="longint"||ftlo=="word"||ftlo=="byte") fv=Value::from_int(0);
            else if (ftlo=="real"||ftlo=="double"||ftlo=="single") fv=Value::from_real(0.0);
            else if (ftlo=="boolean") fv=Value::from_bool(false);
            else if (ftlo=="char")    fv=Value::from_char('\0');
            else if (ftlo=="string")  fv=Value::from_string("");
            (*rec.fields)[flo] = fv;
        }
        return rec;
    }

    Value evalMethodCall(const MethodCallNode* n, std::shared_ptr<Environment> env) {
        // Check for constructor-style call: TypeName.Method(args)
        // where TypeName is a known object type, not a variable
        Value objVal;
        std::string typeName;
        bool isConstructorCall = false;
        if (auto* vn = dynamic_cast<VarNode*>(n->object.get())) {
            std::string lo = vn->name;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            if (g_recordTypes.count(lo)) {
                typeName = lo;
                objVal = makeDefaultObject(lo);
                isConstructorCall = true;
            }
        }
        if (typeName.empty()) {
            objVal = eval(n->object, env);
            typeName = getObjTypeName(objVal);
        }
        if (typeName.empty()) throw std::runtime_error("Cannot call method on non-object");

        auto pd = findMethod(typeName, n->method);
        if (!pd) throw std::runtime_error("Unknown method: " + typeName + "." + n->method);

        // Find root variable for write-back
        std::function<std::string(const ASTNode*)> getRootName = [&](const ASTNode* node) -> std::string {
            if (auto* vn = dynamic_cast<const VarNode*>(node)) {
                std::string lo = vn->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower); return lo;
            }
            if (auto* fa = dynamic_cast<const FieldAccessNode*>(node)) return getRootName(fa->record.get());
            if (auto* mc = dynamic_cast<const MethodCallNode*>(node)) return getRootName(mc->object.get());
            return "";
        };
        std::string rootName = getRootName(n->object.get());

        auto call_env = std::make_shared<Environment>(global_env);
        // Copy all self fields into call env
        for (auto& [fn, fv] : *objVal.fields)
            call_env->set_local(fn, fv);
        // Copy fields from parent types too (already in objVal if initialized correctly)

        // Bind parameters
        for (size_t i = 0; i < pd->params.size() && i < n->args.size(); i++) {
            std::string pn = pd->params[i].name;
            std::transform(pn.begin(), pn.end(), pn.begin(), ::tolower);
            call_env->set_local(pn, eval(n->args[i], env));
        }

        // Return variable for functions (use short method name)
        std::string rname;
        if (pd->is_function) {
            rname = pd->name;
            std::transform(rname.begin(), rname.end(), rname.begin(), ::tolower);
            call_env->set_local(rname, Value{});
        }

        // Process local variable declarations
        processDeclarations(pd->decls, call_env);

        // Get selfPtr for write-back
        Value* selfPtr = nullptr;
        if (!rootName.empty()) {
            try { selfPtr = &env->get_ref(rootName); } catch (...) {}
        }
        selfStack.push_back({selfPtr, typeName});

        try { exec(pd->body, call_env); } catch (ReturnException&) {}

        selfStack.pop_back();

        // Write back modified fields
        if (selfPtr && selfPtr->fields) {
            for (auto& [fn, fv] : *selfPtr->fields) {
                if (fn == "__type__") continue;
                auto it2 = call_env->vars.find(fn);
                if (it2 != call_env->vars.end()) fv = it2->second;
            }
        }

        if (pd->is_function) return call_env->get(rname);
        // Constructor-style call: collect modified fields back into objVal and return it
        if (isConstructorCall) {
            for (auto& [fn, fv] : *objVal.fields) {
                if (fn == "__type__") continue;
                auto it2 = call_env->vars.find(fn);
                if (it2 != call_env->vars.end()) fv = it2->second;
            }
            return objVal;
        }
        return Value{};
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
        if (name == "insert") {
            // insert(source, var dest, pos) — 1-based
            std::string ins = eval(arg_nodes[0], env).to_string();
            if (auto* vn = dynamic_cast<VarNode*>(arg_nodes[1].get())) {
                std::string lo = vn->name;
                std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                Value& dest = env->get_ref(lo);
                std::string s = dest.to_string();
                int p = (int)eval(arg_nodes[2], env).as_int() - 1;
                if (p < 0) p = 0;
                if (p > (int)s.size()) p = (int)s.size();
                s.insert(p, ins);
                dest = Value::from_string(s);
            }
            return Value{};
        }
        if (name == "delete") {
            // delete(var str, pos, len) — 1-based
            if (auto* vn = dynamic_cast<VarNode*>(arg_nodes[0].get())) {
                std::string lo = vn->name;
                std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                Value& dest = env->get_ref(lo);
                std::string s = dest.to_string();
                int p = (int)eval(arg_nodes[1], env).as_int() - 1;
                int l = (int)eval(arg_nodes[2], env).as_int();
                if (p >= 0 && p < (int)s.size())
                    s.erase(p, l);
                dest = Value::from_string(s);
            }
            return Value{};
        }
        if (name == "booltostr") {
            bool b = eval(arg_nodes[0], env).as_bool();
            // BoolToStr(b) or BoolToStr(b, UseBoolStrs) — returns "True"/"False"
            return Value::from_string(b ? "True" : "False");
        }
        if (name == "strtobool" || name == "strtobooldef") {
            std::string s = eval(arg_nodes[0], env).as_string();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            bool def = arg_nodes.size() > 1 ? eval(arg_nodes[1], env).as_bool() : false;
            if (s == "true" || s == "1") return Value::from_bool(true);
            if (s == "false" || s == "0") return Value::from_bool(false);
            return Value::from_bool(def);
        }
        if (name == "gettickcount") {
            auto now = std::chrono::steady_clock::now().time_since_epoch();
            return Value::from_int(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
        }
        if (name == "donewincrt") { _disable_raw_mode(); exit(0); }
        if (name == "halt") { _disable_raw_mode(); exit(0); }
        if (name == "exit") { throw ReturnException{}; }  // return from current procedure/function
        if (name == "clrscr")     { _pas_clrscr(); return Value{}; }
        if (name == "gotoxy") {
            _pas_gotoxy((int)eval(arg_nodes[0],env).as_int(), (int)eval(arg_nodes[1],env).as_int());
            return Value{};
        }
        if (name == "keypressed") { return Value::from_bool(_pas_keypressed()); }
        if (name == "readkey")    { return Value::from_char(_pas_readkey()); }
        if (name == "random") {
            if (arg_nodes.empty()) return Value::from_real((double)rand() / RAND_MAX);
            long long n = eval(arg_nodes[0], env).as_int();
            return Value::from_int(n > 0 ? rand() % n : 0);
        }
        if (name == "randomize") { srand((unsigned)time(nullptr)); return Value{}; }
        if (name == "sin")  return Value::from_real(std::sin(eval(arg_nodes[0],env).as_real()));
        if (name == "cos")  return Value::from_real(std::cos(eval(arg_nodes[0],env).as_real()));
        if (name == "arctan") return Value::from_real(std::atan(eval(arg_nodes[0],env).as_real()));
        if (name == "arcsin") return Value::from_real(std::asin(eval(arg_nodes[0],env).as_real()));
        if (name == "arccos") return Value::from_real(std::acos(eval(arg_nodes[0],env).as_real()));
        if (name == "tan")  return Value::from_real(std::tan(eval(arg_nodes[0],env).as_real()));
        if (name == "exp")  return Value::from_real(std::exp(eval(arg_nodes[0],env).as_real()));
        if (name == "ln")   return Value::from_real(std::log(eval(arg_nodes[0],env).as_real()));
        if (name == "log")  return Value::from_real(std::log10(eval(arg_nodes[0],env).as_real()));
        if (name == "int")  return Value::from_int((long long)eval(arg_nodes[0],env).as_real());
        if (name == "frac") { double v=eval(arg_nodes[0],env).as_real(); return Value::from_real(v - (long long)v); }
        if (name == "pi")   return Value::from_real(3.14159265358979323846);
        if (name == "max")  { auto a=eval(arg_nodes[0],env), b=eval(arg_nodes[1],env); return a.as_real()>=b.as_real()?a:b; }
        if (name == "min")  { auto a=eval(arg_nodes[0],env), b=eval(arg_nodes[1],env); return a.as_real()<=b.as_real()?a:b; }
        if (name == "str") {
            // str(value [,width], var_dest) — Turbo Pascal format
            // Parser emits: [value, width, var] when width given, [value, var] without
            Value v = eval(arg_nodes[0], env);
            ASTPtr varArg;
            long long width = 0;
            if (arg_nodes.size() == 3) {
                width = eval(arg_nodes[1], env).as_int();
                varArg = arg_nodes[2];
            } else if (arg_nodes.size() == 2) {
                varArg = arg_nodes[1];
            }
            std::string s = (v.vtype == Value::Type::REAL)
                ? [&]{ std::ostringstream o; o << v.rval; return o.str(); }()
                : std::to_string(v.as_int());
            if (width > 0)
                while ((long long)s.size() < width) s = " " + s;
            if (varArg) {
                if (auto* vn = dynamic_cast<VarNode*>(varArg.get())) {
                    std::string lo = vn->name;
                    std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                    env->set(lo, Value::from_string(s));
                }
            }
            return Value{};
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
        if (it == procs.end()) {
            // Inside a method — try as self method call
            // BUT only if there's no free function with this name
            if (!selfStack.empty() && !procs.count(name)) {
                auto& [selfPtr2, selfType2] = selfStack.back();
                auto pd2 = findMethod(selfType2, name);
                if (pd2) {
                    std::string fullName2 = selfType2 + "." + name;
                    auto it2 = procs.find(fullName2);
                    if (it2 != procs.end()) {
                        // Build a fake MethodCallNode and dispatch
                        auto call_env2 = std::make_shared<Environment>(global_env);
                        if (selfPtr2 && selfPtr2->fields)
                            for (auto& [fn, fv] : *selfPtr2->fields)
                                call_env2->set_local(fn, fv);
                        for (size_t i = 0; i < pd2->params.size() && i < arg_nodes.size(); i++) {
                            std::string pn = pd2->params[i].name;
                            std::transform(pn.begin(), pn.end(), pn.begin(), ::tolower);
                            call_env2->set_local(pn, eval(arg_nodes[i], env));
                        }
                        std::string rname3;
                        if (pd2->is_function) {
                            rname3 = pd2->name;
                            std::transform(rname3.begin(), rname3.end(), rname3.begin(), ::tolower);
                            call_env2->set_local(rname3, Value{});
                        }
                        processDeclarations(pd2->decls, call_env2);
                        selfStack.push_back({selfPtr2, selfType2});
                        try { exec(pd2->body, call_env2); } catch (ReturnException&) {}
                        selfStack.pop_back();
                        if (selfPtr2 && selfPtr2->fields)
                            for (auto& [fn, fv] : *selfPtr2->fields) {
                                if (fn == "__type__") continue;
                                auto fit2 = call_env2->vars.find(fn);
                                if (fit2 != call_env2->vars.end()) fv = fit2->second;
                            }
                        if (pd2->is_function) return call_env2->get(rname3);
                        return Value{};
                    }
                }
            }
            throw std::runtime_error("Undefined procedure/function: " + name);
        }

        auto& pd = it->second;
        // Use caller's env as parent so nested procs can access outer return vars
        auto call_env = std::make_shared<Environment>(env);

        // bind parameters
        for (size_t i = 0; i < pd->params.size(); i++) {
            std::string pname = pd->params[i].name;
            std::transform(pname.begin(), pname.end(), pname.begin(), ::tolower);
            Value arg_val = i < arg_nodes.size() ? eval(arg_nodes[i], env) : Value{};
            call_env->set_local(pname, arg_val);
        }

        // result variable for functions (strip type prefix for methods)
        if (pd->is_function) {
            std::string rname = name;
            auto rdot = rname.find('.');
            if (rdot != std::string::npos) rname = rname.substr(rdot + 1);
            call_env->set_local(rname, Value{});
        }

        // process local declarations
        processDeclarations(pd->decls, call_env);

        try {
            exec(pd->body, call_env);
        } catch (ReturnException&) {}

        if (pd->is_function) {
            std::string rname2 = name;
            auto rdot2 = rname2.find('.');
            if (rdot2 != std::string::npos) rname2 = rname2.substr(rdot2 + 1);
            return call_env->get(rname2);
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
            size_t start = 0;
            while (start < n->statements.size()) {
                try {
                    for (size_t i = start; i < n->statements.size(); i++)
                        exec(n->statements[i], env);
                    return;
                } catch (GotoException& ge) {
                    // Find label in this compound's statements
                    bool found = false;
                    for (size_t i = 0; i < n->statements.size(); i++) {
                        auto* lbl = dynamic_cast<LabelNode*>(n->statements[i].get());
                        if (lbl && lbl->label == ge.label) {
                            start = i;
                            found = true;
                            break;
                        }
                    }
                    if (!found) throw; // propagate to outer compound
                }
            }
            return;
        }
        if (auto* n = dynamic_cast<AssignNode*>(node.get())) {
            Value val = eval(n->value, env);
            if (auto* idx = dynamic_cast<IndexNode*>(n->target.get())) {
                auto* vn = dynamic_cast<VarNode*>(idx->array.get());
                if (!vn) throw std::runtime_error("Invalid array target");
                std::string lo = vn->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                Value& arr = env->get_ref(lo);
                // String character assignment: s[i] := char (1-based)
                if (arr.vtype == Value::Type::STRING) {
                    int idx1 = (int)eval(idx->index, env).as_int() - 1;
                    char ch = val.vtype == Value::Type::CHAR ? val.cval : (char)val.as_int();
                    if (idx1 >= 0 && idx1 < (int)arr.sval.size())
                        arr.sval[idx1] = ch;
                    return;
                }
                if (arr.vtype != Value::Type::ARRAY) throw std::runtime_error("Not an array: " + lo);
                int flat = 0;
                if (idx->indices.size() > 1 && !arr.arr_dims.empty()) {
                    int i0 = (int)eval(idx->indices[0], env).as_int() - arr.arr_dims[0].lo;
                    int i1 = (int)eval(idx->indices[1], env).as_int() - arr.arr_dims[1].lo;
                    int cols = arr.arr_dims[1].hi - arr.arr_dims[1].lo + 1;
                    flat = i0 * cols + i1;
                } else {
                    flat = (int)eval(idx->index, env).as_int() - (int)arr.ival;
                }
                if (flat < 0 || flat >= (int)arr.arr.size()) throw std::runtime_error("Array index out of bounds");
                arr.arr[flat] = val;
            } else if (auto* fa = dynamic_cast<FieldAccessNode*>(n->target.get())) {
                // record field assignment: p.x := val  OR  t[i].x := val
                // Walk the chain collecting field names, then find root (var or array index)
                std::vector<std::string> path;
                ASTNode* cur = fa;
                while (auto* fac = dynamic_cast<FieldAccessNode*>(cur)) {
                    std::string flo = fac->field;
                    std::transform(flo.begin(), flo.end(), flo.begin(), ::tolower);
                    path.push_back(flo);
                    cur = fac->record.get();
                }
                std::reverse(path.begin(), path.end());

                // Get reference to the root Value — either a plain var or an array element
                Value* rec = nullptr;
                if (auto* rootVar = dynamic_cast<VarNode*>(cur)) {
                    std::string rootName = rootVar->name;
                    std::transform(rootName.begin(), rootName.end(), rootName.begin(), ::tolower);
                    rec = &env->get_ref(rootName);
                } else if (auto* rootIdx = dynamic_cast<IndexNode*>(cur)) {
                    // t[i].field — get reference to the array element
                    auto* vn2 = dynamic_cast<VarNode*>(rootIdx->array.get());
                    if (!vn2) throw std::runtime_error("Invalid assignment target");
                    std::string lo2 = vn2->name;
                    std::transform(lo2.begin(), lo2.end(), lo2.begin(), ::tolower);
                    // Compute index BEFORE taking reference (eval may resize env map)
                    int flat2 = 0;
                    {
                        Value& arr2tmp = env->get_ref(lo2);
                        if (rootIdx->indices.size() > 1 && !arr2tmp.arr_dims.empty()) {
                            int i0 = (int)eval(rootIdx->indices[0], env).as_int() - arr2tmp.arr_dims[0].lo;
                            int i1 = (int)eval(rootIdx->indices[1], env).as_int() - arr2tmp.arr_dims[1].lo;
                            flat2 = i0 * (arr2tmp.arr_dims[1].hi - arr2tmp.arr_dims[1].lo + 1) + i1;
                        } else {
                            flat2 = (int)eval(rootIdx->index, env).as_int() - (int)arr2tmp.ival;
                        }
                        if (flat2 < 0 || flat2 >= (int)arr2tmp.arr.size())
                            throw std::runtime_error("Array index out of bounds");
                    }
                    // Now take stable reference (no more eval calls after this)
                    Value& arr2 = env->get_ref(lo2);
                    rec = &arr2.arr[flat2];
                } else {
                    throw std::runtime_error("Invalid record assignment target");
                }

                // Walk field path and update
                for (size_t pi = 0; pi < path.size() - 1; pi++) {
                    if (rec->vtype != Value::Type::RECORD || !rec->fields)
                        throw std::runtime_error("Not a record: " + path[pi]);
                    rec = &(*rec->fields)[path[pi]];
                }
                if (rec->vtype != Value::Type::RECORD || !rec->fields)
                    throw std::runtime_error("Not a record");
                (*rec->fields)[path.back()] = val;
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
            for (size_t ai = 0; ai < n->args.size(); ++ai) {
                Value v = eval(n->args[ai], env);
                int w = (ai < n->widths.size()) ? n->widths[ai] : -1;
                int d = (ai < n->decimals.size()) ? n->decimals[ai] : -1;
                std::string s;
                if (d >= 0 && (v.vtype == Value::Type::REAL || v.vtype == Value::Type::INT)) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.*f", d, v.as_real());
                    s = buf;
                } else {
                    s = v.to_string();
                }
                if (w > 0 && (int)s.size() < w) s = std::string(w - s.size(), ' ') + s;
                std::cout << s;
            }
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
        if (auto* n = dynamic_cast<MethodCallNode*>(node.get())) {
            execMethodCall(n, env);
            return;
        }
        if (auto* n = dynamic_cast<CaseNode*>(node.get())) {
            Value v = eval(n->expr, env);
            bool matched = false;
            for (auto& branch : n->branches) {
                for (auto& val : branch.values) {
                    Value bv = eval(val, env);
                    bool eq = false;
                    if (v.vtype == Value::Type::CHAR && bv.vtype == Value::Type::CHAR)
                        eq = v.cval == bv.cval;
                    else if (v.vtype == Value::Type::STRING && bv.vtype == Value::Type::CHAR)
                        eq = !v.sval.empty() && v.sval[0] == bv.cval;
                    else if (v.vtype == Value::Type::CHAR && bv.vtype == Value::Type::STRING)
                        eq = !bv.sval.empty() && bv.sval[0] == v.cval;
                    else
                        eq = v.as_int() == bv.as_int();
                    if (eq) {
                        exec(branch.body, env);
                        matched = true;
                        break;
                    }
                }
                if (matched) break;
            }
            if (!matched && n->else_branch)
                exec(n->else_branch, env);
            return;
        }
        if (auto* n = dynamic_cast<GotoNode*>(node.get())) {
            throw GotoException{n->label};
        }
        if (auto* n = dynamic_cast<LabelNode*>(node.get())) {
            if (n->stmt) exec(n->stmt, env);
            return;
        }
        if (auto* n = dynamic_cast<MethodCallNode*>(node.get())) {
            evalMethodCall(n, env);
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
                    if (vd->is_record) {
                        Value rec = Value::make_record();
                        // Store type name so methods can be found
                        std::string tlo = vd->type_name;
                        std::transform(tlo.begin(), tlo.end(), tlo.begin(), ::tolower);
                        (*rec.fields)["__type__"] = Value::from_string(tlo);
                        for (auto& [fn, ft] : vd->record_fields) {
                            std::string flo = fn;
                            std::transform(flo.begin(), flo.end(), flo.begin(), ::tolower);
                            std::string ftlo = ft;
                            std::transform(ftlo.begin(), ftlo.end(), ftlo.begin(), ::tolower);
                            Value fv;
                            if (ftlo=="integer"||ftlo=="longint"||ftlo=="word"||ftlo=="byte") fv=Value::from_int(0);
                            else if (ftlo=="real"||ftlo=="double"||ftlo=="single") fv=Value::from_real(0.0);
                            else if (ftlo=="boolean") fv=Value::from_bool(false);
                            else if (ftlo=="char")    fv=Value::from_char('\0');
                            else if (ftlo=="string")  fv=Value::from_string("");
                            (*rec.fields)[flo] = fv;
                        }
                        env->set_local(lo, rec);
                    } else if (vd->is_array) {
                        auto arrVal = Value::make_array(vd->array_lo, vd->array_hi, vd->array_dims);
                        // If element type is a record, initialize each element
                        std::string elo = vd->type_name;
                        std::transform(elo.begin(), elo.end(), elo.begin(), ::tolower);
                        if (g_recordTypes.count(elo)) {
                            for (auto& elem : arrVal.arr) {
                                elem = Value::make_record();
                                (*elem.fields)["__type__"] = Value::from_string(elo);
                                for (auto& [fn, ft] : getAllFields(elo)) {
                                    std::string flo2 = fn;
                                    std::transform(flo2.begin(), flo2.end(), flo2.begin(), ::tolower);
                                    std::string ftlo = ft;
                                    std::transform(ftlo.begin(), ftlo.end(), ftlo.begin(), ::tolower);
                                    Value fv;
                                    if (ftlo=="integer"||ftlo=="longint"||ftlo=="shortint"||ftlo=="word"||ftlo=="byte") fv=Value::from_int(0);
                                    else if (ftlo=="real"||ftlo=="double"||ftlo=="single") fv=Value::from_real(0.0);
                                    else if (ftlo=="boolean") fv=Value::from_bool(false);
                                    else if (ftlo=="char")    fv=Value::from_char('\0');
                                    else if (ftlo=="string")  fv=Value::from_string("");
                                    (*elem.fields)[flo2] = fv;
                                }
                            }
                        }
                        env->set_local(lo, arrVal);
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
                std::string lo = pd->name;
                std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                auto dot = lo.find('.');
                if (dot != std::string::npos) {
                    // Method implementation: "typename.methodname"
                    std::string typeName   = lo.substr(0, dot);
                    std::string methodName = lo.substr(dot + 1);
                    auto shpd = std::make_shared<ProcDef>(*pd);
                    shpd->name = methodName;
                    g_recordTypes[typeName].methods[methodName] = shpd;
                    // Also store under full name for backward compat
                    procs[lo] = shpd;
                } else {
                    procs[lo] = std::make_shared<ProcDef>(*pd);
                }
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
        // Strip shebang line so the file can be used as a script: #!/usr/bin/env pascal
        if (source.size() >= 2 && source[0] == '#' && source[1] == '!') {
            auto nl = source.find('\n');
            source = (nl == std::string::npos) ? "" : source.substr(nl + 1);
        }
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
    std::string currentFunc;       // name of function being compiled (for return var)
    std::string currentMethodType; // type name when inside a method body, else ""
    std::set<std::string> userProcs; // user-defined proc/func names (lower-case)
    std::map<std::string, std::string> varTypes; // varname(lower) -> pascal type
    std::set<std::string> byRefParams; // param names that are var (pointer) params

    // ── Type helpers ──────────────────────────
    std::string cType(const std::string& pas) {
        std::string t = pas;
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        if (t == "integer" || t == "longint" || t == "shortint" || t == "word" || t == "byte" || t == "smallint") return "long long";
        if (t == "real" || t == "double" || t == "single")                   return "double";
        if (t == "boolean")                                                    return "int";
        if (t == "char")                                                        return "char";
        if (t == "string")                                                      return "char*";
        // Check if it's a known record type
        if (g_recordTypes.count(t)) return "struct pas_rec_" + t;
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

    void emit(const std::string& s) {        out << s; }
    void emitln(const std::string& s = "") {        out << s << "\n"; }
    void emitind(const std::string& s) { out << ind() << s << "\n"; }

    // ── Expression codegen ────────────────────

    bool isStrExpr(const ASTPtr& node) {
        if (!node) return false;
        if (dynamic_cast<StringNode*>(node.get())) return true;
        if (auto* vn = dynamic_cast<VarNode*>(node.get())) {
            std::string lo = vn->name; std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
            auto it = varTypes.find(lo);
            if (it == varTypes.end()) it = varTypes.find("self."+lo);
            if (it != varTypes.end()) { std::string t=it->second; std::transform(t.begin(),t.end(),t.begin(),::tolower); return t=="string"; }
        }
        if (auto* fa = dynamic_cast<FieldAccessNode*>(node.get())) {
            std::string ft = getFieldType(fa); std::transform(ft.begin(),ft.end(),ft.begin(),::tolower);
            return ft == "string";
        }
        // Index into an array-of-string yields a string element.
        // Note: indexing a string variable (string[i]) yields a char, not a string.
        if (auto* idx = dynamic_cast<IndexNode*>(node.get())) {
            if (auto* vn = dynamic_cast<VarNode*>(idx->array.get())) {
                std::string lo = vn->name;
                std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                // Only check arrayElemType (declared arrays), not varTypes
                // because varTypes["string_var"] == "string" but string[i] is a char
                auto it = arrayElemType.find(lo);
                if (it != arrayElemType.end()) {
                    std::string et = it->second;
                    std::transform(et.begin(), et.end(), et.begin(), ::tolower);
                    if (et == "string") return true;
                }
            }
        }
        if (auto* bn = dynamic_cast<BinOpNode*>(node.get())) return bn->op=="+" && (isStrExpr(bn->left)||isStrExpr(bn->right));
        return false;
    }

    // Get the object type name from an expression (for method dispatch)
    std::string getExprTypeName(const ASTPtr& node) {
        if (auto* vn = dynamic_cast<VarNode*>(node.get())) {
            std::string lo = vn->name;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            auto it = varTypes.find(lo);
            if (it != varTypes.end()) return it->second;
        }
        return "";
    }

    std::string genMethodCall(const MethodCallNode* n) {
        // Emit: pas_DefiningTypeName_methodname((struct pas_rec_DefType*)&obj, args...)
        std::string typeName = getExprTypeName(n->object);
        std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
        // Walk parent chain to find where method is defined
        // Check both declared methods (in type section) and implemented procs
        std::string definingType = typeName;
        {
            std::string cur = typeName;
            while (!cur.empty()) {
                auto it = g_recordTypes.find(cur);
                if (it == g_recordTypes.end()) break;
                // Check declared methods
                if (it->second.methods.count(n->method)) { definingType = cur; break; }
                // Check implemented procs (e.g. Pet.init declared without header)
                std::string fullName = cur + "." + n->method;
                if (userProcs.count(fullName)) { definingType = cur; break; }
                cur = it->second.parent;
            }
        }
        std::string objExpr = genExpr(n->object);
        std::string fnName = "pas_" + definingType + "_" + n->method;
        // Cast to parent struct pointer if needed (for inherited methods)
        std::string selfArg;
        if (definingType != typeName)
            selfArg = "(struct pas_rec_" + definingType + "*)&" + objExpr;
        else
            selfArg = "&" + objExpr;
        std::string call = fnName + "(" + selfArg;
        for (auto& arg : n->args) call += ", " + genExpr(arg);
        call += ")";
        return call;
    }

    // Get the Pascal type name of a field access expression
    std::string getFieldType(const FieldAccessNode* fa) const {
        // Walk to the root VarNode to get the record type
        std::vector<std::string> fields;
        const ASTNode* cur = fa;
        while (auto* fac = dynamic_cast<const FieldAccessNode*>(cur)) {
            std::string flo = fac->field;
            std::transform(flo.begin(), flo.end(), flo.begin(), ::tolower);
            fields.push_back(flo);
            cur = fac->record.get();
        }
        std::reverse(fields.begin(), fields.end());
        const VarNode* root = dynamic_cast<const VarNode*>(cur);
        if (!root) return "";
        std::string rlo = root->name;
        std::transform(rlo.begin(), rlo.end(), rlo.begin(), ::tolower);
        // Find record type name
        auto it = varTypes.find(rlo);
        if (it == varTypes.end()) return "";
        std::string typeName = it->second;
        std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
        // Walk through nested fields
        for (size_t i = 0; i < fields.size(); i++) {
            auto git = g_recordTypes.find(typeName);
            if (git == g_recordTypes.end()) return "";
            bool found = false;
            for (auto& [fn, ft] : git->second.fields) {
                std::string flo2 = fn;
                std::transform(flo2.begin(), flo2.end(), flo2.begin(), ::tolower);
                if (flo2 == fields[i]) {
                    typeName = ft;
                    std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
                    found = true;
                    break;
                }
            }
            if (!found) return "";
        }
        return typeName;
    }

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
        if (auto* n = dynamic_cast<FieldAccessNode*>(node.get())) {
            return getFieldType(n) == "real";
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
                "sqrt","sin","cos","tan","exp","ln","log","pi","frac","random",
                "arctan","arcsin","arccos","int","power"
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
            char cv = n->value;
            std::string s = "'";
            if      (cv == '\'') s += "\\'";
            else if (cv == '\\') s += "\\\\";
            else if (cv == '\r') s += "\\r";
            else if (cv == '\n') s += "\\n";
            else if (cv == '\t') s += "\\t";
            else if (cv == '\0') s += "\\0";
            else if ((unsigned char)cv < 32 || (unsigned char)cv == 127)
                { char buf[8]; snprintf(buf, sizeof(buf), "\\x%02x", (unsigned char)cv); s += buf; }
            else s += cv;
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
        if (auto* n = dynamic_cast<MethodCallNode*>(node.get())) {
            return genMethodCall(n);
        }
        if (auto* n = dynamic_cast<FieldAccessNode*>(node.get())) {
            std::string flo = n->field;
            std::transform(flo.begin(), flo.end(), flo.begin(), ::tolower);
            // Check if this is actually a zero-arg method call
            std::string objType = getExprTypeName(n->record);
            std::transform(objType.begin(), objType.end(), objType.begin(), ::tolower);
            if (!objType.empty() && findMethod(objType, flo)) {
                MethodCallNode mc;
                mc.object = n->record;
                mc.method = flo;
                return genMethodCall(&mc);
            }
            // Regular field access
            std::string expr = genExpr(n->record);
            return expr + ".pas_" + flo;
        }
        if (auto* n = dynamic_cast<VarNode*>(node.get())) {
            std::string lo = n->name;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            if (!currentFunc.empty()) {
                std::string cf = currentFunc;
                std::transform(cf.begin(), cf.end(), cf.begin(), ::tolower);
                auto dot = cf.find('.');
                std::string shortCf = (dot != std::string::npos) ? cf.substr(dot+1) : cf;
                if (lo == cf || lo == shortCf) return "_ret_";
            }
            // Self field inside a method
            if (!currentMethodType.empty() && varTypes.count("self." + lo) && !varTypes.count(lo))
                return "self->pas_" + lo;
            // Zero-arg self method inside a method body (bare name, no parens)
            if (!currentMethodType.empty() && !varTypes.count(lo) && !varTypes.count("self."+lo)) {
                auto pd3 = findMethod(currentMethodType, lo);
                if (pd3 && pd3->is_function) {
                    // Find defining type (for inheritance)
                    std::string defType = currentMethodType;
                    for (std::string cur3 = currentMethodType; !cur3.empty(); ) {
                        auto it5 = g_recordTypes.find(cur3);
                        if (it5 == g_recordTypes.end()) break;
                        if (it5->second.methods.count(lo)) { defType = cur3; break; }
                        cur3 = it5->second.parent;
                    }
                    // Use virtual dispatch if any child type overrides this method
                    // with same signature
                    bool hasOverride = false;
                    for (auto& [tn, td] : g_recordTypes) {
                        if (tn == defType) continue;
                        std::string anc = td.parent;
                        while (!anc.empty()) {
                            if (anc == defType) {
                                auto mit2 = td.methods.find(lo);
                                if (mit2 != td.methods.end() &&
                                    mit2->second->params.size() == pd3->params.size())
                                    hasOverride = true;
                                break;
                            }
                            auto ait = g_recordTypes.find(anc);
                            if (ait == g_recordTypes.end()) break;
                            anc = ait->second.parent;
                        }
                        if (hasOverride) break;
                    }
                    if (hasOverride)
                        return "pas_" + defType + "_" + lo + "__virt(self)";
                    return "pas_" + defType + "_" + lo + "(self)";
                }
            }
            // Zero-argument graphics functions arrive as VarNodes
            static const std::set<std::string> zeroArgGfx = {
                "graphresult","getmaxx","getmaxy","getx","gety",
                "getcolor","getbkcolor","readkey",
                "keypressed","gettickcount","donewincrt","clrscr"
            };
            if (zeroArgGfx.count(lo))
                return genCall(lo, {}, true);
            // Zero-arg user function called without parens (e.g. while do_main_menu do)
            if (userProcs.count(lo)) {
                auto pit = allProcs.find(lo);
                if (pit != allProcs.end() && pit->second->is_function && pit->second->params.empty())
                    return "pas_" + lo + "()";
            }
            // Dereference by-ref (var) params when reading
            if (byRefParams.count(lo))
                return "(*" + safeId(n->name) + ")";
            return safeId(n->name);
        }
        if (auto* n = dynamic_cast<IndexNode*>(node.get())) {
            auto* vn = dynamic_cast<VarNode*>(n->array.get());
            if (!vn) {
                // Chained index: e.g. Name[i][k] — inner result is a string, outer is char access
                // Generate inner expression then index into it as a char array
                auto* innerIdx = dynamic_cast<IndexNode*>(n->array.get());
                if (innerIdx) {
                    std::string inner = genExpr(n->array); // e.g. pas_name[i - pas_name_lo]
                    std::string idx2  = genExpr(n->index);
                    // inner[k-1] gives the char; wrap as (char[]){ch, 0} for string param context
                    return "(char[]){" + inner + "[" + idx2 + " - 1], 0}";
                }
                return "/*bad index*/0";
            }
            std::string base = safeId(vn->name);
            if (n->indices.size() > 1) {
                // 2D array: m[row, col] -> m[(row-lo0)*cols + (col-lo1)]
                // We need the dimension info from varTypes or g_recordTypes
                // Use lo vars: base_lo0, base_lo1 — but we only emit base_lo
                // Simple approach: look up dims from global VarDecl info
                // For now encode as (row-lo)*cols + (col-lo) where cols is known at compile time
                // We store dim info in a map during genDecl
                std::string row = genExpr(n->indices[0]);
                std::string col = genExpr(n->indices[1]);
                // Look up in arrayDims map
                std::string loname = base;
                std::transform(loname.begin(), loname.end(), loname.begin(), ::tolower);
                // Remove pas_ prefix for lookup
                std::string bareBase = loname.substr(4); // strip "pas_"
                auto dit = arrayDims.find(bareBase);
                if (dit != arrayDims.end() && dit->second.size() >= 2) {
                    int lo0 = dit->second[0].lo, lo1 = dit->second[1].lo;
                    int cols = dit->second[1].hi - dit->second[1].lo + 1;
                    return base + "[(" + row + " - " + std::to_string(lo0) + ") * " +
                           std::to_string(cols) + " + (" + col + " - " + std::to_string(lo1) + ")]";
                }
                // Fallback
                return base + "[" + row + " - " + base + "_lo]";
            }
            std::string idx  = genExpr(n->index);
            // For string variables, lower bound is always 1
            std::string bareIdx = base.substr(4); // strip "pas_"
            auto vtIt = varTypes.find(bareIdx);
            std::string vt = (vtIt != varTypes.end()) ? vtIt->second : "";
            std::transform(vt.begin(), vt.end(), vt.begin(), ::tolower);
            if (vt == "string" || vt == "char*") {
                return base + "[" + idx + " - 1]";
            }
            return base + "[" + idx + " - " + base + "_lo]";
        }
        if (auto* n = dynamic_cast<UnaryOpNode*>(node.get())) {
            if (n->op == "-") return "(-(" + genExpr(n->operand) + "))";
            if (n->op == "not") return "(!(" + genExpr(n->operand) + "))";
        }
        if (auto* n = dynamic_cast<BinOpNode*>(node.get())) {
            // String concatenation: collect all + operands and emit pas_concat_str
            if (n->op == "+") {
                bool lStr = isStrExpr(n->left), rStr = isStrExpr(n->right);
                if (lStr || rStr) {
                    std::vector<std::string> parts;
                    std::function<void(const ASTPtr&)> collect = [&](const ASTPtr& nd) {
                        if (auto* b = dynamic_cast<BinOpNode*>(nd.get()); b && b->op == "+")
                            { collect(b->left); collect(b->right); }
                        else {
                            // Wrap char literals in a string
                            if (dynamic_cast<CharNode*>(nd.get())) {
                                // genExpr returns '\\x' — wrap in (char[]){x, 0}
                                std::string ce = genExpr(nd);
                                parts.push_back("(char[]){" + ce + ", 0}");
                            } else parts.push_back(genExpr(nd));
                        }
                    };
                    collect(n->left); collect(n->right);
                    std::string call = "pas_concat_str(" + std::to_string(parts.size());
                    for (auto& p : parts) call += ", " + p;
                    return call + ")";
                }
            }
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
            if (n->op == "=") {
                if (isStrExpr(n->left) || isStrExpr(n->right))
                    return "(strncmp(" + l + ", " + r + ", 1023) == 0)";
                return "(" + l + " == " + r + ")";
            }
            if (n->op == "<>") {
                if (isStrExpr(n->left) || isStrExpr(n->right))
                    return "(strncmp(" + l + ", " + r + ", 1023) != 0)";
                return "(" + l + " != " + r + ")";
            }
            if (n->op == "<")   return "(" + l + " < "  + r + ")";
            if (n->op == "<=")  return "(" + l + " <= " + r + ")";
            if (n->op == ">")   return "(" + l + " > "  + r + ")";
            if (n->op == ">=")  return "(" + l + " >= " + r + ")";
        }
        if (auto* n = dynamic_cast<SetMemberNode*>(node.get())) {
            // expr in [a, b, c]  ->  (expr==a || expr==b || expr==c)
            std::string lhs = genExpr(n->expr);
            if (n->elements.empty()) return "0";
            std::string result = "(";
            for (size_t si = 0; si < n->elements.size(); si++) {
                if (si) result += " || ";
                result += "(" + lhs + " == " + genExpr(n->elements[si]) + ")";
            }
            return result + ")";
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
        if (userProcs.count(name) || nestedCallNames.count(name)) {
            // Find the ProcDef to get param types for char->string promotion
            ProcDef* pd2 = nullptr;
            auto allIt = allProcs.find(name);
            if (allIt != allProcs.end()) pd2 = allIt->second;
            // Use mangled name if this is a nested proc call
            std::string callName = nestedCallNames.count(name) ? nestedCallNames.at(name) : safeId(raw);
            bool isNested = nestedCallNames.count(name) > 0;
            std::string call = callName + "(";
            for (size_t i = 0; i < args.size(); i++) {
                if (i) call += ", ";
                // Promote char literal to char* when param expects string
                bool needStr = false;
                if (pd2 && i < pd2->params.size()) {
                    std::string pt = pd2->params[i].type;
                    std::transform(pt.begin(),pt.end(),pt.begin(),::tolower);
                    needStr = (pt == "string");
                }
                if (needStr) {
                    if (auto* cn = dynamic_cast<CharNode*>(args[i].get())) {
                        char cv = cn->value;
                        char buf[32];
                        if (cv == '\'')      snprintf(buf,sizeof(buf),"(char[]){'\\'', 0}");
                        else if (cv == '\\') snprintf(buf,sizeof(buf),"(char[]){'\\\\', 0}");
                        else                 snprintf(buf,sizeof(buf),"(char[]){'%c', 0}",cv);
                        call += buf;
                        continue;
                    }
                    // char variable passed to string param: wrap as (char[]){var, 0}
                    if (auto* vn2 = dynamic_cast<VarNode*>(args[i].get())) {
                        std::string vlo2 = vn2->name;
                        std::transform(vlo2.begin(),vlo2.end(),vlo2.begin(),::tolower);
                        auto vit = varTypes.find(vlo2);
                        if (vit == varTypes.end()) vit = varTypes.find("self."+vlo2);
                        if (vit != varTypes.end()) {
                            std::string vt = vit->second;
                            std::transform(vt.begin(),vt.end(),vt.begin(),::tolower);
                            if (vt == "char") {
                                call += "(char[]){" + genExpr(args[i]) + ", 0}";
                                continue;
                            }
                        }
                    }
                }
                // Pass by-ref params with &
                bool byRef = pd2 && i < pd2->params.size() && pd2->params[i].by_ref;
                std::string ptlo2;
                if (pd2 && i < pd2->params.size()) {
                    ptlo2 = pd2->params[i].type;
                    std::transform(ptlo2.begin(),ptlo2.end(),ptlo2.begin(),::tolower);
                }
                if (byRef && ptlo2 != "string") {
                    // var param: pass address, unless it's a string (already char*)
                    call += "&" + genExpr(args[i]);
                } else {
                    call += genExpr(args[i]);
                }
            }
            // Pass _outer_ret_ and captured outer vars to nested procs
            if (isNested) {
                if (!args.empty()) call += ", ";
                if (!currentFunc.empty()) call += "&_ret_";
                else call += "NULL";
                // Pass outer captured vars
                auto oIt = nestedOuterVars.find(callName);
                if (oIt != nestedOuterVars.end()) {
                    for (auto& [vn, vt] : oIt->second)
                        call += ", " + vn;
                }
            }
            return call + ")";
        }

        // Check nested procs not in userProcs (they are local to parent)
        if (nestedCallNames.count(name)) {
            std::string call = nestedCallNames.at(name) + "(";
            for (size_t i = 0; i < args.size(); i++) { if(i) call+=", "; call+=genExpr(args[i]); }
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
            // pos(needle, haystack) -> 1-based index or 0
            // needle may be a char literal — wrap it as a string
            std::string needle = genExpr(args[0]);
            std::string hay    = genExpr(args[1]);
            // If needle is a char literal (single-quoted), make it a string for strstr
            if (!needle.empty() && needle.front() == '\'')
                needle = "(char[]){" + needle + ", 0}";
            return "(strstr(" + hay + ", " + needle + ") ? "
                   "(long long)(strstr(" + hay + ", " + needle + ") - (" + hay + ")) + 1LL : 0LL)";
        }
        if (name == "str") {
            // str(value, width, var) — write formatted int to char buffer
            // args: [value, width, varptr] — varptr is passed by ref
            std::string val  = genExpr(args[0]);
            std::string wid  = (args.size() > 2) ? genExpr(args[1]) : "0LL";
            std::string dest = (args.size() > 2) ? genExpr(args[2]) : genExpr(args[1]);
            emitind("pas_str((long long)(" + val + "), " + wid + ", " + dest + ");");
            return "";
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
            std::string rhs = genExpr(n->value);
            // Array / string element assignment
            if (auto* idx = dynamic_cast<IndexNode*>(n->target.get())) {
                auto* vn = dynamic_cast<VarNode*>(idx->array.get());
                if (!vn) { emitind("/* bad index target */"); return; }
                std::string base = safeId(vn->name);
                // Check array element type and variable type
                std::string vnlo = vn->name; std::transform(vnlo.begin(),vnlo.end(),vnlo.begin(),::tolower);
                // Check if it's an array-of-string element: Name[i] := str
                auto aeit = arrayElemType.find(vnlo);
                if (aeit != arrayElemType.end()) {
                    std::string et = aeit->second; std::transform(et.begin(),et.end(),et.begin(),::tolower);
                    if (et == "string") {
                        // 2D array-of-string: Name[i,j] := str
                        if (idx->indices.size() > 1) {
                            auto dit2 = arrayDims.find(vnlo);
                            if (dit2 != arrayDims.end() && dit2->second.size() >= 2) {
                                int lo0 = dit2->second[0].lo, lo1 = dit2->second[1].lo;
                                int cols = dit2->second[1].hi - dit2->second[1].lo + 1;
                                std::string row = genExpr(idx->indices[0]);
                                std::string col = genExpr(idx->indices[1]);
                                std::string flatIdx = "(" + row + " - " + std::to_string(lo0) + ") * " +
                                                     std::to_string(cols) + " + (" + col + " - " + std::to_string(lo1) + ")";
                                emitind("strncpy(" + base + "[" + flatIdx + "], " + rhs + ", 1023);");
                                return;
                            }
                        }
                        // 1D array-of-string: strncpy(Name[i-lo], rhs, 1023)
                        emitind("strncpy(" + base + "[" + genExpr(idx->index) + " - " + base + "_lo], " + rhs + ", 1023);");
                        return;
                    }
                }
                // String character assignment: s[i] := char -> s[i-1] = char
                auto vtit = varTypes.find(vnlo);
                if (vtit != varTypes.end()) {
                    std::string vt = vtit->second; std::transform(vt.begin(),vt.end(),vt.begin(),::tolower);
                    if (vt == "string") {
                        emitind(base + "[" + genExpr(idx->index) + " - 1] = (char)(" + rhs + ");");
                        return;
                    }
                }
                // 2D array
                if (idx->indices.size() > 1) {
                    auto dit2 = arrayDims.find(vnlo);
                    if (dit2 != arrayDims.end() && dit2->second.size() >= 2) {
                        int lo0 = dit2->second[0].lo, lo1 = dit2->second[1].lo;
                        int cols = dit2->second[1].hi - dit2->second[1].lo + 1;
                        std::string row = genExpr(idx->indices[0]);
                        std::string col = genExpr(idx->indices[1]);
                        emitind(base + "[(" + row + " - " + std::to_string(lo0) + ") * " +
                               std::to_string(cols) + " + (" + col + " - " + std::to_string(lo1) + ")] = " + rhs + ";");
                        return;
                    }
                }
                // 1D array
                std::string idx1 = genExpr(idx->index);
                // If element type is string, use strncpy
                {
                    std::string elo2 = "";
                    auto evit = varTypes.find(vnlo);
                    // Look up array element type — stored as type name in varTypes... not directly.
                    // Check if rhs is a string literal: use strncpy
                    bool rhsIsStrLit = dynamic_cast<StringNode*>(n->value.get()) != nullptr;
                    if (rhsIsStrLit) {
                        // Check if this is a char array (single char element)
                        // or string array (char* element) — check rhs length
                        // For safety, use strncpy for multi-char string literals
                        std::string el_expr = base + "[" + idx1 + " - " + base + "_lo]";
                        emitind("strncpy(" + el_expr + ", " + rhs + ", 1023);");
                    } else {
                        emitind(base + "[" + idx1 + " - " + base + "_lo] = " + rhs + ";");
                    }
                }
            } else if (dynamic_cast<FieldAccessNode*>(n->target.get())) {
                // Record field assignment
                std::string lhs = genExpr(n->target);
                std::string rhs = genExpr(n->value);
                // String fields need strcpy
                bool isStr = dynamic_cast<StringNode*>(n->value.get()) != nullptr;
                if (!isStr) {
                    // Check if it's a string variable
                    if (auto* vn2 = dynamic_cast<VarNode*>(n->value.get())) {
                        std::string vlo2 = vn2->name;
                        std::transform(vlo2.begin(), vlo2.end(), vlo2.begin(), ::tolower);
                        auto it2 = varTypes.find(vlo2);
                        if (it2 != varTypes.end()) {
                            std::string t2 = it2->second;
                            std::transform(t2.begin(), t2.end(), t2.begin(), ::tolower);
                            if (t2 == "string") isStr = true;
                        }
                    }
                }
                if (isStr)
                    emitind("strncpy(" + lhs + ", " + rhs + ", 1023);");
                else
                    emitind(lhs + " = " + rhs + ";");
            } else if (auto* vn = dynamic_cast<VarNode*>(n->target.get())) {
                std::string vlo = vn->name;
                std::transform(vlo.begin(), vlo.end(), vlo.begin(), ::tolower);
                std::string cf = currentFunc;
                std::transform(cf.begin(), cf.end(), cf.begin(), ::tolower);
                std::string lhs;
                // Check if assigning to outer function return variable
                bool isOuterRet = false;
                for (auto& ofn : outerFuncRetNames) {
                    if (!ofn.empty() && vlo == ofn && vlo != cf) { isOuterRet = true; break; }
                }
                if (isOuterRet) {
                    emitind("if (_outer_ret_) *((int*)_outer_ret_) = (int)(" + rhs + ");");
                    return;
                }
                // Compute short function name (strip type prefix for methods)
                std::string lhsCf = currentFunc;
                std::transform(lhsCf.begin(), lhsCf.end(), lhsCf.begin(), ::tolower);
                auto cfDot = lhsCf.find('.');
                std::string lhsShortCf = (cfDot != std::string::npos) ? lhsCf.substr(cfDot+1) : lhsCf;
                if (!currentFunc.empty() && (vlo == lhsCf || vlo == lhsShortCf))
                    lhs = "_ret_";
                else if (!currentMethodType.empty() && varTypes.count("self." + vlo) && !varTypes.count(vlo) && vlo != lhsShortCf)
                    lhs = "self->pas_" + vlo;
                else
                    lhs = safeId(vn->name);
                // String assignments need strncpy
                std::string rhsExpr = genExpr(n->value);
                bool rhsIsStr = dynamic_cast<StringNode*>(n->value.get()) != nullptr;
                if (!rhsIsStr) {
                    // Check if rhs is a string type variable or field
                    auto checkStrType = [&](const std::string& t) {
                        std::string tl = t; std::transform(tl.begin(),tl.end(),tl.begin(),::tolower);
                        return tl == "string";
                    };
                    if (auto* vn2 = dynamic_cast<VarNode*>(n->value.get())) {
                        std::string rlo = vn2->name; std::transform(rlo.begin(),rlo.end(),rlo.begin(),::tolower);
                        if (varTypes.count(rlo) && checkStrType(varTypes[rlo])) rhsIsStr = true;
                        else if (varTypes.count("self."+rlo) && checkStrType(varTypes["self."+rlo])) rhsIsStr = true;
                    }
                    if (auto* fa = dynamic_cast<FieldAccessNode*>(n->value.get())) {
                        if (checkStrType(getFieldType(fa))) rhsIsStr = true;
                    }
                }
                bool lhsIsStr = false;
                if (lhs == "_ret_") {
                    // Check function return type
                    auto it2 = varTypes.find(lhsCf + "__ret");
                    if (it2 != varTypes.end()) {
                        std::string t = it2->second; std::transform(t.begin(),t.end(),t.begin(),::tolower);
                        lhsIsStr = (t == "string");
                    }
                } else if (!currentMethodType.empty() && varTypes.count("self."+vlo)) {
                    std::string t = varTypes["self."+vlo]; std::transform(t.begin(),t.end(),t.begin(),::tolower);
                    lhsIsStr = (t == "string");
                } else if (varTypes.count(vlo)) {
                    std::string t = varTypes[vlo]; std::transform(t.begin(),t.end(),t.begin(),::tolower);
                    lhsIsStr = (t == "string");
                }
                if (lhsIsStr || rhsIsStr)
                    emitind("strncpy(" + lhs + ", " + rhsExpr + ", 1023);");
                else if (byRefParams.count(vlo))
                    emitind("*" + lhs + " = " + rhsExpr + ";");
                else
                    emitind(lhs + " = " + rhsExpr + ";");
                return;
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
        if (auto* n = dynamic_cast<CaseNode*>(node.get())) {
            // case expr of val1: stmt; val2: stmt; ... [otherwise stmt] end
            std::string expr = genExpr(n->expr);
            bool first = true;
            for (auto& branch : n->branches) {
                std::string cond;
                for (size_t vi = 0; vi < branch.values.size(); vi++) {
                    if (vi) cond += " || ";
                    cond += "(" + expr + " == " + genExpr(branch.values[vi]) + ")";
                }
                emitind((first ? "if (" : "} else if (") + cond + ") {");
                first = false;
                indent++;
                auto* cb = dynamic_cast<CompoundNode*>(branch.body.get());
                if (cb) { for (auto& s : cb->statements) genStmt(s); }
                else genStmt(branch.body);
                indent--;
            }
            if (n->else_branch) {
                emitind("} else {");
                indent++;
                auto* eb = dynamic_cast<CompoundNode*>(n->else_branch.get());
                if (eb) { for (auto& s : eb->statements) genStmt(s); }
                else genStmt(n->else_branch);
                indent--;
            }
            if (!n->branches.empty() || n->else_branch)
                emitind("}");
            return;
        }
        if (auto* n = dynamic_cast<WriteNode*>(node.get())) {
            for (size_t ai = 0; ai < n->args.size(); ++ai) {
                auto& arg = n->args[ai];
                int fw = (ai < n->widths.size()) ? n->widths[ai] : -1;
                int fd = (ai < n->decimals.size()) ? n->decimals[ai] : -1;
                std::string e = genExpr(arg);
                // Helper lambdas that emit formatted print calls
                auto emitFmtD = [&](const std::string& expr) {
                    if (fd >= 0)
                        emitind("pas_print_fmt_d((double)(" + expr + ")," + std::to_string(fw < 0 ? 0 : fw) + "," + std::to_string(fd) + ");");
                    else if (fw > 0)
                        emitind("{ char _b[64]; int _n=snprintf(_b,sizeof(_b),\"%g\",(double)(" + expr + ")); if(" + std::to_string(fw) + ">_n){char _p[64];snprintf(_p,sizeof(_p),\"%*g\"," + std::to_string(fw) + ",(double)(" + expr + "));write(1,_p,strlen(_p));}else write(1,_b,_n); }");
                    else
                        emitind("pas_print_d((double)(" + expr + "));");
                };
                auto emitFmtLL = [&](const std::string& expr) {
                    if (fw > 0)
                        emitind("pas_print_fmt_ll((long long)(" + expr + ")," + std::to_string(fw) + ");");
                    else
                        emitind("pas_print_ll((long long)(" + expr + "));");
                };
                if (dynamic_cast<StringNode*>(arg.get()))
                    emitind("pas_print_s(" + e + ");");
                else if (dynamic_cast<CharNode*>(arg.get()))
                    emitind("pas_print_c(" + e + ");");
                else if (auto* num = dynamic_cast<NumberNode*>(arg.get())) {
                    if (num->is_int) emitFmtLL(e);
                    else emitFmtD(e);
                } else if (auto* fa = dynamic_cast<FieldAccessNode*>(arg.get())) {
                    // Record field or zero-arg method call
                    std::string ftype = getFieldType(fa);
                    if (ftype.empty()) {
                        // Not a field — might be a zero-arg method call
                        std::string objType = getExprTypeName(fa->record);
                        std::transform(objType.begin(), objType.end(), objType.begin(), ::tolower);
                        std::string mlo = fa->field;
                        std::transform(mlo.begin(), mlo.end(), mlo.begin(), ::tolower);
                        auto pd2 = findMethod(objType, mlo);
                        if (pd2 && pd2->is_function) {
                            std::string rt = pd2->return_type;
                            std::transform(rt.begin(), rt.end(), rt.begin(), ::tolower);
                            if (rt == "real" || rt == "double" || rt == "single") emitFmtD(e);
                            else if (rt == "string") emitind("pas_print_s(" + e + ");");
                            else if (rt == "char") emitind("pas_print_c((char)(" + e + "));");
                            else emitFmtLL(e);
                        } else {
                            emitind("PAS_PRINT(" + e + ");");
                        }
                    } else {
                        std::transform(ftype.begin(), ftype.end(), ftype.begin(), ::tolower);
                        if (ftype == "real" || ftype == "double" || ftype == "single") emitFmtD(e);
                        else if (ftype == "char") emitind("pas_print_c((char)(" + e + "));");
                        else if (ftype == "string") emitind("pas_print_s(" + e + ");");
                        else emitFmtLL(e);
                    }
                } else if (auto* vn = dynamic_cast<VarNode*>(arg.get())) {
                    // Variable — look up type (also handle zero-arg self methods)
                    std::string lo = vn->name;
                    std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                    auto it = varTypes.find(lo);
                    if (it == varTypes.end() && !currentMethodType.empty())
                        it = varTypes.find("self." + lo);
                    std::string vtype = (it != varTypes.end()) ? it->second : "";
                    // If not a field, check if it's a zero-arg self method
                    if (vtype.empty() && !currentMethodType.empty()) {
                        auto pd3 = findMethod(currentMethodType, lo);
                        if (pd3 && pd3->is_function) vtype = pd3->return_type;
                    }
                    std::transform(vtype.begin(), vtype.end(), vtype.begin(), ::tolower);
                    if (vtype == "real" || vtype == "double" || vtype == "single") emitFmtD(e);
                    else if (vtype == "char") {
                        // Array of char is a string; scalar char is a single character
                        if (arrayElemType.count(lo)) emitind("pas_print_s(" + e + ");");
                        else emitind("pas_print_c((char)(" + e + "));");
                    }
                    else if (vtype == "string") emitind("pas_print_s(" + e + ");");
                    else emitFmtLL(e);
                } else if (auto* mc = dynamic_cast<MethodCallNode*>(arg.get())) {
                    // Method call — look up return type
                    std::string typeName = getExprTypeName(mc->object);
                    std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);
                    auto pd2 = findMethod(typeName, mc->method);
                    if (pd2 && pd2->is_function) {
                        std::string rt = pd2->return_type;
                        std::transform(rt.begin(), rt.end(), rt.begin(), ::tolower);
                        if (rt == "real" || rt == "double" || rt == "single") emitFmtD(e);
                        else if (rt == "string") emitind("pas_print_s(" + e + ");");
                        else if (rt == "char") emitind("pas_print_c((char)(" + e + "));");
                        else emitFmtLL(e);
                    } else {
                        emitind("PAS_PRINT(" + e + ");");
                    }
                } else {
                    // BinOp, IndexNode, CallNode, or other expression
                    // Check if it's a char/string array element
                    bool isCharElem = false, isStringElem = false;
                    if (auto* idx = dynamic_cast<IndexNode*>(arg.get())) {
                        if (auto* vn2 = dynamic_cast<VarNode*>(idx->array.get())) {
                            std::string vlo2 = vn2->name;
                            std::transform(vlo2.begin(),vlo2.end(),vlo2.begin(),::tolower);
                            // Check arrayElemType first
                            auto aeit2 = arrayElemType.find(vlo2);
                            if (aeit2 != arrayElemType.end()) {
                                std::string et2 = aeit2->second;
                                std::transform(et2.begin(),et2.end(),et2.begin(),::tolower);
                                if (et2 == "char") isCharElem = true;
                                else if (et2 == "string") isStringElem = true;
                            } else {
                                // Fall back to varTypes
                                auto vit2 = varTypes.find(vlo2);
                                if (vit2 != varTypes.end()) {
                                    std::string vt2 = vit2->second;
                                    std::transform(vt2.begin(),vt2.end(),vt2.begin(),::tolower);
                                    // string[i] yields a char
                                    if (vt2 == "char") isCharElem = true;
                                    else if (vt2 == "string") isCharElem = true;
                                }
                            }
                        }
                    }
                    if (isStringElem)
                        emitind("pas_print_s(" + e + ");");
                    else if (isCharElem)
                        emitind("pas_print_c((char)(" + e + "));");
                    else if (isRealExpr(arg))
                        emitFmtD(e);
                    else {
                        // Check if it's a call to a char-returning function
                        bool isCharCall = false;
                        if (auto* cn = dynamic_cast<CallNode*>(arg.get())) {
                            std::string clo = cn->name;
                            std::transform(clo.begin(),clo.end(),clo.begin(),::tolower);
                            auto pit = allProcs.find(clo);
                            if (pit != allProcs.end()) {
                                std::string rt = pit->second->return_type;
                                std::transform(rt.begin(),rt.end(),rt.begin(),::tolower);
                                if (rt == "char") isCharCall = true;
                            }
                            // Built-in char functions
                            if (clo == "chr" || clo == "upcase" || clo == "lowercase")
                                isCharCall = true;
                        }
                        if (isCharCall)
                            emitind("pas_print_c((char)(" + e + "));");
                        else
                            emitFmtLL(e);
                    }
                }
            }
            if (n->newline) emitind("write(1,\"\\n\",1);");
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
                    bool isByRef = byRefParams.count(lo) > 0;
                    std::string addr = isByRef ? safeId(vn->name) : ("&" + safeId(vn->name));
                    if (vtype == "real" || vtype == "double" || vtype == "single")
                        emitind("scanf(\"%lf\", " + addr + ");");
                    else if (vtype == "char")
                        emitind("scanf(\" %c\", " + addr + ");");
                    else if (vtype == "string")
                        emitind("scanf(\"%1023s\", " + safeId(vn->name) + ");");
                    else
                        emitind("scanf(\"%lld\", " + addr + ");");
                }
            }
            if (n->newline) emitind("{ char _nl[256]; fgets(_nl, sizeof(_nl), stdin); }");
            return;
        }
        if (auto* n = dynamic_cast<MethodCallNode*>(node.get())) {
            emitind(genMethodCall(n) + ";");
            return;
        }
        if (auto* n = dynamic_cast<CallNode*>(node.get())) {
            std::string name = n->name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name == "randomize") { emitind("srand((unsigned)time(NULL));"); return; }
            if (name == "halt") { emitind("exit(0);"); return; }
            if (name == "exit") { // Pascal exit = return from current function
                if (!currentFunc.empty()) emitind("return _ret_;");
                else emitind("return;");
                return;
            }
            if (name == "clrscr")   { emitind("pas_clrscr();"); return; }
            if (name == "gotoxy")   { emitind("pas_gotoxy(" + genExpr(n->args[0]) + ", " + genExpr(n->args[1]) + ");"); return; }
            if (name == "donewincrt"){ emitind("exit(0);"); return; }
            if (name == "str") {
                // str(val, width, dest) or str(val, dest)
                std::string val  = genExpr(n->args[0]);
                std::string wid  = (n->args.size() > 2) ? genExpr(n->args[1]) : "0LL";
                std::string dest = (n->args.size() > 2) ? genExpr(n->args[2]) : genExpr(n->args[1]);
                emitind("pas_str((long long)(" + val + "), " + wid + ", " + dest + ");");
                return;
            }
            if (name == "insert") {
                // insert(src, dest, pos)
                emitind("{ char* _ins=" + genExpr(n->args[0]) + "; char* _dst=" + genExpr(n->args[1]) +
                        "; int _p=(int)(" + genExpr(n->args[2]) + ")-1; int _dl=strlen(_dst), _sl=strlen(_ins);"
                        " if(_p>=0&&_p<=_dl&&_dl+_sl<1023){memmove(_dst+_p+_sl,_dst+_p,_dl-_p+1);memcpy(_dst+_p,_ins,_sl);} }");
                return;
            }
            if (name == "delete") {
                // delete(dest, pos, len)
                emitind("{ char* _dst=" + genExpr(n->args[0]) + "; int _p=(int)(" + genExpr(n->args[1]) +
                        ")-1, _l=(int)(" + genExpr(n->args[2]) + "); int _dl=strlen(_dst);"
                        " if(_p>=0&&_p<_dl){memmove(_dst+_p,_dst+_p+_l,_dl-_p-_l+1);} }");
                return;
            }
            std::string result = genCall(n->name, n->args, false);
            if (!result.empty()) emitind(result + ";");
            return;
        }
        if (auto* n = dynamic_cast<GotoNode*>(node.get())) {
            emitind("goto lbl_" + n->label + ";");
            return;
        }
        if (auto* n = dynamic_cast<LabelNode*>(node.get())) {
            // Emit label at current indent (labels in C can't be at end of block without stmt)
            out << std::string(indent > 0 ? (indent-1)*4 : 0, ' ') << "lbl_" << n->label << ":;\n";
            if (n->stmt) genStmt(n->stmt);
            return;
        }
    }

    // ── Declaration codegen ───────────────────
    // Emit a C struct definition for a record type (once per type name)
    void emitRecordStruct(const std::string& typeName,
                          const std::vector<std::pair<std::string,std::string>>& fields) {
        std::string sname = "struct pas_rec_" + typeName;
        emitind(sname + " {");
        indent++;
        emitind("char __type__[64];"); // runtime type tag for virtual dispatch
        for (auto& [fn, ft] : fields) {
            std::string ct = cType(ft);
            std::string fid = "pas_" + fn;
            std::transform(fid.begin(), fid.end(), fid.begin(), ::tolower);
            if (ct == "char*")
                emitind("char " + fid + "[1024];");
            else
                emitind(ct + " " + fid + ";");
        }
        indent--;
        emitind("};");
    }

    std::set<std::string> emittedStructs; // avoid duplicate struct defs
    std::set<std::string> globalVarNames;  // names of global variables
    std::vector<std::pair<std::string,std::string>> globalObjVars; // (id, typeName)
    std::map<std::string, std::vector<ArrayDim>> arrayDims; // var name -> dims for multi-dim arrays
    std::map<std::string, std::string> arrayElemType; // var name -> element type
    std::map<std::string, ProcDef*> allProcs; // all procs for param type lookup
    std::string nestedPrefix; // prefix for lifted nested proc names
    std::map<std::string,std::string> nestedCallNames; // short name -> mangled name for current scope
    std::vector<std::string> outerFuncRetNames; // stack of outer function return var names
    // Maps mangled nested proc name -> list of (C_var_name, C_type) for outer captured vars
    std::map<std::string, std::vector<std::pair<std::string,std::string>>> nestedOuterVars;

    void genDecl(const ASTPtr& node, bool global) {
        if (auto* vd = dynamic_cast<VarDecl*>(node.get())) {
            for (auto& nm : vd->names) {
                std::string id = safeId(nm);
                std::string lo = nm; std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
                varTypes[lo] = vd->type_name;
                if (global) globalVarNames.insert(lo);
                if (vd->is_record) {
                    // Emit the struct type if not already done
                    std::string tlo = vd->type_name;
                    std::transform(tlo.begin(), tlo.end(), tlo.begin(), ::tolower);
                    if (!emittedStructs.count(tlo)) {
                        emittedStructs.insert(tlo);
                        emitRecordStruct(tlo, vd->record_fields);
                    }
                    emitind("struct pas_rec_" + tlo + " " + id + " = {0};");
                    if (global) globalObjVars.push_back({id, tlo});
                } else if (vd->is_array) {
                    int sz = vd->array_hi - vd->array_lo + 1;
                    std::string elemCT = cType(vd->type_name);
                    // array of string: char name[N][1024]
                    std::string etlo = vd->type_name;
                    std::transform(etlo.begin(), etlo.end(), etlo.begin(), ::tolower);
                    if (etlo == "string")
                        emitind("char " + id + "[" + std::to_string(sz) + "][1024];");
                    else if (etlo == "char")
                        // char arrays are used as strings: allocate as string buffer
                        emitind("char " + id + "[1024];");
                    else
                        emitind(elemCT + " " + id + "[" + std::to_string(sz) + "];");
                    emitind("const int " + id + "_lo = " + std::to_string(vd->array_lo) + ";");
                    // Store dimension and element type info
                    std::string bareId = id.substr(4); // strip "pas_"
                    if (!vd->array_dims.empty()) arrayDims[bareId] = vd->array_dims;
                    arrayElemType[bareId] = vd->type_name;
                } else {
                    std::string ct = cType(vd->type_name);
                    if (ct == "char*")
                        emitind("char " + id + "[1024] = \"\";");
                    else
                        emitind(ct + " " + id + " = 0;");
                }
            }
        } else if (auto* cd = dynamic_cast<ConstDecl*>(node.get())) {
            // Use char type for char constants to avoid comparison warnings
            if (dynamic_cast<CharNode*>(cd->value.get()))
                emitind("const char " + safeId(cd->name) + " = " + genExpr(cd->value) + ";");
            else
                emitind("const long long " + safeId(cd->name) + " = " + genExpr(cd->value) + ";");
        }
    }

    // ── Function/procedure forward declarations ──
    void genForwardDecl(const ASTPtr& node) {
        auto* pd = dynamic_cast<ProcDef*>(node.get());
        if (!pd) return;
        std::string ret = pd->is_function ? cType(pd->return_type) : "void";

        // Handle method: dotted name
        std::string procName = pd->name;
        std::string methType;
        {
            std::string lo = procName;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            auto dot = lo.find('.');
            if (dot != std::string::npos) {
                methType  = lo.substr(0, dot);
                procName  = lo.substr(0, dot) + "_" + lo.substr(dot+1);
            } else {
                procName = lo;  // lowercase non-method proc names
            }
        }
        std::string sig = ret + " pas_" + procName + "(";
        if (!methType.empty()) {
            sig += "struct pas_rec_" + methType + "* self";
            if (!pd->params.empty()) sig += ", ";
        }
        for (size_t i = 0; i < pd->params.size(); i++) {
            if (i) sig += ", ";
            std::string ct = cType(pd->params[i].type);
            std::string ptfwd = pd->params[i].type;
            std::transform(ptfwd.begin(),ptfwd.end(),ptfwd.begin(),::tolower);
            if (pd->params[i].by_ref && ptfwd != "string") ct += "*";
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

        // Save globals that params might shadow, clear non-global/non-self varTypes
        std::map<std::string,std::string> savedGlobals;
        for (auto it6 = varTypes.begin(); it6 != varTypes.end(); ) {
            const std::string& k = it6->first;
            if (k.substr(0,5) != "self." && !globalVarNames.count(k))
                it6 = varTypes.erase(it6);
            else ++it6;
        }

        // Record parameter types (may shadow globals — save them)
        byRefParams.clear();
        for (auto& p : pd->params) {
            std::string lo = p.name; std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            if (globalVarNames.count(lo)) savedGlobals[lo] = varTypes[lo];
            varTypes[lo] = p.type;
            // Track var (by-ref, non-string) params — they are pointers in C
            if (p.by_ref) {
                std::string ptlo = p.type; std::transform(ptlo.begin(),ptlo.end(),ptlo.begin(),::tolower);
                if (ptlo != "string") byRefParams.insert(lo);
            }
        }
        if (pd->is_function) {
            std::string lo = pd->name; std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            varTypes[lo + "__ret"] = pd->return_type;
        }

        // Check for method: name contains dot
        std::string procName = pd->name;
        std::string methodTypeName, methodFuncName;
        {
            std::string lo = procName;
            std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
            auto dot = lo.find('.');
            if (dot != std::string::npos) {
                methodTypeName = lo.substr(0, dot);
                methodFuncName = lo.substr(dot + 1);
                procName = methodTypeName + "_" + methodFuncName;
            } else if (!nestedPrefix.empty()) {
                // Nested proc — mangle: parentname__childname
                procName = nestedPrefix + "__" + lo;
            } else {
                procName = lo;
            }
        }
        currentMethodType = methodTypeName; // set so genExpr emits self->field

        std::string ret = pd->is_function ? cType(pd->return_type) : "void";
        std::string sig = ret + " pas_" + procName + "(";
        // For methods, add self pointer as first param
        if (!methodTypeName.empty()) {
            sig += "struct pas_rec_" + methodTypeName + "* self";
            if (!pd->params.empty()) sig += ", ";
            // Fields accessible as self->pas_field — record in varTypes with special prefix
            if (g_recordTypes.count(methodTypeName)) {
                for (auto& [fn, ft] : getAllFields(methodTypeName)) {
                    std::string flo = fn;
                    std::transform(flo.begin(), flo.end(), flo.begin(), ::tolower);
                    varTypes["self." + flo] = ft;
                }
            }
        }
        for (size_t i = 0; i < pd->params.size(); i++) {
            if (i && methodTypeName.empty()) sig += ", ";
            else if (i) sig += ", ";
            std::string ct = cType(pd->params[i].type);
            std::string ptdef = pd->params[i].type;
            std::transform(ptdef.begin(), ptdef.end(), ptdef.begin(), ::tolower);
            // String vars passed by ref are already char* (array), no extra *
            if (pd->params[i].by_ref && ptdef != "string") ct += "*";
            sig += ct + " " + safeId(pd->params[i].name);
        }
        // If this is a nested proc (has a prefix), add hidden _outer_ret_ param + outer captured vars
        if (!nestedPrefix.empty() && procName.find("__") != std::string::npos) {
            sig += (sig.back() == '(' ? std::string("") : std::string(", ")) + "void* _outer_ret_";
            std::string fullMangledName = "pas_" + procName;
            auto oIt = nestedOuterVars.find(fullMangledName);
            if (oIt != nestedOuterVars.end()) {
                for (auto& [vn, vt] : oIt->second)
                    sig += ", " + vt + " " + vn;
            }
        }
        sig += ")";

        // Emit nested proc/func definitions BEFORE this function (C doesn't allow nesting)
        std::string savedPrefix = nestedPrefix;
        auto savedNestedCallNames = nestedCallNames;
        // Track outer return var for nested proc assignment resolution
        if (pd->is_function) {
            std::string fnlo = pd->name; std::transform(fnlo.begin(),fnlo.end(),fnlo.begin(),::tolower);
            outerFuncRetNames.push_back(fnlo);
        } else {
            outerFuncRetNames.push_back("");
        }
        if (!methodTypeName.empty())
            nestedPrefix = methodTypeName + "_" + methodFuncName;
        else {
            // Accumulate prefix for deeply nested procs (grandparent__parent)
            // procName is already mangled at this point
            nestedPrefix = procName;
        }
        // Register nested proc names so calls get mangled
        for (auto& d : pd->decls) {
            if (auto* npd = dynamic_cast<ProcDef*>(d.get())) {
                std::string nlo = npd->name;
                std::transform(nlo.begin(), nlo.end(), nlo.begin(), ::tolower);
                nestedCallNames[nlo] = "pas_" + nestedPrefix + "__" + nlo;
            }
        }
        // Collect outer vars that nested procs may capture (params + scalar locals)
        {
            std::vector<std::pair<std::string,std::string>> outerVars;
            for (auto& p : pd->params) {
                std::string plo = p.name;
                std::transform(plo.begin(), plo.end(), plo.begin(), ::tolower);
                std::string ct = cType(p.type);
                std::string ptlo = p.type;
                std::transform(ptlo.begin(), ptlo.end(), ptlo.begin(), ::tolower);
                if (p.by_ref && ptlo != "string") ct += "*";
                outerVars.push_back({safeId(plo), ct});
            }
            for (auto& d2 : pd->decls) {
                if (auto* vd = dynamic_cast<VarDecl*>(d2.get())) {
                    if (!vd->is_array && !vd->is_record) {
                        for (auto& nm : vd->names) {
                            std::string nlo = nm;
                            std::transform(nlo.begin(), nlo.end(), nlo.begin(), ::tolower);
                            outerVars.push_back({safeId(nlo), cType(vd->type_name)});
                        }
                    }
                }
            }
            for (auto& d2 : pd->decls) {
                if (auto* npd = dynamic_cast<ProcDef*>(d2.get())) {
                    std::string nlo = npd->name;
                    std::transform(nlo.begin(), nlo.end(), nlo.begin(), ::tolower);
                    std::string mangledName = "pas_" + nestedPrefix + "__" + nlo;
                    // Build set of names the nested proc defines itself
                    std::set<std::string> ownNames;
                    for (auto& p : npd->params) {
                        std::string plo = p.name;
                        std::transform(plo.begin(), plo.end(), plo.begin(), ::tolower);
                        ownNames.insert(plo);
                    }
                    for (auto& d3 : npd->decls) {
                        if (auto* vd2 = dynamic_cast<VarDecl*>(d3.get())) {
                            for (auto& nm : vd2->names) {
                                std::string lo2 = nm;
                                std::transform(lo2.begin(), lo2.end(), lo2.begin(), ::tolower);
                                ownNames.insert(lo2);
                            }
                        }
                    }
                    nestedOuterVars[mangledName].clear();
                    for (auto& [vname, vtype] : outerVars) {
                        std::string bareId = vname.substr(4); // strip "pas_"
                        if (!ownNames.count(bareId))
                            nestedOuterVars[mangledName].push_back({vname, vtype});
                    }
                }
            }
        }
        for (auto& d : pd->decls) {
            if (dynamic_cast<ProcDef*>(d.get()))
                genProcDef(d);
        }
        outerFuncRetNames.pop_back();
        // Note: nestedPrefix and nestedCallNames restored AFTER body emission below

        emitind(sig);
        emitind("{");
        indent++;

        // Return variable for functions
        if (pd->is_function) {
            std::string rt = cType(pd->return_type);
            if (rt == "char*")
                emitind("static char _ret_[1024] = \"\";");
            else
                emitind(rt + " _ret_ = 0;");
        }

        // Local variable declarations only (nested procs already emitted above)
        for (auto& d : pd->decls) {
            if (!dynamic_cast<ProcDef*>(d.get()))
                genDecl(d, false);
        }

        // Body — emitted while nestedCallNames still active so calls get mangled
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

        // Now restore saved context
        nestedPrefix = savedPrefix;
        nestedCallNames = savedNestedCallNames;
        currentFunc = prevFunc;
        currentMethodType = "";
        // Restore any globals that were shadowed by params
        for (auto& [k,v] : savedGlobals) varTypes[k] = v;
        currentMethodType = "";
    }

public:
    bool usesGraphics = false;

    std::string compile(const ASTPtr& ast) {
        auto* prog = dynamic_cast<ProgramNode*>(ast.get());
        if (!prog) throw std::runtime_error("Not a program node");

        // Detect if program uses graphics functions by scanning all procedures
        {
            static const std::set<std::string> gfxFuncs = {
                "initgraph","line","lineto","circle","bar","arc","ellipse",
                "fillellipse","floodfill","setcolor","cleardevice","closegraph",
                "putpixel","drawpoly","fillpoly","setbkcolor","rectangle"
            };
            std::function<void(const std::vector<ASTPtr>&)> scanDecls;
            std::function<void(const ASTPtr&)> scanNode = [&](const ASTPtr& n) {
                if (!n) return;
                if (auto* c = dynamic_cast<CallNode*>(n.get())) {
                    std::string lo = c->name;
                    std::transform(lo.begin(),lo.end(),lo.begin(),::tolower);
                    if (gfxFuncs.count(lo)) usesGraphics = true;
                    for (auto& a : c->args) scanNode(a);
                } else if (auto* blk = dynamic_cast<CompoundNode*>(n.get())) {
                    for (auto& s : blk->statements) scanNode(s);
                } else if (auto* wh = dynamic_cast<WhileNode*>(n.get())) {
                    scanNode(wh->cond); scanNode(wh->body);
                } else if (auto* fi = dynamic_cast<IfNode*>(n.get())) {
                    scanNode(fi->cond); scanNode(fi->then_branch); scanNode(fi->else_branch);
                } else if (auto* fo = dynamic_cast<ForNode*>(n.get())) {
                    scanNode(fo->body);
                } else if (auto* re = dynamic_cast<RepeatNode*>(n.get())) {
                    scanNode(re->body);
                } else if (auto* pd = dynamic_cast<ProcDef*>(n.get())) {
                    scanDecls(pd->decls);
                    scanNode(pd->body);
                } else if (auto* as = dynamic_cast<AssignNode*>(n.get())) {
                    scanNode(as->value);
                }
            };
            scanDecls = [&](const std::vector<ASTPtr>& decls) {
                for (auto& d : decls) scanNode(d);
            };
            scanDecls(prog->declarations);
            scanNode(prog->body);
        }

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
        emitln("#include <unistd.h>");
        emitln("#include <time.h>");
        emitln("#include <sys/select.h>");
        emitln("#include <math.h>");
        emitln("#include <ctype.h>");
        emitln("#include <stdarg.h>");
        emitln();

        // Graphics — full SDL2 implementation inline, only when program uses graphics
        if (usesGraphics) {
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
        // SDL keypressed uses SDL event system
        if (!userProcs.count("keypressed"))
            emitln("static int pas_keypressed(void){SDL_PumpEvents();return SDL_HasEvent(SDL_KEYDOWN);}");
        // NOTE: gotoxy and clrscr are NOT defined here — they are defined after the #endif
        // so they work correctly for both SDL and non-SDL builds
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
        } // end if (usesGraphics)
        emitln();

        // ── Terminal stubs — only for non-graphics programs ──
        // Graphics programs use SDL versions defined above in the #ifdef HAVE_SDL2 block
        if (!usesGraphics) {
        if (!userProcs.count("readkey") || !userProcs.count("keypressed")) {
            emitln(R"(
#include <termios.h>
#include <unistd.h>
#ifndef _PAS_RAW_DEFINED
#define _PAS_RAW_DEFINED
static struct termios _pas_orig_tio;
static int _pas_raw_mode = 0;
static void _pas_restore_tio(void) { if(_pas_raw_mode){tcsetattr(0,0,&_pas_orig_tio);_pas_raw_mode=0;} }
static void _pas_set_raw(void) { if(_pas_raw_mode) return; tcgetattr(0,&_pas_orig_tio); atexit(_pas_restore_tio); struct termios t=_pas_orig_tio; t.c_lflag&=~(ECHO|ICANON); t.c_cc[VMIN]=1; t.c_cc[VTIME]=0; tcsetattr(0,0,&t); _pas_raw_mode=1; }
#endif
)");
        }
        if (!userProcs.count("readkey"))
            emitln(R"(static char pas_readkey(void) { _pas_set_raw(); char ch; if(read(0,&ch,1)!=1)return 0; if(ch==27){ struct timeval tv={0,50000}; fd_set f; FD_ZERO(&f); FD_SET(0,&f); char s[2]={0}; if(select(1,&f,0,0,&tv)>0&&read(0,s,1)==1&&s[0]=='['){ if(select(1,&f,0,0,&tv)>0&&read(0,s+1,1)==1){ if(s[1]=='A')return 1; if(s[1]=='B')return 2; if(s[1]=='C')return 4; if(s[1]=='D')return 3; } } return 27; } if(ch==10)ch=13; return ch; })");
        if (!userProcs.count("keypressed"))
            emitln("static inline int pas_keypressed(void){ _pas_set_raw(); fd_set _fds; struct timeval _tv={0,0}; FD_ZERO(&_fds); FD_SET(0,&_fds); return select(1,&_fds,0,0,&_tv)>0; }");
        if (!userProcs.count("clrscr"))
            emitln("static inline void pas_clrscr(void){ write(1,\"\\033[2J\\033[H\",7); }");
        if (!userProcs.count("gotoxy"))
            emitln("static inline void pas_gotoxy(long long x, long long y){ char buf[32]; int n=snprintf(buf,sizeof(buf),\"\\033[%lld;%lldH\",y,x); write(1,buf,n); }");
        } // end if (!usesGraphics)
        emitln("#include <time.h>");
        emitln("static inline long long pas_gettickcount(void){ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC,&_ts); return (long long)_ts.tv_sec*1000+_ts.tv_nsec/1000000; }");
        emitln("static void pas_str(long long n, long long w, char* out) { char tmp[64]; snprintf(tmp,sizeof(tmp),\"%lld\",n); long long len=(long long)strlen(tmp); if(w>len){long long pad=w-len; memmove(tmp+pad,tmp,len+1); for(int i=0;i<pad;i++)tmp[i]=' ';} strncpy(out,tmp,1023); }");
        emitln();

        // ── Runtime helpers ───────────────────
        emitln("/* Runtime helpers */");
        emitln("static void pas_print_ll(long long v)  { char b[32]; write(1,b,snprintf(b,sizeof(b),\"%lld\",v)); }");
        emitln("static void pas_print_d(double v)      { char b[64]; write(1,b,snprintf(b,sizeof(b),\"%g\",v)); }");
        emitln("static void pas_print_s(const char* v) { if(v)write(1,v,strlen(v)); }");
        emitln("static void pas_print_c(char v)        { write(1,&v,1); }");
        emitln("static void pas_print_val(double v)    { printf(\"%g\", v); }");
        emitln("static void pas_print_fmt_d(double v,int w,int d) { char b[64]; int n=snprintf(b,sizeof(b),\"%.*f\",d,v); if(w>n){char p[64];snprintf(p,sizeof(p),\"%*.*f\",w,d,v);write(1,p,strlen(p));}else write(1,b,n); }");
        emitln("static void pas_print_fmt_ll(long long v,int w) { char b[32]; int n=snprintf(b,sizeof(b),\"%lld\",v); if(w>n){char p[32];snprintf(p,sizeof(p),\"%*lld\",w,v);write(1,p,strlen(p));}else write(1,b,n); }");
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

        // ── Record type struct definitions ─────
        // Emit structs for all TYPE-declared records before forward declarations
        // so they're available for proc/func parameter types
        if (!g_recordTypes.empty()) {
            emitln("/* Record type definitions */");
            for (auto& [tname, tdef] : g_recordTypes) {
                if (!emittedStructs.count(tname)) {
                    emittedStructs.insert(tname);
                    emitRecordStruct(tname, getAllFields(tname));
                }
            }
            emitln();
        }

        // ── Forward declarations ──────────────
        emitln("/* Forward declarations */");
        for (auto& d : prog->declarations)
            if (dynamic_cast<ProcDef*>(d.get()))
                genForwardDecl(d);
        // Forward declare __virt dispatchers
        for (auto& [baseType, baseDef] : g_recordTypes) {
            for (auto& [methName, methPd] : baseDef.methods) {
                bool hasOverride = false;
                for (auto& [childType, childDef] : g_recordTypes) {
                    if (childType == baseType) continue;
                    std::string anc = childDef.parent;
                    while (!anc.empty()) {
                        if (anc == baseType) {
                            auto mit3 = childDef.methods.find(methName);
                            if (mit3 != childDef.methods.end() &&
                                mit3->second->params.size() == methPd->params.size())
                                hasOverride = true;
                            break;
                        }
                        auto ait = g_recordTypes.find(anc);
                        if (ait == g_recordTypes.end()) break;
                        anc = ait->second.parent;
                    }
                    if (hasOverride) break;
                }
                if (!hasOverride) continue;
                std::string ret = methPd->is_function ? cType(methPd->return_type) : "void";
                std::string sig = ret + " pas_" + baseType + "_" + methName + "__virt(";
                sig += "struct pas_rec_" + baseType + "* self";
                for (auto& p : methPd->params)
                    sig += ", " + cType(p.type) + " " + safeId(p.name);
                sig += ");";
                emitind(sig);
            }
        }
        emitln();

        // ── Global variables & constants ──────
        emitln("/* Global variables */");
        for (auto& d : prog->declarations)
            if (!dynamic_cast<ProcDef*>(d.get()))
                genDecl(d, true);
        emitln();

        // Populate allProcs for genCall param type lookup
        std::function<void(const std::vector<ASTPtr>&)> fillAllProcs =
            [&](const std::vector<ASTPtr>& decls) {
                for (auto& d : decls) {
                    if (auto* p = dynamic_cast<ProcDef*>(d.get())) {
                        std::string pn = p->name; std::transform(pn.begin(),pn.end(),pn.begin(),::tolower);
                        allProcs[pn] = p;
                        fillAllProcs(p->decls);
                    }
                }
            };
        fillAllProcs(prog->declarations);
        // ── Procedure/function definitions ────
        for (auto& d : prog->declarations)
            if (dynamic_cast<ProcDef*>(d.get()))
                genProcDef(d);

        // ── Virtual dispatch functions ─────────
        // For each method in a base type that is overridden in a child type,
        // emit a __virt dispatcher that checks self->__type__ at runtime.
        for (auto& [baseType, baseDef] : g_recordTypes) {
            for (auto& [methName, methPd] : baseDef.methods) {
                // Collect child types that override this method WITH THE SAME SIGNATURE
                std::vector<std::string> overriders;
                for (auto& [childType, childDef] : g_recordTypes) {
                    if (childType == baseType) continue;
                    std::string anc = childDef.parent;
                    bool isChild = false;
                    while (!anc.empty()) {
                        if (anc == baseType) { isChild = true; break; }
                        auto ait = g_recordTypes.find(anc);
                        if (ait == g_recordTypes.end()) break;
                        anc = ait->second.parent;
                    }
                    if (!isChild) continue;
                    auto mit = childDef.methods.find(methName);
                    if (mit == childDef.methods.end()) continue;
                    // Only if same number of params (same signature)
                    if (mit->second->params.size() == methPd->params.size())
                        overriders.push_back(childType);
                }
                if (overriders.empty()) continue;

                // Build the __virt function signature matching the base method
                std::string ret = methPd->is_function ? cType(methPd->return_type) : "void";
                std::string sig = ret + " pas_" + baseType + "_" + methName + "__virt(";
                sig += "struct pas_rec_" + baseType + "* self";
                for (auto& p : methPd->params) {
                    sig += ", " + cType(p.type) + " " + safeId(p.name);
                }
                sig += ")";
                emitind(sig + " {");
                indent++;
                // Check each overriding child type
                for (auto& childType : overriders) {
                    std::string paramList = "((struct pas_rec_" + childType + "*)self)";
                    for (auto& p : methPd->params)
                        paramList += ", " + safeId(p.name);
                    emitind("if (strcmp(self->__type__, \"" + childType + "\") == 0)");
                    std::string callLine = (methPd->is_function ? std::string("return ") : std::string("")) +
                            "pas_" + childType + "_" + methName + "(" + paramList + ");";
                    emitind("    " + callLine);
                }
                // Base fallback
                std::string baseCall = "pas_" + baseType + "_" + methName + "(self";
                for (auto& p : methPd->params)
                    baseCall += ", " + safeId(p.name);
                baseCall += ")";
                emitind(std::string(methPd->is_function ? "return " : "") + baseCall + ";");
                indent--;
                emitind("}");
                emitln();
            }
        }

        // ── main ──────────────────────────────
        emitln("int main(void) {");
        indent++;
        // Disable stdout buffering and enable raw terminal mode immediately
        emitind("setvbuf(stdout, NULL, _IONBF, 0);");
        // Enable raw mode at startup for terminal programs only
        if (!usesGraphics && (!userProcs.count("readkey") || !userProcs.count("keypressed")))
            emitind("_pas_set_raw();");
        // Initialize __type__ for all global object variables
        for (auto& [varId, typeName] : globalObjVars)
            emitind("strncpy(" + varId + ".__type__, \"" + typeName + "\", 63);");
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

    // Get the system temp directory ($TMPDIR on Android/Termux, /tmp elsewhere)
    auto getTmpDir = []() -> std::string {
        const char* t = getenv("TMPDIR");
        if (t && *t) return std::string(t);
        // Termux fallback
        if (access("/data/data/com.termux/files/usr/tmp", W_OK) == 0)
            return "/data/data/com.termux/files/usr/tmp";
        return "/tmp";
    };
    std::string tmpDir = getTmpDir();
    std::string cFile      = tmpDir + "/_pascal_compiled.c";
    std::string errFile    = tmpDir + "/_pascal_compile_err.txt";
    std::string debugFile  = tmpDir + "/_pascal_compiled_debug.c";
    {
        std::ofstream f(cFile);
        if (!f) { errorMsg = "Cannot write temp file"; return false; }
        f << cSource;
    }

    std::string sdlFlags;
    // Use the compiler's own graphics detection
    if (compiler.usesGraphics)
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
                    + " -lm 2>" + errFile;
    std::cerr << "[SDL2 flags: " << (sdlFlags.empty() ? "(none)" : sdlFlags) << "]\n";
    std::cerr << "[gcc cmd: " << cmd << "]\n";
    int ret = system(cmd.c_str());

    if (ret != 0) {
        std::ifstream ef(errFile);
        errorMsg = std::string(std::istreambuf_iterator<char>(ef), {});
        std::ofstream dbg(debugFile);
        dbg << cSource;
        errorMsg += "\n[Generated C saved to " + debugFile + "]";
        return false;
    }
    return true;
}
