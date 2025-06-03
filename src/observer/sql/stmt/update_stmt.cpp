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
// Created by Wangyunlai on 2022/5/22.
//

#include "sql/stmt/update_stmt.h"
#include "common/log/log.h"
#include "storage/db/db.h"
#include "storage/table/table.h"

UpdateStmt::UpdateStmt(Table *table, Value *values, int value_amount, int field_id, FilterStmt *filter_stmt_)
    : table_(table), values_(values), value_amount_(value_amount), field_id_(field_id), filter_stmt_(filter_stmt_)
{}

RC UpdateStmt::create(Db *db, const UpdateSqlNode &update, Stmt *&stmt)
{
  RC rc = RC::SUCCESS;
  // 检查表名是否存在
  const char *table_name = update.relation_name.c_str();
  Table *table = db->find_table(table_name);
  if (table == nullptr) {
    LOG_WARN("table %s doesn't exit", table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // 检查字段和类型
  // update t1 set name=909 where id>1;
  const TableMeta &table_meta = table->table_meta();
  const std::vector<FieldMeta>* fieldMeta = table_meta.field_metas();
  bool valid = false;
  FieldMeta update_field;
  rc = RC::SCHEMA_FIELD_NOT_EXIST;
  for ( FieldMeta field :*fieldMeta) {
    // 字段是否存在
    if( 0 == strcmp(field.name(),update.attribute_name.c_str()))
    {
      // 字段类型是否匹配
      if(field.type() == update.value.attr_type())
      {
        valid = true;
        update_field = field;
        break;
      }
      rc = RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
  }
  if (!valid){
    LOG_WARN("No field named %s with type %s", update.attribute_name.c_str(), update.value.attr_type());
    return rc;
  }

  std::unordered_map<std::string, Table *> table_map   = {{update.relation_name, table}};
  FilterStmt                              *filter_stmt = nullptr;
  rc = FilterStmt::create(db, table, &table_map, 
    update.conditions.data(), static_cast<int>(update.conditions.size()), filter_stmt);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("cannot construct filter stmt");
    return rc;
  }
  Value *value = new Value(update.value);
  stmt = new UpdateStmt(table, value, 1, update_field.field_id(), filter_stmt);
  return RC::SUCCESS;
}
