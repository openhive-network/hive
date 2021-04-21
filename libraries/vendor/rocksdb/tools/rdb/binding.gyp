{
    "targets": [
        {
            "target_name": "rdb",
            "sources": [
                "rdb.cc",
                "db_wrapper.cc",
                "db_wrapper.h"
            ],
            "cflags_cc!": [
                "-fno-exceptions"
            ],
            "include_dirs+": [
                "../../include"
            ],
            "libraries": [
                "../../../librocksdb.a",
                "-lsnappy"
            ],
        }
    ]
}
