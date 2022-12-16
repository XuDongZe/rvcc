#include "rvcc.h"

// 本地变量表
Obj *locals;

// 抽象语法树Node 数据结构 操作

static Node *attachTok(Node *node, Token *tok)
{
    node->tok = tok;
    return node;
}

static Node *newNode(NodeKind kind, Token *tok)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

static Node *newBinary(NodeKind kind, Node *l, Node *r, Token *tok)
{
    Node *node = newNode(kind, tok);
    node->lhs = l;
    node->rhs = r;
    return node;
}

static Node *newUnary(NodeKind kind, Node *n, Token *tok)
{
    Node *node = newNode(kind, tok);
    node->lhs = n;
    return node;
}

static Node *newNum(int val, Token *tok)
{
    Node *node = newNode(ND_NUM, tok);
    node->val = val;
    return node;
}

// 新变量节点
static Node *newVarNode(Obj *var, Token *tok)
{
    Node *node = newNode(ND_VAR, tok);
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

// program = stmt*

// stmt = returnStmt | compoundStmt | ifStmt | forStmt | whileStmt | exprStmt
// returnStmt = "return" exprStmt
// compoundStmt = "{" stmt* "}"
// ifStmt = "if" "(" expr ")" stmt ("else" stmt)?
// forStmt = "for" "(" (expr:init)? ";" (expr:cond)? ";" (expr:inc)? ")" stmt
// whileStmt = "while" "(" expr ")" stmt
// exprStmt = expr? ";"
// expr = assign
// assign = equality ("=" assign)?
// equality = relational ("==" relational || "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add = mul ("*" mul | "/" mul)
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-" | "&" | "*") unary | primary
// primary = "(" expr ")" | ident | num
static Node *program(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *returnStmt(Token **rest, Token *tok);
static Node *compoundStmt(Token **rest, Token *tok);
static Node *ifStmt(Token **rest, Token *tok);
static Node *forStmt(Token **rest, Token *tok);
static Node *whileStmt(Token **rest, Token *tok);
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

// stmt = returnStmt | compoundStmt | ifStmt | exprStmt
static Node *stmt(Token **rest, Token *tok)
{
    if (equal(tok, "return"))
    {
        return returnStmt(rest, tok);
    }
    if (equal(tok, "{"))
    {
        return compoundStmt(rest, tok);
    }
    if (equal(tok, "if"))
    {
        return ifStmt(rest, tok);
    }
    if (equal(tok, "for"))
    {
        return forStmt(rest, tok);
    }
    if (equal(tok, "while"))
    {
        return whileStmt(rest, tok);
    }
    return exprStmt(rest, tok);
}

// returnStmt = "return" exprStmt
static Node *returnStmt(Token **rest, Token *tok)
{
    Token *startTok = tok;

    tok = skip(tok, "return");
    Node *node = newUnary(ND_RETURN, exprStmt(&tok, tok), startTok);

    *rest = tok;
    return node;
}

// compoundStmt = "{" (stmt)* "}"
static Node *compoundStmt(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // "{"
    tok = skip(tok, "{");
    // stmt*
    Node head = {};
    Node *cur = &head;
    while (!equal(tok, "}"))
    {
        cur->next = stmt(&tok, tok);
        cur = cur->next;
    }
    Node *node = newNode(ND_BLOCK, startTok);
    // maybe NULL
    node->body = head.next;
    // "}"
    tok = skip(tok, "}");

    *rest = tok;
    return node;
}

// ifStmt = "if" "(" expr ")" stmt ("else" stmt)?
static Node *ifStmt(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // "if"
    tok = skip(tok, "if");
    // "("
    tok = skip(tok, "(");
    // cond expr
    Node *cond = expr(&tok, tok);
    // ")"
    tok = skip(tok, ")");
    // then stmt
    Node *then = stmt(&tok, tok);
    // else stmt
    Node *els = NULL;
    // 可选的else
    if (equal(tok, "else"))
    {
        tok = skip(tok, "else");
        els = stmt(&tok, tok);
    }
    // 构造if节点
    Node *node = newNode(ND_IF, startTok);
    node->cond = cond;
    node->then = then;
    node->els = els;

    // 返回
    *rest = tok;
    return node;
}

// forStmt = "for" "(" (expr:init)? ";" (expr:cond)? ";" (expr:inc)? ")" stmt
static Node *forStmt(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // "for"
    tok = skip(tok, "for");
    // "("
    tok = skip(tok, "(");
    // 可选的init expr
    Node *init = NULL;
    if (!equal(tok, ";"))
    {
        init = expr(&tok, tok);
    }
    tok = skip(tok, ";");
    // 可选的cond expr
    Node *cond = NULL;
    if (!equal(tok, ";"))
    {
        cond = expr(&tok, tok);
    }
    tok = skip(tok, ";");
    // 可选的incr expr
    Node *inc = NULL;
    if (!equal(tok, ")"))
    {
        inc = expr(&tok, tok);
    }
    // ")"
    tok = skip(tok, ")");
    // 条件成立的 循环体
    Node *then = stmt(&tok, tok);

    // 构建for节点
    Node *node = newNode(ND_FOR, startTok);
    node->init = init;
    node->cond = cond;
    node->inc = inc;
    node->then = then;

    *rest = tok;
    return node;
}

// whileStmt = "while" "(" expr ")" stmt
static Node *whileStmt(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // "while"
    tok = skip(tok, "while");
    // "("
    tok = skip(tok, "(");
    // expr
    Node *cond = expr(&tok, tok);
    // ")"
    tok = skip(tok, ")");
    // stmt
    Node *then = stmt(&tok, tok);
    // 构造While节点。我们用FOR来表示：for是增强版本的while
    Node *node = newNode(ND_FOR, startTok);
    node->cond = cond;
    node->then = then;

    *rest = tok;
    return node;
}

// exprStmt = ; | expr ";"
static Node *exprStmt(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // ";"
    if (equal(tok, ";"))
    {
        // 将";"当作"{}"处理。
        Node *node = newNode(ND_BLOCK, startTok);
        *rest = tok->next;
        return node;
    }

    // expr ";"
    Node *node = newUnary(ND_EXPR_STMT, expr(&tok, tok), startTok);
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
    Token *startTok = tok;

    // equality
    Node *node = equality(&tok, tok);
    // ("=" assign)?
    if (equal(tok, "="))
    {
        node = newBinary(ND_ASSIGN, node, assign(&tok, tok->next), startTok);
    }
    // 调整rest指针值为下一个要处理的指针，即tok。
    *rest = tok;
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // relational
    Node *node = relational(&tok, tok);
    // "!=" relational || "==" relational
    while (true)
    {
        if (equal(tok, "=="))
        {
            node = newBinary(ND_EQ, node, relational(&tok, tok->next), startTok);
            continue;
        }
        if (equal(tok, "!="))
        {
            node = newBinary(ND_NEQ, node, relational(&tok, tok->next), startTok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// realational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // add
    Node *node = add(&tok, tok);
    // ("<" add | "<=" add | ">" add | ">=" add)
    while (true)
    {
        if (equal(tok, "<"))
        {
            node = newBinary(ND_LT, node, add(&tok, tok->next), startTok);
            continue;
        }
        if (equal(tok, "<="))
        {
            node = newBinary(ND_LET, node, add(&tok, tok->next), startTok);
            continue;
        }
        if (equal(tok, ">"))
        {
            node = newBinary(ND_LT, add(&tok, tok->next), node, startTok);
            continue;
        }
        if (equal(tok, ">="))
        {
            node = newBinary(ND_LET, add(&tok, tok->next), node, startTok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// 支持类型转换的加法
static Node *typeAdd(Node *lhs, Node *rhs, Token *tok)
{
    // 为左右节点 添加类型
    addType(lhs);
    addType(rhs);

    // num + num
    if (isInteger(lhs->type) && isInteger(rhs->type))
    {
        return newBinary(ND_ADD, lhs, rhs, tok);
    }
    // num + ptr => ptr + num
    // 1+p => p+1
    if (!lhs->type->base && rhs->type->base)
    {
        // 交换lhs和rhs
        // 注意这里没有影响外面的参数
        Node *t = lhs;
        lhs = rhs;
        rhs = t;
    }

    // p+1 => p+1*8
    if (lhs->type->base && !rhs->type->base)
    {
        Node *r = newBinary(ND_MUL, rhs, newNum(8, tok), tok);
        return newBinary(ND_ADD, lhs, r, tok);
    }

    errorTok(tok, "invalid operand");
}

// 支持类型转换的减法
static Node *typeSub(Node *lhs, Node *rhs, Token *tok)
{
    // 为左右节点 添加类型
    addType(lhs);
    addType(rhs);

    // num - num
    if (isInteger(lhs->type) && isInteger(rhs->type))
    {
        return newBinary(ND_SUB, lhs, rhs, tok);
    }

    // ptr - ptr
    if (lhs->type->base && rhs->type->base)
    {
        Node *diff = newBinary(ND_SUB, lhs, rhs, tok);
        // 类型隐式转换为int
        diff->type = TY_INT;
        // 返回两个指针之间有多少元素
        return newBinary(ND_DIV, diff, newNum(8, tok), tok);
    }

    // ptr - num
    // p-1 => p-1*8
    if (lhs->type->base && !rhs->type->base)
    {
        Node *diff = newBinary(ND_MUL, rhs, newNum(8, tok), tok);
        return newBinary(ND_SUB, lhs, diff, tok);
    }

    errorTok(tok, "invalid operand");
}

// add = mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // mul
    Node *node = mul(&tok, tok);
    while (true)
    {
        // "+" mul
        if (equal(tok, "+"))
        {
            node = typeAdd(node, mul(&tok, tok->next), startTok);
            continue;
        }
        // "-" mul
        if (equal(tok, "-"))
        {
            node = typeSub(node, mul(&tok, tok->next), startTok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// mul = unary ("*" unary | "/" unary)*
static Node *mul(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // unary
    Node *node = unary(&tok, tok);
    // ("*" unary | "/" unary)*
    while (true)
    {
        // "*" unary
        if (equal(tok, "*"))
        {
            node = newBinary(ND_MUL, node, unary(&tok, tok->next), startTok);
            continue;
        }
        if (equal(tok, "/"))
        {
            node = newBinary(ND_DIV, node, unary(&tok, tok->next), startTok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// unary = ("+" | "-" | "*" | "&") unary | primary
// 解析一元运算
static Node *unary(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // "+" unary
    if (equal(tok, "+"))
    {
        return unary(rest, tok->next);
    }
    // "-" unary
    if (equal(tok, "-"))
    {
        return newUnary(ND_NEG, unary(rest, tok->next), startTok);
    }
    // "&" unary
    if (equal(tok, "&"))
    {
        return newUnary(ND_ADDR, unary(rest, tok->next), startTok);
    }
    // "*" unary
    if (equal(tok, "*"))
    {
        return newUnary(ND_DEREF, unary(rest, tok->next), startTok);
    }
    // primary
    return primary(rest, tok);
}

// primary = "(" expr ")" | ident | num
static Node *primary(Token **rest, Token *tok)
{
    Token *startTok = tok;

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
        Node *node = newVarNode(var, startTok);
        *rest = tok->next;
        return node;
    }
    // num
    if (tok->kind == TOK_NUM)
    {
        Node *node = newNum(tok->val, startTok);
        *rest = tok->next;
        return node;
    }
    errorTok(tok, "expected identifier or num");
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
