# 编程技巧
## 对齐函数
1. 
```c
// 返回 n对align向上对齐的值
int align(int n, int align) {
    return (n + align - 1) / align * align;
}
```
* align - 1为align为除数时的最大余数。n == 0时 成立。 n>=1时，(n + align - 1) >= align，也就是n再加(align - 1)前后，一定会跨越一个align的长度。此时数值向上步进了一个align的长度。
* /align然后再*align，消除余数。
* 首先，步进到下一个周期。然后消除余数。