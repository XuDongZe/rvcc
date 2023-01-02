#include "rvcc.h"

// (Type){...}构造了一个复合字面量，相当于Type的匿名变量。
// TypeKind = TY_INIT, size = 8
Type *tyInt = &(Type){TY_INT, 8};
Type *tyChar = &(Type){TY_CHAR, 1};

// 判断给定type是否是整型
bool isInteger(Type *ty)
{
    return ty->kind == TY_INT || ty->kind == TY_CHAR;
}

// 判断给定tok是否是Type类型
bool isKeywordType(Token *tok) {
    if (tok->kind != TOK_KEKWORD) {
        return false;
    }
    return equal(tok, "int") || equal(tok, "char");
}

// 构造指针类型
Type *newPointer(Type *base)
{
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_PTR;
    ty->base = base;
    ty->size = 8;
    return ty;
}

// 构造函数类型
Type *newFuncType(Type *returnTy)
{
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_FUNC;
    ty->returnTy = returnTy;
    return ty;
}

// 构造数组类型
Type *newArrayType(Type *base, int len)
{
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_ARRAY;
    ty->base = base;
    ty->arrayLen = len;
    // 数组的size是元素size * 元素数量
    ty->size = len * base->size;
    return ty;
}

// 类型复制
Type *copyType(Type *base)
{
    Type *ty = calloc(1, sizeof(Type));
    *ty = *base;
    return ty;
}

// 为ast节点node，及其子节点，递归的添加类型
void addType(Node *node)
{
    if (!node || node->type)
    {
        return;
    }

    // 递归子节点
    addType(node->lhs);
    addType(node->rhs);
    addType(node->init);
    addType(node->cond);
    addType(node->then);
    addType(node->els);
    addType(node->inc);

    // body类型
    for (Node *nd = node->body; nd; nd = nd->next)
    {
        addType(nd);
    }

    // 递归到叶子节点，处理
    switch (node->kind)
    {
    // 将节点类型设置为左节点的类型。
    // 基础字面量
    case ND_NUM:
    // 关系运算
    case ND_EQ:
    case ND_NEQ:
    case ND_LT:
    case ND_LET:
    case ND_FUNCALL:
        node->type = tyInt;
        return;
    case ND_VAR:
        // 节点的类型就是节点内变量的声明类型。是在变量声明时构造的。
        node->type = node->var->ty;
        return;
    // 节点类型为左部节点的类型
    // 取相反数
    case ND_NEG:
    // 算数运算
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
        node->type = node->lhs->type;
        return;
    // 赋值语句类型为左值的声明类型
    case ND_ASSIGN:
        // 数组不能做左值
        if (node->lhs->type->kind == TY_ARRAY)
            errorTok(node->lhs->tok, "not an lvalue");
        node->type = node->lhs->type;
        return;
    // 取地址 类型为指针。指针指向子节点
    case ND_ADDR:
        Type *ty = node->lhs->type;
        // int x[4]; int *y = &x; y的类型是 int *，是指向数组元素类型(base)的指针
        // 其实数组是语法糖 需要在编译阶段做特殊处理
        if (ty->kind == TY_ARRAY)
            node->type = newPointer(ty->base);
        else
            node->type = newPointer(node->lhs->type);
        return;
    // 解引用 此时是叶子节点，
    case ND_DEREF:
        // 子节点没有base类型。不可以解除引用操作。这里用base判断来对TY_PTR和TY_ARRAY做统一处理
        if (!node->lhs->type->base)
            errorTok(node->tok, "invalid pointer derefrence");
        // 子节点是指针，接触引用后，类型为指针指向的原数据的类型。
        node->type = node->lhs->type->base;
        return;
    default:
        break;
    }
}
