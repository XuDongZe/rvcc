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
} NodeKind;

typedef struct Node Node;
struct Node {
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

static Node *expr(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

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

// mul = primary ("*" primary | "/" primary)*
static Node *mul(Token **rest, Token *tok)
{
    // primary
    Node *node = primary(&tok, tok);
    // ("*" primary | "/" primary)*
    while (true)
    {
        // "*" primary
        if (equal(tok, "*"))
        {
            node = newBinary(ND_MUL, node, primary(&tok, tok->next));
            continue;
        }
        if (equal(tok, "/"))
        {
            node = newBinary(ND_DIV, node, primary(&tok, tok->next));
            continue;
        }

        *rest = tok;
        return node;
    }
}

// expr = mul ("+" mul | "-" mul)*
static Node *expr(Token **rest, Token *tok)
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
            ;
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
    if (node->kind == ND_NUM)
    {
        printf(" li a0, %d\n", node->val);
        return;
    }

    genExpr(node->rhs);
    push();
    genExpr(node->lhs);
    pop("a1");

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
    default:
        error("invalid expression");
        return;
    }
}

static bool isPunct(char p) {
    return p == '+' || p == '-' || p == '*' || p == '/' || p == '(' || p == ')';
}

static Token *tokenize(char *p)
{
    Token head = {};
    Token *cur = &head;

    while (*p)
    {
        if (isspace(*p))
        {
            p++;
            continue;
        }
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
        if (isPunct(*p))
        {
            Token *tok = newToken(TOK_PUNCT, p, p + 1);
            cur->next = tok;
            cur = cur->next;
            p++;
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
    Token *tok = tokenize(currentInput);

    Node *node = expr(&tok, tok);
    if (tok->kind != TOK_EOF) {
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
