#include "rvcc.h"

int main(int argc, char *argv[])
{
    // 参数处理
    if (argc != 2)
    {
        error("%s: invalid number of arguments\n", argv[0]);
        return 1;
    }

    char *s = argv[1];
    // char *s = "int main() { return \"\"[0]; }";

    // 词法分析
    Token *tok = tokenize(s);

    // 语法分析
    Obj *prog = parse(tok);

    // 代码生成
    codegen(prog);
    
    return 0;
}
