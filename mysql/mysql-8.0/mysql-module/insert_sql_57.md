
do_command
|->get_command(&com_data, &command)
|   |->state="starting"
|->dispatch_command
|   |->mysql_parse
|   |  |->mysql_execute_command
|   |  |  |->Sql_cmd_update::execute
|   |  |  | |->try_single_table_update
|   |  |  |    |->update_precheck(thd, all_tables)
|   |  |  |    |  |->state="checking permissions" //主要做用户权限检查参考check_one_table_access
|   |  |  |    |->open_tables_for_query
|   |  |  |    |  |->open_tables //主要加MDL 打开对应的ibd文件和内存管理对象
|   |  |  |    |     |->state="open_tables"
|   |  |  |    |->mysql_update
|   |  |  |       |->state="init"
|   |  |  |       |->mysql_prepare_update //构造select 查询语句 通过update的where和item构造select语句
|   |  |  |       |->substitute_gc(thd, select_lex, conds, NULL, order) //where条件等效替换
|   |  |  |       |->lock_tables(thd, table_list, thd->lex->table_count, 0)
|   |  |  |       |  |->mysql_lock_tables
|   |  |  |       |     |->state="System lock" //innodb标识锁类型/MyISAM表级锁
|   |  |  |       |->table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK) //统计table的记录数
|   |  |  |       |->test_quick_select //生成对应的执行计划
|   |  |  |       |->init_read_record //根据执行计划确定访问记录的函数
|   |  |  |       |->state="updating"
|   |  |  |       |->while(1)
|   |  |  |       |  |->error= info.read_record(&info) //遍历where条件原记录 带锁
|   |  |  |       |  |->qep_tab.skip_record(thd, &skip_record) //过滤掉不满足where条件的记录 同时释放这条记录锁
|   |  |  |       |  |->store_record(table,record[1]) //保存记录
|   |  |  |       |  |->fill_record_n_invoke_before_triggers //用更新的值填充 新记录
|   |  |  |       |  |->ha_update_row(old_row,new_row)
|   |  |  |       |     |->ha_innobase::update_row
|   |  |  |       |     |  |->row_upd
|   |  |  |       |     |     |->//先更新clust_rec 然后依次更新sec_rec
|   |  |  |       |     |->binlog_log_row //记录row格式binlog
|   |  |  |       |->end_read_record(&info) //清空info结构
|   |  |  |       |->state="end"
|   |  |  |       |->//记录UPDATE_ROWS_EVENT 否则stmt格式记录sql语句binlog      
|   |  |  |->state="query end" //如果是事务执行commit 则state=="starting"
|   |  |  |->trans_commit_stmt //执行commit 默认autocommit
|   |  |  |  |->ha_commit_trans
|   |  |  |     |->tc_log->prepare(thd, all) //prepare阶段 innodb只flush redolog
|   |  |  |     |->tc_log->commit(thd, all)
|   |  |  |        |->MYSQL_BIN_LOG::commit
|   |  |  |           |->//记录xid的event
|   |  |  |           |->MYSQL_BIN_LOG::ordered_commit
|   |  |  |              |->//该函数就是经典的三阶段提交
|   |  |  |              |->//1.其中flush阶段会持有LOCK_log锁 该锁会阻塞show master/slave status
|   |  |  |              |->//2.半同步的等待也在该函数内以及binlog_group_commit_sync_delay 也在该阶段
|   |  |  |              |->//3.innodb的锁释放也在该阶段
|   |  |  |->state="closing tables"
|   |  |  |->close_thread_tables( thd) //将open_tables打开的ibd内存对象缓存起来 接着使用
|   |  |  |->thd->mdl_context.release_statement_locks() ////事务结束,释放MDL锁
|   |  |->state="freeing items"  
|   |  |->THD::cleanup_after_query
|   |     |->free_items //释放内存              
|   |->log_slow_statement(thd) //记录慢查询日志
|   |->state="cleaning up"
|   |->thd->reset_query()
|   |->thd->set_command(COM_SLEEP)//设置为sleep
|->//等待下一个命令


