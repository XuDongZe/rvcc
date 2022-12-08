#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

char *currentInput;

typedef enum
{
    TOK_PUNCT, // 操作符号
    TOK_NUM,
    TOK_EOF, // 文件终止符
} TokenKind;

typedef enum
{
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_NUM, // 整数
    ND_NEG, // 取相反数
    ND_EQ,  // 相等
    ND_NEQ, // 不等
    ND_LT,  // 小于
    ND_LET, // 小于等于
    ND_GT,  // 大于
    ND_GET, // 大于等于
} NodeKind;

typedef struct Node Node;
struct Node
{
    NodeKind kind;
    struct Node *lhs; // left-hand side
    struct Node *rhs; // right-hand side
    int val;          // 存储ND_NUM的值
};

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

static void error(char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    va_end(va);
    exit(1);
}

static void errorAt(char *loc, char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);

    int pos = loc - currentInput;
    if (pos > 0)
    {
        fprintf(stderr, "%s\n", currentInput);
        fprintf(stderr, "%*s", pos, " ");
        fprintf(stderr, "^ ");
    }
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");

    va_end(va);
    exit(1);
}

typedef struct Token
{
    TokenKind kind;
    struct Token *next;
    long val;
    char *loc; //在当前解析的字符串内的起始位置
    int len;   // 字符长度
} Token;

static Token *newToken(TokenKind kind, char *start, char *end)
{
    Token *tok = malloc(sizeof(Token));
    if (tok == NULL)
    {
        error("malloc mem for tok failed.");
    }
    tok->kind = kind;
    tok->loc = start;
    tok->len = end - start;
    return tok;
}

static bool equal(Token *tok, char *str)
{
    return memcmp(tok->loc, str, tok->len) == 0 && str[tok->len] == '\0';
}

static Token *skip(Token *tok, char *str)
{
    if (!equal(tok, str))
    {
        errorAt(tok->loc, str);
    }
    return tok->next;
}

static long getNumber(Token *tok)
{
    if (tok->kind != TOK_NUM)
    {
        errorAt(tok->loc, "expect a number");
    }
    return tok->val;
}

// expr = equality
// equality = relational ("==" relational || "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add = mul ("*" mul | "/" mul)
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-") unary | primary
// primary = "(" expr ")" | num
static Node *expr(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

// expr = equality
static Node *expr(Token **rest, Token *tok)
{
    return equality(rest, tok);
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
            node = newBinary(ND_GT, node, add(&tok, tok->next));
            continue;
        }
        if (equal(tok, ">="))
        {
            node = newBinary(ND_GET, node, add(&tok, tok->next));
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

// primary = "(" expr ")" | num
static Node *primary(Token **rest, Token *tok)
{
    // "(" expr ")"
    if (equal(tok, "("))
    {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
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

// 语义分析与代码生成
static int depth;

// 压栈，结果临时压入栈中保存
// sp为栈顶指针 stack pointer，栈反向向下增长。64位下，一个单位8个字节。
// 不使用寄存器，因为需要存储的值的数量是变化的

static void push(void)
{
    // 调整sp
    printf(" addi sp, sp, -8\n");
    // 将a0压入栈
    // save double, from a0, to sp[0]
    printf(" sd a0, 0(sp)\n");
    // 记录当前栈深度
    depth++;
}

static void pop(char *reg)
{
    // load double,
    printf(" ld %s, 0(sp)\n", reg);
    printf(" addi sp, sp, 8\n");
    depth--;
}

static void genExpr(Node *node)
{
    switch (node->kind)
    {
    case ND_NUM:
        printf(" li a0, %d\n", node->val);
        return;
    case ND_NEG:
        // 子树代码生成
        genExpr(node->lhs);
        // 此时子树的结果保存在a0中。
        // 最后对结果取反
        printf(" neg a0, a0\n");
        return;
    default:
        break;
    }

    // 递归到最右节点
    genExpr(node->rhs);
    // 右节点结果压栈
    push();
    // 递归到左节点
    genExpr(node->lhs);
    // 此时左节点结果没有压栈，只保存在a0中。
    // 将之前右节点压栈的结果，弹栈到a1
    pop("a1");

    // 左右节点结果均处理完毕，并且保存在寄存器中：左节点在a0，右节点在a1中。
    // 根据节点类型，运算。
    switch (node->kind)
    {
    case ND_ADD:
        printf(" add a0, a0, a1\n");
        return;
    case ND_SUB:
        printf(" sub a0, a0, a1\n");
        return;
    case ND_MUL:
        printf(" mul a0, a0, a1\n");
        return;
    case ND_DIV:
        printf(" div a0, a0, a1\n");
        return;
    case ND_EQ:
        // a0 = a0 ^ a1
        printf(" xor a0, a0, a1\n");
        // 如果a0寄存器的值为0, 即a0 == a1, 置a0为1
        printf(" seqz a0, a0\n");
        return;
    case ND_NEQ:
        printf(" xor a0, a0, a1\n");
        // 如果a0寄存器的值不为0, 即a0 != a1, 置a0为1
        printf(" snez a0, a0\n");
        return;
    case ND_LT:
        // set a0 = 1 if a0 < a1
        printf(" slt a0, a0, a1\n");
        return;
    case ND_LET:
        // a0<=a1 == 1 => !(a0>a1) => a0>a1 != 1 =>
        printf(" slt a0, a1, a0\n");
        printf(" xori a0, a0, 1\n");
        // 如果a0寄存器的值不为0, 即a0 != 1, 置a0为1
        printf(" snez a0, a0\n");
        return;
    case ND_GT:
        printf(" slt a0, a1, a0\n");
        return;
    case ND_GET:
        // a0>=a1 => (a0<a1) != 1
        printf(" slt a0, a0, a1\n");
        printf(" xori a0, a0, 1\n");
        printf(" snez a0, a0\n");
        return;
    default:
        error("invalid expression");
        return;
    }
}

static bool isPunct(char p)
{
    // 解析二元操作符号
    return p == '+' || p == '-' || p == '*' || p == '/' || p == '(' || p == ')' || p == '>' || p == '<';
}

static bool startsWith(char *str, char *subStr)
{
    return memcmp(str, subStr, strlen(subStr)) == 0;
}

static int readPunct(char *p)
{
    if (startsWith(p, "==") || startsWith(p, "!=") || startsWith(p, ">=") || startsWith(p, "<="))
    {
        return 2;
    }
    return isPunct(*p) ? 1 : 0;
}

static Token *tokenize(char *p)
{
    Token head = {};
    Token *cur = &head;

    while (*p)
    {
        // 跳过空白符
        if (isspace(*p))
        {
            p++;
            continue;
        }
        // 解析数字
        if (isdigit(*p))
        {
            char *startP = p;
            int val = strtoul(p, &p, 10);
            Token *tok = newToken(TOK_NUM, startP, p);
            tok->val = val;
            cur->next = tok;
            cur = cur->next;
            continue;
        }
        // 解析操作符
        int punctLen = readPunct(p);
        if (punctLen)
        {
            Token *tok = newToken(TOK_PUNCT, p, p + punctLen);
            cur->next = tok;
            cur = cur->next;
            p += punctLen;
            continue;
        }
        errorAt(p, "invalid token: %c", *p);
    }
    cur->next = newToken(TOK_EOF, p, p);
    return head.next;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        error("%s: invalid number of arguments\n", argv[0]);
        return 1;
    }

    currentInput = argv[1];
    // currentInput = "1-8/(2*2)+3*6";
    // currentInput = "1";
    Token *tok = tokenize(currentInput);

    Node *node = expr(&tok, tok);
    if (tok->kind != TOK_EOF)
    {
        errorAt(tok->loc, "extra token: %d", tok->kind);
    }

    // 声明一个全局main段，同时也是程序入口段
    printf("  .global main\n");
    printf("main:\n");

    // 便利AST树生成汇编代码
    genExpr(node);

    printf(" ret\n");

    // 如果stack未清空 报错
    assert(depth == 0);
    return 0;
}
