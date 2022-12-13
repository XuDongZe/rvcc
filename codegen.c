#include "rvcc.h"

// 栈深度记录
static int depth;

// 压栈，结果临时压入栈中保存
// sp为栈顶指针 stack pointer，栈反向向下增长。64位下，一个单位8个字节。
// 不使用寄存器，因为需要存储的值的数量是变化的
static void push(void)
{
    printf(" # a0压栈\n");
    printf(" addi sp, sp, -8\n");
    printf(" sd a0, 0(sp)\n");
    // 记录当前栈深度
    depth++;
}

// 弹栈 到指定寄存器
static void pop(char *reg)
{
    // load double,
    printf(" # 出栈到%s\n", reg);
    printf(" ld %s, 0(sp)\n", reg);
    printf(" addi sp, sp, 8\n");
    depth--;
}

// 向上对齐到align的整数倍
static int alignTo(int n, int align)
{
    // 当align=16时：
    // 0 => 0, 1=>16, 16=>16, 17=>2*16
    // int base = n / align;
    // if (n % align != 0) {
    //     base ++;
    // }
    // return base * align;
    return ((n + align - 1) / align) * align;
}

static void assignLVarOffset(Function *prog)
{
    int offset = 0;
    for (Obj *var = prog->locals; var; var = var->next)
    {
        // todo: locals为语法树节点，头插法构建。链表中第一个var为最后处理的ast节点。也就是token表中的顺序。
        offset += 8;
        // 栈向下增长。地址变小。offset是负数。
        var->offset = -offset;
    }
    // 栈大小 调整为16字节对齐
    prog->stackSize = alignTo(offset, 16);
}

static int count()
{
    static int i = 1;
    return i++;
}

static void genAddr(Node *node);
static void genExpr(Node *node);
static void genStmt(Node *node);

// 计算给定节点的绝对地址：变量，函数，指针
static void genAddr(Node *node)
{
    switch (node->kind)
    {
    case ND_VAR:
        // fp为栈帧起始地址
        printf(" # 将变量的栈帧偏移地址加载到a0\n");
        printf(" addi a0, fp, %d\n", node->var->offset);
        // 此时a0的值为fp+offset。var的内存地址
        return;
        break;
    case ND_DEREF:
        // 解引用
        genExpr(node->lhs);
        return;
    default:
        break;
    }

    errorTok(node->tok, "not a lvalue");
}

// 计算node的结果，保存在a0寄存器中
static void genExpr(Node *node)
{
    switch (node->kind)
    {
    case ND_NUM:
        // return: a0=node->val
        printf(" # 加载%d到a0\n", node->val);
        printf(" li a0, %d\n", node->val);
        return;
    case ND_VAR:
        // return:a0=node->name变量的值
        // 计算node内变量的内存地址，保存到a0
        genAddr(node);
        // 访问a0地址中存储的数据，加载到a0
        printf(" #访问a0地址指向的内存数据，加载到a0\n");
        printf(" ld a0, 0(a0)\n");
        return;
    case ND_NEG:
        // return: a0为node的结果取反
        // 计算node的子树结果
        genExpr(node->lhs);
        // 此时子树的结果保存在a0中。
        printf(" #对a0取相反数\n");
        printf(" neg a0, a0\n");
        return;
    case ND_ADDR:
        // 将node子节点是一个变量 将其内存地址保存到a0
        genAddr(node->lhs);
        return;
    case ND_DEREF:
        // 解除引用: node->lhs是一个变量的地址，将地址计算，保存到a0。
        genExpr(node->lhs);
        // 此时a0中保存着一个变量，将变量对应的值加载到a0。
        // y = &x; *y; *y时：计算y值时，得到x的地址。返回*addr, 再次load得到x的值。
        printf(" ld a0, 0(a0)\n");
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
        printf(" # 访问a1内地址指向的内存数据, 加载到a0\n");
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
        errorTok(node->tok, "invalid expression");
        return;
    }
}

static void genStmt(Node *node)
{
    switch (node->kind)
    {
    case ND_EXPR_STMT:
        // 左侧节点。与parse一致
        genExpr(node->lhs);
        return;
    case ND_RETURN:
        // 先计算子表达式值
        genStmt(node->lhs);
        // 程序返回: 跳转到标签处
        printf(" j .L.return\n");
        return;
    case ND_BLOCK:
        // 依次计算body指向的stmt列表
        // body 可能是空列表: "{}" || ";" => do nothing
        for (Node *p = node->body; p; p = p->next)
        {
            genStmt(p);
        }
        return;
    case ND_IF:
    {
        // 分配if语句的id
        int c = count();
        // 计算cond expr
        genExpr(node->cond);
        // 此时cond结果保存在a0寄存器。判定a0,如果满足条件继续执行，不满足条件，跳转到else入口标签。
        printf(" beqz a0, .L.else.%d\n", c);
        // 满足条件，继续执行then stmt
        genStmt(node->then);
        // then执行完毕，无条件跳转到if出口标签
        printf(" j .L.end.%d\n", c);
        // 不满足条件，else入口标签
        printf(".L.else.%d:\n", c);
        // 执行else stmt。else部分是可选的
        if (node->els)
        {
            genStmt(node->els);
        }
        // 生成if出口标签
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_FOR:
    {
        int c = count();
        // init expr，init语句的返回丢弃，不使用。可选
        if (node->init)
        {
            genExpr(node->init);
        }
        // for循环入口标签。
        printf(".L.begin.%d:\n", c);
        // cond expr. 可选。没有条件，默认true
        if (node->cond)
        {
            genExpr(node->cond);
        }
        else
        {
            printf(" li a0, 1\n");
        }
        // 判断条件，如果为0则跳转到for结束标签
        printf(" beqz a0, .L.end.%d\n", c);
        // 执行循环体
        genStmt(node->then);
        // incr expr 可选。
        if (node->inc)
        {
            genExpr(node->inc);
        }
        // 跳转到for开始标签
        printf(" j .L.begin.%d\n", c);
        // for结束标签
        printf(".L.end.%d:\n", c);
        return;
    }
    default:
        break;
    }
    errorTok(node->tok, "invalid statment");
}

void codegen(Function *prog)
{
    // 声明全局main段: 程序入口段
    printf("# 声明全局main段: 程序入口段\n");
    printf(" .global main\n");
    printf("main:\n");

    // 栈分配
    //----------------------// sp
    //  fp旧值，用于恢复fp
    //---------------------- //fp=sp
    //  本地变量表            // fp-8
    //----------------------// sp = fp-stackSize-8
    //  表达式生成
    //----------------------//

    // Prologue 前言

    printf("# ========栈帧内存分配==========\n");
    // 栈内存分配
    // 将fp的旧值压栈，随后将sp的值保存到fp 后续sp可以变化进行压栈操作了。所以sp->fp两个寄存器，分割出来一部分栈空间。
    // 问题：为何要用fp保存sp，而不用栈空间保存sp呢：因为栈空间只能访问栈顶，而fp寄存器可以随时访问。
    // 后续计算栈内本地变量地址的时候，需要直接使用寄存器作为base值。所以选择fp来保存栈基址。
    // fp: 程序栈帧基址(frame pointer)，sp: 栈顶指针
    printf("# ========上下文保护: 寄存器==========\n");
    // 保存fp
    printf("# 保存fp: 到栈帧中\n");
    printf(" addi sp, sp, -8\n");
    printf(" sd fp, 0(sp)\n");
    // 保存sp
    printf("# 保存sp: 加载到fp\n");
    printf(" mv fp, sp\n");

    printf("# ========栈帧分配: 本地变量表==========\n");
    // 在栈中分配变量表的内存空间: 映射变量和栈地址
    assignLVarOffset(prog);
    // 调整栈顶指针
    printf(" addi sp, sp, -%d\n", prog->stackSize);

    printf("# ========代码生成==========\n");
    for (Node *n = prog->body; n; n = n->next)
    {
        genStmt(n);
        // 如果stack未清空 报错
        assert(depth == 0);
    }

    printf(" # return标签\n");
    printf(".L.return:\n");

    // Epilogue 后语
    printf("# ========上下文恢复: 寄存器==========\n");
    // 恢复sp
    printf(" # 恢复sp, 从fp中\n");
    printf(" mv sp, fp\n");
    // 恢复fp
    printf(" # 恢复fp, 从栈中\n");
    pop("fp");

    printf("# ========将a0值返回给系统调用==========\n");
    printf(" ret\n");
}