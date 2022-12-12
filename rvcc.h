// 使用POSIX.1标准
// 使用了strndup函数
#define _POSIX_C_SOURCE 200809L

// 公共头文件
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

// 类型声明

// 词法分析-Token类型
typedef enum
{
    TOK_IDENT,      // 标记符号，变量名/函数名等。
    TOK_PUNCT,      // 操作符号
    TOK_KEKWORD,    // 关键字
    TOK_NUM,        // 数字
    TOK_EOF,        // 文件终止符
} TokenKind;

typedef struct Token
{
    TokenKind kind;
    struct Token *next;
    long val;
    char *loc; // 在当前解析的字符串内的起始位置
    int len;   // 字符长度
} Token;

// 语法分析：

// AST抽象语法树 节点类型
typedef struct Node Node;

// 本地变量表 节点类型
typedef struct Obj Obj;
struct Obj
{
    Obj *next;  // 指向下一个节点的链接
    char *name; // 变量的标识符
    int offset; // 本地变量的栈偏移，基地址存放在fp。实际地址为：fp+offset
};

// 函数表 节点类型
typedef struct Function Function;
struct Function
{
    Node *body;    // 函数体
    Obj *locals;   // 本地变量表
    int stackSize; // 进入函数时，动态计算的栈大小
};

typedef enum
{
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_NEG, // 取相反数
    ND_EQ,  // 相等
    ND_NEQ, // 不等
    ND_LT,  // 小于
    ND_LET, // 小于等于
    ND_ASSIGN,      // 赋值
    ND_RETURN,      // 返回
    ND_BLOCK,       // 代码块
    ND_EXPR_STMT,   // 表达式语句
    ND_IF,          // if语句
    ND_FOR,         // for语句
    ND_VAR,         // 变量
    ND_NUM,         // 数字
} NodeKind;

// 抽象语法树-节点结构
struct Node
{
    NodeKind kind;      // 节点类型
    struct Node *next;  // 下一个
    struct Node *lhs;   // left-hand side
    struct Node *rhs;   // right-hand side

    Node *body;         // 代码块

    // if
    Node *cond;         // 条件表达式
    Node *then;         // then语句
    Node *els;          // else语句
    Node *init;         // for init表达式
    Node *inc;          // for incr表达式

    Obj *var;           // 存储ND_VAR的变量
    int val;            // 存储ND_NUM的常量值
};

// 报错函数
void error(char *fmt, ...);
void errorAt(char *loc, char *fmt, ...);

// 数据结构操作-辅助函数
bool equal(Token *tok, char *str);
Token *skip(Token *tok, char *str);

// 词法分析 入口函数
Token *tokenize(char *input);

// 语法分析 入口函数
Function *parse(Token *tok);

// 代码生成
void codegen(Function *node);