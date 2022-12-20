#include "rvcc.h"

// (Type){...}构造了一个复合字面量，相当于Type的匿名变量。
Type *tyInt = &(Type){TY_INT, NULL};

// 判断给定type是否是整型
bool isInteger(Type *ty)
{
    return ty->kind == TY_INT;
}

// 构造指针类型
Type *newPointer(Type *base)
{
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_PTR;
    ty->base = base;
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
        node->type = tyInt;
        return;
    case ND_VAR:
        // 节点的类型就是节点内变量的声明类型。是在变量声明时构造的。
        node->type = node->var->ty;
        return;
    // 取相反数
    case ND_NEG:
    // 算数运算
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    // 赋值语句类型为左值的声明类型
    case ND_ASSIGN:
        node->type = node->lhs->type;
        return;
    // 取地址 类型为指针。指针指向子节点
    case ND_ADDR:
        node->type = newPointer(node->lhs->type);
        return;
    // 解引用 此时是叶子节点，
    case ND_DEREF:
        // 子节点是指针，接触引用后，类型为指针指向的原数据的类型。
        // y = &x; 则*y类型是x的类型。
        if (node->lhs->type->kind == TY_PTR)
        {
            node->type = node->lhs->type->base;
        }
        else
        {
            // todo
            node->type = TY_INT;
        }
        return;
    default:
        break;
    }
}
