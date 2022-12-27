#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <iostream>

#include "doubly_buffered_data.h"

using namespace std;

struct DbdTest {
    int _index = 0;
    string _body = "";
};

static bool AddDbd(DbdTest& bg, int index, string body) {
    bg._index = index;
    bg._body = body;
    return true;
}

int main() {
    butil::DoublyBufferedData<DbdTest> dbd;

    dbd.Modify(AddDbd, 1, "test-1");
    {
        butil::DoublyBufferedData<DbdTest>::ScopedPtr s;
        if (dbd.Read(&s) != 0) {
            return -1;
        }
        printf("dbd read index:%d body:%s\n", s->_index, s->_body.c_str());
    }
       

    dbd.Modify(AddDbd, 2, "test-2");
    {
        butil::DoublyBufferedData<DbdTest>::ScopedPtr s;
        if (dbd.Read(&s) != 0) {
            return -1;
        }
        printf("dbd read index:%d body:%s\n", s->_index, s->_body.c_str());
    }
    return 0;
}
