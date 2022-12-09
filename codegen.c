#include "rvcc.h"

// 栈深度记录
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

// 弹栈 到指定寄存器
static void pop(char *reg)
{
    // load double,
    printf(" ld %s, 0(sp)\n", reg);
    printf(" addi sp, sp, 8\n");
    depth--;
}

static void genStmt(Node *node);
static void genExpr(Node *node);

static void genStmt(Node *node) {
    if (node->kind == ND_EXPR_STMT) {
        // 左侧节点。与parse一致
        genExpr(node->lhs);
        return;
    }
    error("invalid statment");
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
        // a0<=a1 == 1 => !(a0>a1)
        printf(" slt a0, a1, a0\n");
        // 此时a0的值为1或者0, 对a0取反 相当于 a0^1
        printf(" xori a0, a0, 1\n");
        return;
    default:
        error("invalid expression");
        return;
    }
}

void codegen(Node *node)
{
    // 声明一个全局main段，同时也是程序入口段
    printf("  .global main\n");
    printf("main:\n");

    // 代码生成
    for (Node *n=node; n; n = n->next) {
        genStmt(n);
        // 如果stack未清空 报错
        assert(depth == 0);
    }

    // 将a0返回
    printf(" ret\n");
}