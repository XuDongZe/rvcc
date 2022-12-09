#include "rvcc.h"

// 当前输入
static char *currentInput;

// 内部报错函数
void error(char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    va_end(va);
    exit(1);
}

void errorAt(char *loc, char *fmt, ...)
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

// Token操作

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

static long getNumber(Token *tok)
{
    if (tok->kind != TOK_NUM)
    {
        errorAt(tok->loc, "expect a number");
    }
    return tok->val;
}

bool equal(Token *tok, char *str)
{
    return memcmp(tok->loc, str, tok->len) == 0 && str[tok->len] == '\0';
}

Token *skip(Token *tok, char *str)
{
    if (!equal(tok, str))
    {
        errorAt(tok->loc, str);
    }
    return tok->next;
}

// 字符判定

static bool isPunct(char p)
{
    // 解析二元操作符号
    return ispunct(p);
    // return p == '+' || p == '-' || p == '*' || p == '/' || p == '(' || p == ')' || p == '>' || p == '<';
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

// 接口函数
Token *tokenize(char *p)
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