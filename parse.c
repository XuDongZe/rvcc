#include "rvcc.h"

// 抽象语法树Node 数据结构 操作

static Node *newNode(NodeKind kind)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

static Node *newBinary(NodeKind kind, Node *l, Node *r)
{
    Node *node = newNode(kind);
    node->lhs = l;
    node->rhs = r;
    return node;
}

static Node *newUnary(NodeKind kind, Node *n)
{
    Node *node = newNode(kind);
    node->lhs = n;
    return node;
}

static Node *newNum(int val)
{
    Node *node = newNode(ND_NUM);
    node->val = val;
    return node;
}

static Node *newVarNode(char name) {
    Node *node = newNode(ND_VAR);
    node->name = name;
    return node;
}

// program = stmt*
// stmt = exprStmt
// exprStmt = expr ";"
// expr = assign
// assign = equality ("=" assign)?
// equality = relational ("==" relational || "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add = mul ("*" mul | "/" mul)
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-") unary | primary
// primary = "(" expr ")" | ident | num
static Node *program(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *exprStmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

// program = stmt*

// stmt = exprStmt
static Node *stmt(Token **rest, Token *tok)
{
    return exprStmt(rest, tok);
}

// exprStmt = expr ";"
static Node *exprStmt(Token **rest, Token *tok)
{
    Node *node = newUnary(ND_EXPR_STMT, expr(&tok, tok));
    *rest = skip(tok, ";");
    return node;
}

// expr = assign
static Node *expr(Token **rest, Token *tok)
{
    return assign(rest, tok);
}

// assign = equality ("=" assign)?
static Node *assign(Token **rest, Token *tok)
{
    // equality
    Node *node = equality(&tok, tok);
    // ("=" assign)?
    if (equal(tok, "="))
    {
        node = newBinary(ND_ASSIGN, node, assign(&tok, tok->next));
    }
    // 调整rest指针值为下一个要处理的指针，即tok。
    *rest = tok;
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality(Token **rest, Token *tok)
{
    // relational
    Node *node = relational(&tok, tok);
    // "!=" relational || "==" relational
    while (true)
    {
        if (equal(tok, "=="))
        {
            node = newBinary(ND_EQ, node, relational(&tok, tok->next));
            continue;
        }
        if (equal(tok, "!="))
        {
            node = newBinary(ND_NEQ, node, relational(&tok, tok->next));
            continue;
        }

        *rest = tok;
        return node;
    }
}

// realational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(Token **rest, Token *tok)
{
    // add
    Node *node = add(&tok, tok);
    // ("<" add | "<=" add | ">" add | ">=" add)
    while (true)
    {
        if (equal(tok, "<"))
        {
            node = newBinary(ND_LT, node, add(&tok, tok->next));
            continue;
        }
        if (equal(tok, "<="))
        {
            node = newBinary(ND_LET, node, add(&tok, tok->next));
            continue;
        }
        if (equal(tok, ">"))
        {
            node = newBinary(ND_LT, add(&tok, tok->next), node);
            continue;
        }
        if (equal(tok, ">="))
        {
            node = newBinary(ND_LET, add(&tok, tok->next), node);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// add = mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok)
{
    // mul
    Node *node = mul(&tok, tok);
    while (true)
    {
        // "+" mul
        if (equal(tok, "+"))
        {
            node = newBinary(ND_ADD, node, mul(&tok, tok->next));
            continue;
        }
        // "-" mul
        if (equal(tok, "-"))
        {
            node = newBinary(ND_SUB, node, mul(&tok, tok->next));
            continue;
        }

        *rest = tok;
        return node;
    }
    errorAt(tok->loc, "expected an expression");
}

// mul = unary ("*" unary | "/" unary)*
static Node *mul(Token **rest, Token *tok)
{
    // unary
    Node *node = unary(&tok, tok);
    // ("*" unary | "/" unary)*
    while (true)
    {
        // "*" unary
        if (equal(tok, "*"))
        {
            node = newBinary(ND_MUL, node, unary(&tok, tok->next));
            continue;
        }
        if (equal(tok, "/"))
        {
            node = newBinary(ND_DIV, node, unary(&tok, tok->next));
            continue;
        }

        *rest = tok;
        return node;
    }
}

// unary = ("+" | "-") unary | primary
// 解析一元运算
static Node *unary(Token **rest, Token *tok)
{
    // "+" unary
    if (equal(tok, "+"))
    {
        return unary(rest, tok->next);
    }
    // "-" unary
    if (equal(tok, "-"))
    {
        return newUnary(ND_NEG, unary(rest, tok->next));
    }
    // primary
    return primary(rest, tok);
}

// primary = "(" expr ")" | ident | num
static Node *primary(Token **rest, Token *tok)
{
    // "(" expr ")"
    if (equal(tok, "("))
    {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }
    // ident
    if (tok->kind == TOK_IDENT) {
        Node *node = newVarNode(*tok->loc);
        *rest = tok->next;
        return node;
    }
    // num
    if (tok->kind == TOK_NUM)
    {
        Node *node = newNum(tok->val);
        *rest = tok->next;
        return node;
    }
    errorAt(tok->loc, "expected an expression");
}

Node *parse(Token *tok)
{

    Node head = {};
    Node *cur = &head;

    // program = stmt*
    while (tok->kind != TOK_EOF)
    {
        cur->next = stmt(&tok, tok);
        cur = cur->next;
    }
    return head.next;
}
