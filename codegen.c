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

static void genExpr(Node *node);
static void genAddr(Node *node);
static void genStmt(Node *node);

// 计算给定节点的绝对地址：变量，函数，指针
static void genAddr(Node *node)
{
    if (node->kind == ND_VAR)
    {
        // base值为fp，fp的下一个空间为'a'
        int offset = (node->name - 'a' + 1) * 8;
        // fp为栈帧起始地址
        printf(" addi a0, fp, %d\n", -offset);
        // 此时a0的值为fp-offset。一个内存地址
        return;
    }

    error("not an address");
}

static void genStmt(Node *node)
{
    if (node->kind == ND_EXPR_STMT)
    {
        // 左侧节点。与parse一致
        genExpr(node->lhs);
        return;
    }
    error("invalid statment");
}

// 计算node的结果，保存在a0寄存器中
static void genExpr(Node *node)
{
    switch (node->kind)
    {
    case ND_NUM:
        // return: a0=node->val
        printf(" li a0, %d\n", node->val);
        return;
    case ND_VAR:
        // return:a0=node->name变量的值
        // 计算node内变量的内存地址，保存到a0
        genAddr(node);
        // 访问a0地址中存储的数据，加载到a0
        printf(" ld a0, 0(a0)\n");
        return;
    case ND_NEG:
        // return: a0为node的结果取反
        // 计算node的子树结果
        genExpr(node->lhs);
        // 此时子树的结果保存在a0中。
        // 最后对结果取反
        printf(" neg a0, a0\n");
        return;
    case ND_ASSIGN:
        // 计算方向：先处理地址
        // 计算左值的地址，保存到a0。与ND_VAR不同，ND_VAR是将左值保存到a0中。
        genAddr(node->lhs);
        // 地址压栈
        push();
        // 将右值保存到a0
        genExpr(node->rhs);
        // 左值地址弹栈
        pop("a1");
        // 此时a1为左值地址，a0为右值。将a0寄存器内存放的值，存储到a1地址指向的内存处。
        printf(" sd a0, 0(a1)\n");
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

    // 栈分配
    //----------------------// sp
    //  fp旧值，用于恢复fp
    //----------------------// fp=sp
    //  'a'                 // fp-8
    //  'b'                 // fp-16
    //  ...                 // fp-(n-'a'+1)*8
    //  'z'                 // fp-26*8 = fp-208
    //----------------------// sp = fp-208-8
    //  表达式生成
    //----------------------//

    // 问题：为何要用fp保存sp，而不用栈空间保存sp呢？

    // Prologue 前言
    // 栈内存分配
    // 将fp的旧值压栈，随后将sp的值保存到fp 后续sp可以变化进行压栈操作了。所以sp->fp两个寄存器，分割出来一部分栈空间。
    printf(" addi sp, sp, -8\n");
    printf(" sd fp, 0(sp)\n");
    printf(" mv fp, sp\n");
    // 在栈中分配变量表的内存空间
    printf(" addi sp, sp, -208\n");

    // 代码生成
    for (Node *n = node; n; n = n->next)
    {
        genStmt(n);
        // 如果stack未清空 报错
        assert(depth == 0);
    }

    // Epilogue 后语
    // 恢复sp
    printf(" mv sp, fp\n");
    // 恢复fp
    pop("fp");

    // 将a0返回
    printf(" ret\n");
}