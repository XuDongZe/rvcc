git init

git add .
git commit -m <msg>
git push origin main

git rm -f cached .

git config user.name
git config user.email
git config --global user.name "dongze"
git config --global user.email "xxx"

git config --list
git config --list | grep url

git clone
git pullc
git branch
git status
git log

## 撤销commit
```sh
git reset --soft HEAD^
```
* --soft: 不删除工作空间的代码，撤销commit，不撤销git add file
* --hard: 删除工作空间的代码，撤销commit且撤销add
* HEAD^ 相当与 HEAD～1，reset至上一个版本，即上一次commit。如果想要reset到倒数第二个版本，可以使用HEAD~2
