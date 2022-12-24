#include "rvcc.h"

// 栈深度记录
static int depth;
// 寄存器表
static char *argRegs[] = {"a0", "a1", "a2", "a3", "a4", "a5", "a6"};
static char *regA0 = "a0";
static char *regA1 = "a1";
// 当前函数
static Function *currentFn;

// 压栈，结果临时压入栈中保存
// sp为栈顶指针 stack pointer，栈反向向下增长。64位下，一个单位8个字节。
// 不使用寄存器，因为需要存储的值的数量是变化的
static void push()
{
    printf("# a0压栈[%d]\n", depth + 1);
    printf(" addi sp, sp, -8\n");
    printf(" sd a0, 0(sp)\n");
    // 记录当前栈深度
    depth++;
}

// 弹栈 到指定寄存器
static void pop(char *reg)
{
    // load double,
    printf(" # [%d]出栈,保存到寄存器%s\n", depth, reg);
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
    // 为每一个函数的本地变量表分配栈空间
    for (Function *func = prog; func; func = func->next)
    {
        int offset = 0;
        // 遍历处理所有本地变量
        for (Obj *var = func->locals; var; var = var->next)
        {
            // todo: locals为语法树节点，头插法构建。链表中第一个var为最后处理的ast节点。也就是token表中的顺序。
            // 每个本地变量分配8个字节的固定空间
            offset += var->ty->size;
            // 栈向下增长。地址变小。offset是负数。
            var->offset = -offset;
        }
        // 栈大小 调整为16字节对齐
        func->stackSize = alignTo(offset, 16);
    }
}

static int count()
{
    static int i = 1;
    return i++;
}

static void genAddr(Node *node);
static void genExpr(Node *node);
static void genStmt(Node *node);
// 访问a0（为一个地址）指向的栈地址中存储的数据，加载到a0
static void load(Node *node);
// 将栈顶值(为一个地址)存入a0
static void store(void);

// 访问a0地址中存储的数据，加载到a0
static void load(Node *node)
{
    if (node->type->kind == TY_ARRAY)
    {
        // 数组类型不加载
        return;
    }
    printf("# a0=mem[a0]\n");
    printf(" ld a0, 0(a0)\n");
}

// 将栈顶值(为一个地址)存入a0
static void store(void)
{
    // 左值地址弹栈
    pop("a1");
    // 此时a1为左值地址，a0为右值。将a0寄存器内存放的值，存储到a1地址指向的内存处。
    printf(" # a0=mem[a1]\n");
    printf(" sd a0, 0(a1)\n");
};

// 计算给定节点的绝对地址：变量，函数，指针
static void genAddr(Node *node)
{
    switch (node->kind)
    {
    case ND_VAR:
        // fp为栈帧起始地址
        printf("# 加载变量%s的栈内地址到a0\n", node->var->name);
        printf(" addi a0, fp, %d\n", node->var->offset);
        // 此时a0的值为fp+offset。var的内存地址
        return;
        break;
    case ND_DEREF:
        // 解引用，与取地址抵消了
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
        load(node);
        return;
    case ND_FUNCALL:
        // funcall前言，准备参数堆栈。
        // a0-参数1，a1-参数2，a2-参数3...

        printf("# 调用函数%s前: 实参传递到寄存器\n", node->funcName);
        // 压栈的顺序为文本参数顺序
        int n = 0;
        for (Node *arg = node->args; arg; arg = arg->next)
        {
            // 计算表达式的值
            genExpr(arg);
            // 压栈
            push();
            n++;
        }
        // 反向出栈
        for (int i = n - 1; i >= 0; i--)
            pop(argRegs[i]);

        // 函数调用系统调用
        printf("# 调用函数%s\n", node->funcName);
        printf(" call %s\n", node->funcName);
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
        load(node);
        return;
    case ND_ASSIGN:
        // 计算方向：先处理地址
        // 计算左值的地址，保存到a0。与ND_VAR不同，ND_VAR是将左值保存到a0中。
        genAddr(node->lhs);
        // 地址压栈
        push();
        // 将右值保存到a0
        genExpr(node->rhs);
        store();
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
        printf("# a0=a0+a1\n");
        printf(" add a0, a0, a1\n");
        return;
    case ND_SUB:
        printf("# a0=a0-a1\n");
        printf(" sub a0, a0, a1\n");
        return;
    case ND_MUL:
        printf("# a0=a0*a1\n");
        printf(" mul a0, a0, a1\n");
        return;
    case ND_DIV:
        printf("# a0=a0/a1\n");
        printf(" div a0, a0, a1\n");
        return;
    case ND_EQ:
        // a0 = a0 ^ a1
        printf("# a0=a0^a1\n");
        printf(" xor a0, a0, a1\n");
        // 如果a0寄存器的值为0, 即a0 == a1, 置a0为1
        printf("# 如果a0寄存器的值为0, 即a0 == a1, 则a0=1\n");
        printf(" seqz a0, a0\n");
        return;
    case ND_NEQ:
        printf("# a0=a0^a1\n");
        printf(" xor a0, a0, a1\n");
        // 如果a0寄存器的值不为0, 即a0 != a1, 置a0为1
        printf("# 如果a0寄存器的值为0, 即a0 != a1, 则a0=1\n");
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
        printf("# 此时a0的值为1或者0, 对a0取反 相当于 a0^1\n");
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
        printf(" # 跳转到.L.return.%s段\n", currentFn->name);
        printf(" j .L.return.%s\n", currentFn->name);
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
    // printf("# ========栈帧分配: 本地变量表==========\n");
    // 在栈中分配变量表的内存空间: 映射变量和栈地址
    assignLVarOffset(prog);

    // 遍历每一个函数，进行代码生成
    for (Function *func = prog; func; func = func->next)
    {
        // 声明全局main段: 程序入口段
        printf("\n# ==========%s段开始================\n", func->name);
        printf("# 声明全局%s段\n", func->name);
        printf(" .global %s\n", func->name);
        printf("%s:\n", func->name);

        // 全局函数指针
        currentFn = func;

        // 栈分配
        //----------------------// sp
        // ra
        //----------------------// ra = sp - 8
        // fp
        //---------------------- //fp = sp - 16
        //  本地变量表
        //----------------------// sp = sp - 16 - stackSize
        //  表达式生成
        //----------------------//

        // Prologue 前言
        // 栈内存分配
        // 将fp的旧值压栈，随后将sp的值保存到fp 后续sp可以变化进行压栈操作了。所以sp->fp两个寄存器，分割出来一部分栈空间。
        // 问题：为何要用fp保存sp，而不用栈空间保存sp呢：因为栈空间只能访问栈顶，而fp寄存器可以随时访问。
        // 后续计算栈内本地变量地址的时候，需要直接使用寄存器作为base值。所以选择fp来保存栈基址。
        // fp: 程序栈帧基址(frame pointer)，sp: 栈顶指针

        // 保存ra
        printf("# ra压栈\n");
        printf(" addi sp, sp, -8\n");
        printf(" sd ra, 0(sp)\n");
        // 保存fp
        printf("# fp压栈\n");
        printf(" addi sp, sp, -8\n");
        printf(" sd fp, 0(sp)\n");
        // 保存sp
        printf("# 加载sp到fp\n");
        printf(" mv fp, sp\n");

        // 调整栈顶指针
        printf("# 分配本地变量表栈空间(对齐到16字节)\n");
        printf(" addi sp, sp, -%d\n", func->stackSize);

        printf("# ===将预先存储的实参列表从寄存器加载到对应的栈空间中===\n");
        int i = 0;
        for (Obj *param = func->params; param; param = param->next)
        {
            // FUNCALL指令中 call汇编指令调用之前 第一个实参保存在a0中，第二个保存在a1中，以此类推
            printf("# 从寄存器%s中加载参数%s到栈中\n", argRegs[i], param->name);
            printf("sd %s, %d(fp)\n", argRegs[i++], param->offset);
        }

        printf("# ========%s段主体代码==========\n", func->name);
        genStmt(func->body);

        // return标签
        printf(" # return段标签\n");
        printf(".L.return.%s:\n", func->name);
        printf("# ========%s段结束==========\n", func->name);

        // Epilogue 后语
        printf("# ========上下文恢复: 寄存器==========\n");
        // 恢复sp
        printf(" # 恢复sp, 从fp中\n");
        printf(" mv sp, fp\n");
        // 恢复fp
        printf(" # 恢复fp, 从栈中\n");
        printf(" ld fp, 0(sp)\n");
        printf(" addi sp, sp, 8\n");
        // 恢复ra
        printf(" # 恢复ra\n");
        printf(" ld ra, 0(sp)\n");
        printf(" addi sp, sp, 8\n");

        // 如果stack未清空 报错
        assert(depth == 0);

        // 系统调用返回
        printf("# 将a0值返回给系统调用\n");
        printf(" ret\n");
    }
}