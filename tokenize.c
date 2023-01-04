#include "rvcc.h"

// 当前输入
const char *currentInput;

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

// 输出错误出现的位置，并退出
static void verrorAt(char *loc, char *fmt, va_list va)
{
    int pos = loc - currentInput;
    // 输出源信息
    fprintf(stderr, "%s\n", currentInput);
    // 输出^位置标记
    if (pos > 0)
    {
        // 将""补齐为pos位，在左侧填充pos个空格
        fprintf(stderr, "%*s", pos, "");
    }
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");

    va_end(va);
    exit(1);
}

void errorAt(char *loc, char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    verrorAt(loc, fmt, va);
}

void errorTok(Token *tok, char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    verrorAt(tok->loc, fmt, va);
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
        errorTok(tok, "expect a number");
    }
    return tok->val;
}

// 匹配token与指定的字符串 true表示匹配成功
bool equal(Token *tok, char *str)
{
    return memcmp(tok->loc, str, tok->len) == 0 && str[tok->len] == '\0';
}

// 跳过指定的字符串，不匹配则报错
Token *skip(Token *tok, char *str)
{
    if (!equal(tok, str))
    {
        errorTok(tok, "expected: '%s'", str);
    }
    return tok->next;
}

// 消耗指定的字符串 true表示成功匹配并消耗 false匹配失败
bool consume(Token **rest, Token *tok, char *str)
{
    if (equal(tok, str))
    {
        *rest = tok->next;
        return true;
    }
    *rest = tok;
    return false;
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

// 判断是否是标记符的首字母规则
// [a-zA-Z_]
static bool isIdent1(char p)
{
    return (p >= 'a' && p <= 'z') || (p >= 'A' && p <= 'Z') || p == '_';
}

// 判断是否是标记符的非首字母规则
// [a-zA-Z0-9_]
static bool isIdent2(char p)
{
    return isIdent1(p) || (p >= '0' && p <= '9');
}

static int readPunct(char *p)
{
    if (startsWith(p, "==") || startsWith(p, "!=") || startsWith(p, ">=") || startsWith(p, "<="))
    {
        return 2;
    }
    return isPunct(*p) ? 1 : 0;
}

// 关键字表。指针数组，每一个元素是char*
static char *kws[] = {"return", "if", "else", "for", "while", "int", "char", "sizeof"};
static int KW_LEN = sizeof(kws) / sizeof(kws[0]);
// 判断tok是否是一个关键字
static bool isKw(Token *tok)
{
    if (tok->kind == TOK_KEKWORD)
    {
        return true;
    }
    if (tok->kind != TOK_IDENT)
    {
        return false;
    }
    for (int i = 0; i < KW_LEN; i++)
    {
        if (equal(tok, kws[i]))
        {
            return true;
        }
    }
    return false;
}

// 将tok中的keyword部分找出来
static void convertKeyWord(Token *tok)
{
    for (Token *p = tok; p; p = p->next)
    {
        if (isKw(p))
        {
            p->kind = TOK_KEKWORD;
        }
    }
}

Token *readStringLiteral(char *p)
{
    char *start = p;
    // skip "
    p++;
    while (*p != '"')
        p++;
    // now *p == '"'
    // start is left-'"' and p is right-'"'

    // if str is "", then tok->len = 2
    // if str is "a",then tok->len = 3
    Token *tok = newToken(TOK_STR, start, p + 1);
    // tok->str = "abc"
    tok->str = strndup(start + 1, p - start - 1);
    return tok;
}

// 接口函数
Token *tokenize(char *input)
{
    currentInput = input;
    char *p = input;

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
        // 解析字符串
        if (*p == '"')
        {
            cur = cur->next = readStringLiteral(p);
            p += cur->len;
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
        // 解析标记符 isIdent1 (isIdent2)*
        // 首先命中首字母规则
        if (isIdent1(*p))
        {
            char *startP = p;
            do
            {
                p++;
            } while (isIdent2(*p));
            // 处理完毕，此时p指向下一个待处理的字符
            cur->next = newToken(TOK_IDENT, startP, p);
            cur = cur->next;
            continue;
            ;
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
    // 关键字识别
    convertKeyWord(head.next);
    // 哨兵节点
    cur->next = newToken(TOK_EOF, p, p);
    return head.next;
}