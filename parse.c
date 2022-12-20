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

// 在变量表中新增一个变量
static Obj *newLVar(char *name, Type *ty)
{
    Obj *var = calloc(1, sizeof(Obj));
    // 拷贝标识符：locals中的name和token表中的loc，互不影响。指向两个值相同的字符串。
    var->name = name;
    var->ty = ty;
    // offset先不处理：offset按照链表内节点的index分配

    // 放置到链表中：放置到链表头部。所以地址分配是后进先分配的。是一个地址分配的栈。
    var->next = locals;
    locals = var;
    // 返回新建的var
    return var;
}

// 获取tok中的标识符副本
static char *getIdent(Token *tok)
{
    if (tok->kind != TOK_IDENT)
    {
        errorTok(tok, "expected an identifier");
    }
    return strndup(tok->loc, tok->len);
}

// return: 生成一个AST树节点。
// rest: 在callee内部修改当前tok的指向。
// 在caller调用newASTNode函数前后，都要保持tok的栈值，是当前要处理的token的地址。
// 也就是tok在newASTNode调用前，调用时，调用后，都得保持tok是当前待处理的token数据的指针。
//
// static Node *newASTNode(Token **rest, Token *tok);

// program = stmt*

// stmt = returnStmt | compoundStmt | ifStmt | forStmt | whileStmt | exprStmt
// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*) ";"
// declspec = "int"
// declarator = "*"* ident
// returnStmt = "return" exprStmt
// compoundStmt = "{" (declaration | stmt)* "}"
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
static Node *declaration(Token **rest, Token *tok);
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

// declspec = "int"
// decarator specifier
static Type *declspec(Token **rest, Token *tok)
{
    *rest = skip(tok, "int");
    return tyInt;
}

// declarator = "*"* ident
static Type *declarator(Token **rest, Token *tok, Type *ty)
{
    // "*"*
    while (consume(&tok, tok, "*"))
    {
        ty = newPointer(ty);
    }

    if (tok->kind != TOK_IDENT)
    {
        errorTok(tok, "expected a variable name");
    }

    // ident
    ty->tok = tok;
    *rest = tok->next;
    return ty;
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*) ";"
// int x=1, *z=&x, y=*z=2;
static Node *declaration(Token **rest, Token *tok)
{
    Type *base = declspec(&tok, tok);

    Node head = {};
    Node *cur = &head;
    int i = 0;

    while (!equal(tok, ";"))
    {
        if (i++ > 0)
        {
            tok = skip(tok, ",");
        }

        // declarator
        Type *ty = declarator(&tok, tok, base);
        Obj *var = newLVar(getIdent(ty->tok), ty);

        // 如果不是赋值语句，则为声明语句，变量已经放置到变量表中了。
        if (!equal(tok, "="))
        {
            continue;
        }

        // 封装为表达式语句
        Node *lhs = newVarNode(var, ty->tok);
        // 递归赋值语句
        Node *rhs = assign(&tok, tok->next);
        Node *node = newBinary(ND_ASSIGN, lhs, rhs, tok);
        // 表达式
        cur->next = newUnary(ND_EXPR_STMT, node, tok);
        cur = cur->next;
    }

    // 封装为block节点
    Node *node = newNode(ND_BLOCK, tok);
    node->body = head.next;
    // 调整全局token指针
    tok = skip(tok, ";");
    *rest = tok;
    return node;
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

// compoundStmt = "{" (declaration | stmt)* "}"
static Node *compoundStmt(Token **rest, Token *tok)
{
    Token *startTok = tok;

    // "{"
    tok = skip(tok, "{");
    // (declaration | stmt)*
    Node head = {};
    Node *cur = &head;
    while (!equal(tok, "}"))
    {
        // declaration
        if (equal(tok, "int"))
            cur->next = declaration(&tok, tok);
        // stmt
        else
            cur->next = stmt(&tok, tok);
        cur = cur->next;
        // 为ast添加类型信息
        addType(cur);
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

// funcCall = ident "(" assign ("," assign)? ")"
static Node *funcCall(Token **rest, Token *tok)
{
    // tok is ident now
    Token *startTok = tok;
    tok = tok->next->next;

    // args
    Node head = {};
    Node *cur = &head;
    while (!equal(tok, ")"))
    {
        if (cur != &head)
        {
            tok = skip(tok, ",");
        }
        cur->next = assign(&tok, tok);
        cur = cur->next;
    }
    *rest = skip(tok, ")");

    Node *nd = newNode(ND_FUNCALL, tok);
    nd->funcName = getIdent(startTok);
    nd->args = head.next;
    return nd;
}

// primary = "(" expr ")" | ident args? | num
// args = "(" ")"
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
        // function
        if (equal(tok->next, "("))
        {
            Node *nd = funcCall(&tok, tok);
            *rest = tok;
            return nd;
        }

        // var
        Obj *var = findLVar(tok);
        if (!var)
        {
            errorTok(tok, "undefined variable");
        }
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
