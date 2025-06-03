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
// Created by WangYunlai on 2022/6/27.
//

#include "sql/operator/update_physical_operator.h"
#include "common/log/log.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"

RC UpdatePhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::SUCCESS;
  }

  unique_ptr<PhysicalOperator> &child = children_[0];

  RC rc = child->open(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  trx_ = trx;

  // 获取到需要的记录，并制作更新后的副本
  const int field_start_index = table_->table_meta().sys_field_num();
  std::vector<Record> old_records;
  std::vector<vector<Value>> new_values;
  while (OB_SUCC(rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current record: %s", strrc(rc));
      return rc;
    }

    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    Record   &record    = row_tuple->record();

    // 然后制作更新的记录副本
    std::vector<Value> new_value;
    for (int i = field_start_index; i < table_->table_meta().field_num(); i++) {
      const FieldMeta *field = table_->table_meta().field(i);
      if (field->field_id() == field_id_) {
        new_value.emplace_back(*value_);
      }
      else {
        Value nvalue;
        row_tuple->cell_at(i, nvalue);
        new_value.emplace_back(nvalue);
      }
    }
    new_values.emplace_back(new_value);
    old_records.emplace_back(std::move(record));
  }
  child->close();

  // 删除对应记录并创建新的记录
  const int value_num = static_cast<int>(table_->table_meta().field_num() - field_start_index);
  ASSERT(old_records.size() == new_values.size(), "update size not fit");
  // std::vector<Record> new_records(old_records.size());
  for (int i = 0; i < int(old_records.size()); i++) {
    rc = trx_->delete_record(table_, old_records[i]);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to delete record: %s", strrc(rc));
      return rc;
    }

    Record new_record;
    rc = table_->make_record(value_num, new_values[i].data(), new_record);
    // new_records.emplace_back(new_record);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to make record: %s", strrc(rc));
      return rc;
    }

    rc = trx->insert_record(table_, new_record);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to insert record: %s", strrc(rc));
      return rc;
    }
  }

  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::next()
{
  return RC::RECORD_EOF;
}

RC UpdatePhysicalOperator::close()
{
  return RC::SUCCESS;
}
