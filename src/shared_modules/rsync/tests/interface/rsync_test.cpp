/*
 * Wazuh DBSYNC
 * Copyright (C) 2015-2020, Wazuh Inc.
 * August 28, 2020.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include <iostream>
#include "rsync_test.h"
#include "rsync.h"
#include "dbsync.h"

constexpr auto DATABASE_TEMP {"TEMP.db"};
constexpr auto SQL_STMT_INFO
{
    R"(
    PRAGMA foreign_keys=OFF;
    BEGIN TRANSACTION;
    CREATE TABLE entry_path (path TEXT NOT NULL, inode_id INTEGER, mode INTEGER, last_event INTEGER, entry_type INTEGER, scanned INTEGER, options INTEGER, checksum TEXT NOT NULL, PRIMARY KEY(path));
    INSERT INTO entry_path VALUES('/boot/grub2/fonts/unicode.pf2',1,0,1596489273,0,1,131583,'96482cde495f716fcd66a71a601fbb905c13b426');
    INSERT INTO entry_path VALUES('/boot/grub2/grubenv',2,0,1596489273,0,1,131583,'e041159610c7ec18490345af13f7f49371b56893');
    INSERT INTO entry_path VALUES('/boot/grub2/i386-pc/datehook.mod',3,0,1596489273,0,1,131583,'f83bc87319566e270fcece2fae4910bc18fe7355');
    INSERT INTO entry_path VALUES('/boot/grub2/i386-pc/gcry_whirlpool.mod',4,0,1596489273,0,1,131583,'d59ffd58d107b9398ff5a809097f056b903b3c3e');
    INSERT INTO entry_path VALUES('/boot/grub2/i386-pc/gzio.mod',5,0,1596489273,0,1,131583,'e4a541bdcf17cb5435064881a1616befdc71f871');
    CREATE INDEX path_index ON entry_path (path);
    CREATE INDEX inode_index ON entry_path (inode_id);
    COMMIT;)"
};

class CallbackMock
{
public:
    CallbackMock() = default;
    ~CallbackMock() = default;
    MOCK_METHOD(void, callbackMock, (const std::string&), ());
};

static void callback(const void* data,
                     const size_t /*size*/,
                     void* ctx)
{
    CallbackMock* wrapper { reinterpret_cast<CallbackMock*>(ctx)};
    wrapper->callbackMock(reinterpret_cast<const char *>(data));
}

struct CJsonDeleter
{
    void operator()(char* json)
    {
        cJSON_free(json);
    }
    void operator()(cJSON* json)
    {
        cJSON_Delete(json);
    }
};

static void logFunction(const char* msg)
{
    if (msg)
    {
        std::cout << msg << std::endl;
    }
}

void RSyncTest::SetUp()
{
    rsync_initialize(&logFunction);
};

void RSyncTest::TearDown()
{
    EXPECT_NO_THROW(rsync_teardown());
};

TEST_F(RSyncTest, Initialization)
{
    const auto handle { rsync_create() };
    ASSERT_NE(nullptr, handle);
}

TEST_F(RSyncTest, startSync)
{
    const auto handle { rsync_create() };
    ASSERT_EQ(0, rsync_start_sync(handle));
}

TEST_F(RSyncTest, registerSyncId)
{
    const auto handle { rsync_create() };
    ASSERT_EQ(-1, rsync_register_sync_id(handle, nullptr, nullptr, nullptr, {}));
}

TEST_F(RSyncTest, pushMessage)
{
    const std::string buffer{"test buffer"};
    const auto handle { rsync_create() };
    ASSERT_NE(0, rsync_push_message(handle, nullptr, 1000));
    ASSERT_NE(0, rsync_push_message(handle, reinterpret_cast<const void*>(0x1000), 0));
    ASSERT_EQ(0, rsync_push_message(handle, reinterpret_cast<const void*>(buffer.data()), buffer.size()));
}

TEST_F(RSyncTest, CloseWithoutInitialization)
{   
    EXPECT_EQ(-1, rsync_close(nullptr));
}

TEST_F(RSyncTest, CloseCorrectInitialization)
{
    const auto handle { rsync_create() };
    ASSERT_NE(nullptr, handle);
    EXPECT_EQ(0, rsync_close(handle));
}

TEST_F(RSyncTest, RegisterAndPush)
{
    const auto handle_dbsync { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, SQL_STMT_INFO) };
    ASSERT_NE(nullptr, handle_dbsync);

    const auto handle_rsync { rsync_create() };
    ASSERT_NE(nullptr, handle_rsync);

    const auto expectedResult1
    {
        R"({"component":"test_component","data":{"begin":"/boot/grub2/fonts/unicode.pf2","checksum":"2d567d2a180a96ad6b3ecd9ec7beae31d103d090280e7eaec8383ef27c8ab4a5","end":"/boot/grub2/grubenv","id":1,"tail":"/boot/grub2/i386-pc/datehook.mod"},"type":"integrity_check_left"})"
    };

    const auto expectedResult2
    {
        R"({"component":"test_component","data":{"begin":"/boot/grub2/i386-pc/datehook.mod","checksum":"cc933107bbe6c3eee784b74e180b9da2dbfa6766807aa1483257f055e52e4ca9","end":"/boot/grub2/i386-pc/gzio.mod","id":1},"type":"integrity_check_right"})"
    };
    
    const auto expectedResult3
    {
        R"({"component":"test_component","data":{"attributes":{"checksum":"96482cde495f716fcd66a71a601fbb905c13b426","entry_type":0,"inode_id":1,"last_event":1596489273,"mode":0,"options":131583,"path":"/boot/grub2/fonts/unicode.pf2","scanned":1},"index":"/boot/grub2/fonts/unicode.pf2","timestamp":1596489273},"type":"state"})"
    };

    const auto expectedResult4
    {
        R"({"component":"test_component","data":{"attributes":{"checksum":"e041159610c7ec18490345af13f7f49371b56893","entry_type":0,"inode_id":2,"last_event":1596489273,"mode":0,"options":131583,"path":"/boot/grub2/grubenv","scanned":1},"index":"/boot/grub2/grubenv","timestamp":1596489273},"type":"state"})"
    };

    const auto expectedResult5
    {
        R"({"component":"test_component","data":{"attributes":{"checksum":"e4a541bdcf17cb5435064881a1616befdc71f871","entry_type":0,"inode_id":5,"last_event":1596489273,"mode":0,"options":131583,"path":"/boot/grub2/i386-pc/gzio.mod","scanned":1},"index":"/boot/grub2/i386-pc/gzio.mod","timestamp":1596489273},"type":"state"})"
    };

    const auto expectedResult6
    {
        R"({"component":"test_component","data":{"attributes":{"checksum":"d59ffd58d107b9398ff5a809097f056b903b3c3e","entry_type":0,"inode_id":4,"last_event":1596489273,"mode":0,"options":131583,"path":"/boot/grub2/i386-pc/gcry_whirlpool.mod","scanned":1},"index":"/boot/grub2/i386-pc/gcry_whirlpool.mod","timestamp":1596489273},"type":"state"})"
    };

    const auto expectedResult7
    {
        R"({"component":"test_component","data":{"attributes":{"checksum":"f83bc87319566e270fcece2fae4910bc18fe7355","entry_type":0,"inode_id":3,"last_event":1596489273,"mode":0,"options":131583,"path":"/boot/grub2/i386-pc/datehook.mod","scanned":1},"index":"/boot/grub2/i386-pc/datehook.mod","timestamp":1596489273},"type":"state"})"
    };

    const auto registerConfigStmt
    {
        R"({"decoder_type":"JSON_RANGE",
            "table":"entry_path",
            "component":"test_component",
            "index":"path",
            "last_event":"last_event",
            "checksum_field":"checksum",
            "no_data_query_json":
                {
                    "row_filter":" ",
                    "column_list":["path, inode_id, mode, last_event, entry_type, scanned, options, checksum"],
                    "distinct_opt":false,
                    "order_by_opt":""
                },
            "count_range_query_json":
                {
                    "row_filter":"WHERE path BETWEEN '?' and '?' ORDER BY path",
                    "count_field_name":"count",
                    "column_list":["count(*) AS count "],
                    "distinct_opt":false,
                    "order_by_opt":""
                },
            "row_data_query_json":
                {
                    "row_filter":"WHERE path ='?'",
                    "column_list":["path, inode_id, mode, last_event, entry_type, scanned, options, checksum"],
                    "distinct_opt":false,
                    "order_by_opt":""
                },
            "range_checksum_query_json":
                {
                    "row_filter":"WHERE path BETWEEN '?' and '?' ORDER BY path",
                    "column_list":["path, inode_id, mode, last_event, entry_type, scanned, options, checksum"],
                    "distinct_opt":false,
                    "order_by_opt":""
                }
        })"
    };

    CallbackMock wrapper;
    
    sync_callback_data_t callbackData { callback, &wrapper };
    EXPECT_CALL(wrapper, callbackMock(expectedResult1)).Times(1);
    EXPECT_CALL(wrapper, callbackMock(expectedResult2)).Times(1);
    EXPECT_CALL(wrapper, callbackMock(expectedResult3)).Times(2);
    EXPECT_CALL(wrapper, callbackMock(expectedResult4)).Times(1);
    EXPECT_CALL(wrapper, callbackMock(expectedResult5)).Times(1);
    EXPECT_CALL(wrapper, callbackMock(expectedResult6)).Times(1);
    EXPECT_CALL(wrapper, callbackMock(expectedResult7)).Times(1);

    const std::unique_ptr<cJSON, CJsonDeleter> spRegisterConfigStmt{ cJSON_Parse(registerConfigStmt) };
    ASSERT_EQ(0, rsync_register_sync_id(handle_rsync, "test_id", handle_dbsync, spRegisterConfigStmt.get(), callbackData));
    
    std::string buffer1{R"(test_id checksum_fail {"begin":"/boot/grub2/fonts/unicode.pf2","end":"/boot/grub2/i386-pc/gzio.mod","id":1})"};

    ASSERT_EQ(0, rsync_push_message(handle_rsync, reinterpret_cast<const void*>(buffer1.data()), buffer1.size()));

    std::string buffer2{R"(test_id checksum_fail {"begin":"/boot/grub2/fonts/unicode.pf2","end":"/boot/grub2/fonts/unicode.pf2","id":1})"};
    ASSERT_EQ(0, rsync_push_message(handle_rsync, reinterpret_cast<const void*>(buffer2.data()), buffer2.size()));

    std::string buffer3{R"(test_id no_data {"begin":"/boot/grub2/fonts/unicode.pf2","end":"/boot/grub2/i386-pc/gzio.mod","id":1})"};
    ASSERT_EQ(0, rsync_push_message(handle_rsync, reinterpret_cast<const void*>(buffer3.data()), buffer3.size()));

    EXPECT_EQ(0, rsync_close(handle_rsync));
}

TEST_F(RSyncTest, RegisterIncorrectQueryAndPush)
{
    const auto handle_dbsync { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, SQL_STMT_INFO) };
    ASSERT_NE(nullptr, handle_dbsync);

    const auto handle_rsync { rsync_create() };
    ASSERT_NE(nullptr, handle_rsync);

    const auto registerConfigStmt
    {
        R"({"decoder_type":"JSON_RANGE",
            "table":"entry_path",
            "component":"test_component",
            "index":"path",
            "last_event":"last_event",
            "checksum_field":"checksum",
            "no_data_query_json":
                {
                    "row_filter":" ",
                    "column_list":["pathx, inode_id, mode, last_event, entry_type, scanned, options, checksum"],
                    "distinct_opt":false,
                    "order_by_opt":""
                },
            "count_range_query_json":
                {
                    "row_filter":"WHEREx path BETWEEN '?' and '?' ORDER BY path",
                    "count_field_name":"count",
                    "column_list":["count(*) AS count "],
                    "distinct_opt":false,
                    "order_by_opt":""
                },
            "row_data_query_json":
                {
                    "row_filter":"WHEREx path ='?'",
                    "column_list":["path, inode_id, mode, last_event, entry_type, scanned, options, checksum"],
                    "distinct_opt":false,
                    "order_by_opt":""
                },
            "range_checksum_query_json":
                {
                    "row_filter":"WHEREx path BETWEEN '?' and '?' ORDER BY path",
                    "column_list":["path, inode_id, mode, last_event, entry_type, scanned, options, checksum"],
                    "distinct_opt":false,
                    "order_by_opt":""
                }
        })"
    };

    CallbackMock wrapper;
    
    sync_callback_data_t callbackData { callback, &wrapper };

    const std::unique_ptr<cJSON, CJsonDeleter> spRegisterConfigStmt{ cJSON_Parse(registerConfigStmt) };
    ASSERT_EQ(0, rsync_register_sync_id(handle_rsync, "test_id", handle_dbsync, spRegisterConfigStmt.get(), callbackData));
    
    std::string buffer1{R"(test_id checksum_fail {"begin":"/boot/grub2/fonts/unicode.pf2","end":"/boot/grub2/i386-pc/gzio.mod","id":1})"};

    ASSERT_EQ(0, rsync_push_message(handle_rsync, reinterpret_cast<const void*>(buffer1.data()), buffer1.size()));

    std::string buffer2{R"(test_id checksum_fail {"begin":"/boot/grub2/fonts/unicode.pf2","end":"/boot/grub2/fonts/unicode.pf2","id":1})"};
    ASSERT_EQ(0, rsync_push_message(handle_rsync, reinterpret_cast<const void*>(buffer2.data()), buffer2.size()));

    std::string buffer3{R"(test_id no_data {"begin":"/boot/grub2/fonts/unicode.pf2","end":"/boot/grub2/i386-pc/gzio.mod","id":1})"};
    ASSERT_EQ(0, rsync_push_message(handle_rsync, reinterpret_cast<const void*>(buffer3.data()), buffer3.size()));

    EXPECT_EQ(0, rsync_close(handle_rsync));
}

