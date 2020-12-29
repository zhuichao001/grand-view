#include <iostream>
#include <cassert>
#include <cstdlib>
#include <string>
#include <leveldb/db.h>
#include <leveldb/status.h>

using namespace leveldb;

int main(void) {
    leveldb::DB *db = nullptr;
    leveldb::Options options;

    options.create_if_missing = true;

    leveldb::Status status = DB::Open(options, "/tmp/testdb", &db);
    assert(status.ok());

    std::string key = "foo";
    std::string val = "bar";
    std::string out;

    leveldb::Status s = db->Put(WriteOptions(), key, val);

    if (s.ok()){
        s = db->Get(ReadOptions(), key, &out);
    }

    if (s.ok()){
        std::cout << out << std::endl;
    } else {
        std::cout << s.ToString() << std::endl;
    }

    delete db;
    return 0;
}

