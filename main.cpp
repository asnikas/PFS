#include <iostream>
#include "Menu.cpp"

using namespace std;

int main(){
    Parser parser("disk.img"); //расширение можно передать любое, оно ничего не значит для программы, 
    //важно чтобы оно было в .gitignore, дабы не залить образ на репозиторий
    Superblock superblock;
    Allocator allocator(-1, 0);
    FileSystem filesystem(parser, allocator, superblock);

    if(filesystem.mount()) cout << "образ подключен, выполните login" << endl;
    else cout << "образ не найден, выполните mkfs" << endl;

    Menu menu(filesystem);

    menu.run();
}