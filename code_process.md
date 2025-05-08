# 启动server
./build_debug/bin/observer -f ./etc/observer.ini -P cli
# miniob解析流程分析
1. **事件会话流程**
- **PATH**: `/root/miniob/src/observer/net/sql_task_handler.cpp`
  - `handle_evnet`监听捕获来自上层的操作数据 -> `read_event`解析数据为`SessionEvent`类型的对象 -> `session_stage_`会话处理，设置为当前会话？ -> 创建新`SQLStageEvent`对象，`handle_sql`进行sql解析操作: `query_cache_stage_`查询缓存 -> `parse_stage_`语法树解释 -> `resolve_stage_`解析为stmt语句 -> `optimize_stage_`优化执行计划 -> `execute_stage_`执行阶段
  - *create\drop主要关注stmt语法解析和executor执行阶段*

2. **stmt语法解析**
- **PATH**: `/root/miniob/src/observer/sql/stmt/stmt.cpp`
  - `resolve_stage_.handle_request` -> `Stmt::create_stmt`选择需要创建的stmt语句类型，`CreateTableStmt::create`创建语句

3. **executor执行**
- **PATH**: `/root/miniob/src/observer/sql/executor/execute_stage.cpp`
  - 对象`SQLStageEvent`中包含了各个阶段的解析结果，包括`session_event_`注册的事件、`sql_`需处理的sql语句、`sql_node_`解析后的sql命令、`stmt_`进一步处理的sql数据结构、`operator_`执行计划
  - 进入`CommandExecutor.execute`函数，判断执行sql指令类型。
  - 假设进入`CREATE_TABLE`分支，`CreateTableExecutor.execute`执行对应建表指令
  - `CreateTableExecutor.execute`获取事件信息，由事件的数据库Db对象执行建表语句`session->get_current_db()->create_table`
  - 最终执行函数位于`/root/miniob/src/observer/storage/db/db.cpp`

4. **create_table样例分析**
- **PATH**: `/root/miniob/src/observer/storage/db/db.cpp`
  - **create_table**
    1. 创建一个`Table`对象，调用`Table.create`方法
    2. 对传入参数进行验证，并检验表的唯一性
    3. `Table.table_meta_`属性用于记录表的名称、属性各类信息，使用`table_meta_.init`初始化创建表的信息
    4. 创建`fstream`对象创建表元数据文件，由`table_meta_.serialize`方法将表元数据.table写入物理磁盘
    5. 之后由数据库的`BufferPoolManager`将表数据文件.data写入物理磁盘，并使用对应引擎打开表，存储到缓冲区

# drop实现
- `handle_sql`进行sql解析操作: `query_cache_stage_`查询缓存 -> `parse_stage_`语法树解释 -> `resolve_stage_`解析为stmt语句 -> `optimize_stage_`优化执行计划 -> `execute_stage_`执行阶段
1. **stmt解析**
  - 首先在`resolve_stage_`中解析drop table的stmt语句: `resolve_stage_.handle_request` -> `Stmt::create_stmt`
  - 添加`SCF_DROP_INDEX`与`SCF_DROP_TABLE`分支，基于`parse_stage_`语法树解析结果`DropIndexSqlNode`与`DropTableSqlNode`创建`DropIndexStmt`与`DropTableStmt`类。
  - `RC`中添加`SCHEMA_INDEX_NOT_EXIST`参数表示index索引不存在
  - `DropIndexStmt`包含表`table`对象和`index_name_`索引
  - `DropTableStmt`只含有`table_name_`属性
2. **excecute执行指令**
  - 由`execute_stage_.handle_request`创建对象，执行`CommandExecutor.execute`操作解析drop table命令
  - 添加`DROP_INDEX`与`DROP_TABLE`分支，同时新建`DropIndexExecutor`与`DropTableExecutor`类
  
  - **DROPINDEX**
  - `DropIndexExecutor.execute`获取对应事件管理器`Trx`和表`Table`，由`Table->drop_index`表对象执行索引删除
  - `Table::drop_index`接收表名称调用数据库引擎`engine_->drop_index`执行索引删除
  - 在数据库引擎基类`TableEngine`与当前两种引擎`HeapTableEngine`和`LsmTableEngine`中添加`drop_index`函数。`LsmTableEngine`引擎暂未实现，抛出`UNIMPLEMENT`异常
  - 删除索引文件，先关闭后删除`BplusTreeIndex::close` - `BplusTreeHandler::close` - `BufferPoolManager::close_file` -> `BplusTreeIndex::drop` - `BufferPoolManager::drop_file`
  - 删除表元数据中对应索引，在`TableMeta::drop_index`中根据索引名称找到索引并删除
  - 创建一份表元数据的临时文件写入修改后重命名覆盖原磁盘中的元数据文件，同时使用`TableMeta::swap`覆写内存中的元数据

  - **DROPTABLE**
  - `DropTableExecutor.execute`将`table_name_`属性传递给事件的数据库Db对象`session->get_current_db()->drop_table`，由数据库执行表删除
  - `Db::drop_table`接收表名称，检查表是否存在，并获取表路径、名称、id等信息。创建调用`table->drop`方法，结束后在`opened_tables_`中删除对应表名
  - `Table::drop`: 1.检查表是否存在 2.`remove(path)`删除表元数据文件 3.`BufferPoolManager::drop_file`删除表数据文件