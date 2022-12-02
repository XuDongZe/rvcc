#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

char *currentInput;

typedef enum
{
    TOK_OP, // 操作符号
    TOK_NUM,
    TOK_EOF, // 文件终止符
} TokenKind;

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

static long getNumber(Token *tok)
{
    if (tok->kind != TOK_NUM)
    {
        errorAt(tok->loc, "expect a number");
    }
    return tok->val;
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
        if (*p == '+' || *p == '-')
        {
            Token *tok = newToken(TOK_OP, p, p + 1);
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

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        error("%s: invalid number of arguments\n", argv[0]);
        return 1;
    }

    currentInput = argv[1];
    Token *tok = tokenize(argv[1]);

    printf("  .global main\n");
    printf("main:\n");
    printf("  li a0, %ld\n", getNumber(tok));
    tok = tok->next;

    while (tok->kind != TOK_EOF)
    {
        if (equal(tok, "+"))
        {
            tok = tok->next;
            printf("  addi a0, a0, %ld\n", getNumber(tok));
            tok = tok->next;
            continue;
        }
        if (equal(tok, "-"))
        {
            tok = tok->next;
            printf(" addi a0, a0, -%ld\n", getNumber(tok));
            tok = tok->next;
            continue;
        }
        errorAt(tok->loc, "unexpected tok: %s", tok->loc);
    }
    printf("  ret\n");
    return 0;
}
