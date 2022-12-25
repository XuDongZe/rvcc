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
    // char *currentInput = "int main() { int a=3;return a; }";

    printf("# %s\n\n", currentInput);

    // 词法分析
    Token *tok = tokenize(currentInput);

    // 语法分析
    Obj *prog = parse(tok);

    // 代码生成
    codegen(prog);
    
    return 0;
}
