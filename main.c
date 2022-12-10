#include "rvcc.h"

int main(int argc, char *argv[])
{
    // 参数处理
    if (argc != 2)
    {
        error("%s: invalid number of arguments\n", argv[0]);
        return 1;
    }

    char *currentInput = argv[1];
    // currentInput = "1-8/(2*2)+3*6";
    // char *currentInput = "a=3;";

    // 词法分析
    Token *tok = tokenize(currentInput);

    // 语法分析
    Node *node = parse(tok);

    // 代码生成
    codegen(node);
    
    return 0;
}
