/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2023/4/25.
//

#include "sql/stmt/drop_index_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "storage/db/db.h"
#include "storage/table/table.h"

using namespace std;
using namespace common;

RC DropIndexStmt::create(Db *db, const DropIndexSqlNode &drop_index, Stmt *&stmt)
{
  stmt = nullptr;

  const char *table_name = drop_index.relation_name.c_str();
  if (is_blank(table_name) || is_blank(drop_index.index_name.c_str())) {
    LOG_WARN("invalid argument. db=%p, table_name=%p, index name=%s",
        db, table_name, drop_index.index_name.c_str());
    return RC::INVALID_ARGUMENT;
  }

  // check whether the table exists
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // check whether the index exists
  Index *index = table->find_index(drop_index.index_name.c_str());
  if (nullptr == index) {
    LOG_WARN("no such index. table_name=%s, index_name", table_name, drop_index.index_name.c_str());
    return RC::SCHEMA_INDEX_NOT_EXIST;
  }

  stmt = new DropIndexStmt(table, drop_index.index_name);
  return RC::SUCCESS;
}
