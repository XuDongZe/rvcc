#include "rvcc.h"

// 本地变量表
Obj *locals;

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

// 新变量节点
static Node *newVarNode(Obj *var)
{
    Node *node = newNode(ND_VAR);
    node->var = var;
    return node;
}

// 变量表操作

// 在本地变量表中查找与tok->var匹配的变量
static Obj *findLVar(Token *tok)
{
    for (Obj *var = locals; var; var = var->next)
    {
        // 首先判断长度，然后逐字符比较
        if (strlen(var->name) == tok->len && !strncmp(var->name, tok->loc, tok->len))
        {
            return var;
        }
    }
    return NULL;
}

static Obj *newLVar(Token *tok)
{
    Obj *var = calloc(1, sizeof(Obj));
    // 拷贝标识符：locals中的name和token表中的loc，互不影响。指向两个值相同的字符串。
    var->name = strndup(tok->loc, tok->len);
    // offset先不处理：offset按照链表内节点的index分配

    // 放置到链表中：放置到链表头部。所以地址分配是后进先分配的。是一个地址分配的栈。
    var->next = locals;
    locals = var;
    // 返回新建的var
    return var;
}

// 新建一个本地变量，并插入本地变量表中
static Obj *newOrFindLVar(Token *tok)
{
    Obj *oldVar = findLVar(tok);
    if (oldVar)
    {
        return oldVar;
    }
    return newLVar(tok);
}

// return: 生成一个AST树节点。
// rest: 在callee内部修改当前tok的指向。
// 在caller调用newASTNode函数前后，都要保持tok的栈值，是当前要处理的token的地址。
// 也就是tok在newASTNode调用前，调用时，调用后，都得保持tok是当前待处理的token数据的指针。
//
// static Node *newASTNode(Token **rest, Token *tok);

// program = compoundStmt

// compoundStmt = "{" stmt* "}"
// stmt = returnStmt | compoundStmt | exprStmt
// returnStmt = "return" exprStmt
// exprStmt = expr? ";"
// expr = assign
// assign = equality ("=" assign)?
// equality = relational ("==" relational || "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add = mul ("*" mul | "/" mul)
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-") unary | primary
// primary = "(" expr ")" | ident | num
static Node *program(Token **rest, Token *tok);
static Node *compoundStmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *returnStmt(Token **rest, Token *tok);
static Node *exprStmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

// program = compoundStmt

// compoundStmt = "{" (stmt)* "}"
static Node *compoundStmt(Token **rest, Token *tok) {
    // "{"
    tok = skip(tok, "{");
    // stmt*
    Node head = {};
    Node *cur = &head;
    while (!equal(tok, "}")) {
        cur->next = stmt(&tok, tok);
        cur = cur->next;
    }
    Node *node = newNode(ND_BLOCK);
    // maybe NULL
    node->body = head.next;
    // "}"
    tok = skip(tok, "}");
    *rest = tok;
    return node;
}

// stmt = returnStmt | exprStmt
static Node *stmt(Token **rest, Token *tok)
{
    if (equal(tok, "return")) {
        return returnStmt(rest, tok);
    }
    if (equal(tok, "{")) {
        return compoundStmt(rest, tok);
    }
    return exprStmt(rest, tok);
}

// returnStmt = "return" exprStmt
static Node *returnStmt(Token **rest, Token *tok)
{
    tok = skip(tok, "return");
    Node *node = newUnary(ND_RETURN, exprStmt(&tok, tok));
    *rest = tok;
    return node;
}

// exprStmt = ; | expr ";"
static Node *exprStmt(Token **rest, Token *tok)
{
    // ";"
    if (equal(tok, ";")) {
        // 将";"当作"{}"处理。
        Node *node = newNode(ND_BLOCK);
        *rest = tok->next;
        return node;
    }

    // expr ";"
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
    if (tok->kind == TOK_IDENT)
    {
        Obj *var = newOrFindLVar(tok);
        Node *node = newVarNode(var);
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

Function *parse(Token *tok)
{
    Node *body = compoundStmt(&tok, tok);

    // 封装为Function
    Function *prog = calloc(1, sizeof(Function));
    prog->body = body;
    prog->locals = locals;
    // 此时还未分配stackSize和locals里var的stack地址 => 代码生成时处理
    return prog;
}
