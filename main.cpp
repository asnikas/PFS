#include <iostream>
#include <cstdio>
#include "Parser.h"
#include "Superblock.h"
#include "Allocator.h"
#include "FileSystem.h"
#include "Menu.h"

#ifdef _WIN32
    #include <io.h> //_isatty (ненулевое значение, если ввод идет из терминала), _fileno
#else
    #include <unistd.h> //isatty, fileno
#endif

using namespace std;

int main(){
    #ifdef _WIN32
        bool is_interactive = _isatty(_fileno(stdin)) != 0;
    #else
        bool is_interactive = isatty(fileno(stdin)) != 0;
    #endif

    Parser parser("disk.img"); //расширение можно передать любое, оно ничего не значит для программы, 
    //важно чтобы оно было в .gitignore, дабы не залить образ на репозиторий
    Superblock superblock;
    Allocator allocator(parser, -1, 0);
    FileSystem filesystem(parser, allocator, superblock);

    bool fs_mount = filesystem.mount();

    Menu menu(filesystem, is_interactive);

    if(is_interactive){
        if(fs_mount) cout << "образ подключен, выполните login" << endl;
        else cout << "образ не найден, выполните mkfs" << endl;
    }

    menu.run();
}