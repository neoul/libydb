#include <iostream>
#include <fstream>

#include <ylog.h>
#include <ydbcpp.h>

using namespace std;

static int done;
void HANDLER_SIGINT(int signal)
{
    done = 1;
}
const char *yaml_example = 
"a:\n"
" b: c\n"
" d: f\n"
"g:\n"
" - h\n"
" - i\n";

int main(int argc, char *argv[])
{
    Ydb db((char *)"hello-ydb");
    db.write((char *)yaml_example);
    std::cin >> db;
    std::cout << db;
    return 0;
}