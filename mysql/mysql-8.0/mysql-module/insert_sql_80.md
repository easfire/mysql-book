


主干流程在 阿里内核月刊 insert 流程解析里面，已经介绍的很详细，但应该是5.7版本的
http://mysql.taobao.org/monthly/2017/09/10/

这里对8.0版本的 insert statement Debug链路梳理了一遍。(建议用sublime打开，代码按列号收敛做得很智能.)

INSERT:[8.0.26-debug]

b handle_connection

      while (thd_connection_alive(thd)) {
        if (do_command(thd)) break;
      }

	b do_command
		b Protocol_classic::parse_packet
			case COM_QUERY: // sql/protocol_classic.cc:2928
		b dispatch_command
			b dispatch_sql_command
				b parse_sql		// Transform an SQL statement into an AST
				b general_log_write 	// Rewrite the query for logging and for the Performance Schema statement tables.
					/* Actually execute the query */
				b set_trg_event_type_for_tables		// LEX* lex;  // parse tree descriptor
					b sql_command = SQLCOM_INSERT
				b mysql_execute_command	[2730:4738]
					b Sql_cmd_dml::execute [3450]
						b Sql_cmd_dml::prepare()
							b Sql_cmd_insert_base::precheck()
							 b check_one_table_access
							  b check_single_table_access // Check grants for commands which work only with one table.
							  b check_table_access
							  	b check_access // Compare requested privileges with the privileges acquired from the User- and Db-tables.

							b open_tables_for_query // Open all tables for a query or statement, in list started by "tables"
							 b open_tables
							 b open_secondary_engine_tables

							b prepare_inner	//   Prepare items in INSERT statement
							 // Statement plan is available within these braces
							 b check_insert_fields // Check insert fields

						b lock_tables // Locking of tables is done after preparation but before optimization.

						b Sql_cmd_insert_values::execute_inner() // [472:791] Insert one or more rows from a VALUES list into a table
						     b // Statement plan is available within these braces
				 			 b prepare_triggers_for_insert_stmt
				 			  b mark_columns_needed_for_insert
				 			 b write_record [1794:2208] // 
				 			  b init_sql_alloc // create MEM_ROOT memory
				 			  b ha_write_row // 
				 				 b mark_trx_read_write
				 				   
				 				   <InnoDB insert START>
				 				 b ha_innobase::write_row() [8710:8908] // Stores a row in an InnoDB database, to the table specified in this handle.
				 				    b row_insert_for_mysql // Execute insert graph that will result in actual insert.
				 				     b row_insert_for_mysql_using_ins_graph // 
				 				      b trx_start_if_not_started_xa_low // 
				 				      	b trx_start_low // Starts a transaction
				 				      	 b trx_sys_rw_trx_add // 
				 				      b row_get_prebuilt_insert_row
				 				      b row_mysql_convert_row_to_innobase //  Convert a row in the MySQL format to a row in the Innobase format
				 				      b row_ins_step //  Inserts a row to a table, This is a high-level function used in SQL execution graphs
				 				       b lock_table // LOCK_IX  we should set an IX lock on table
				 				       b row_ins // Inserts a row to a table
				 				        while (node->index != nullptr) // while insert index
				 					        b row_ins_alloc_row_id_step // Allocates a row id for row and inits the node->index field
				 					         b row_ins_index_entry_set_vals // Sets the values of the dtuple fields in entry from the values of appropriate columns in row.
				 					         b row_ins_index_entry // Inserts an index entry to index. Tries first optimistic, then pessimistic descent down the tree. If the entry matches enough to a delete marked record, performs the insert by updating or delete unmarking the delete marked record.
				 					          b row_ins_clust_index_entry //  Inserts an entry into a clustered index. 
				 					           b row_ins_clust_index_entry_low // Tries to insert an entry into a clustered index, ignoring foreign key constraints. If a record with the same unique key is found, the other record is necessarily marked deleted by a committed transaction, or a unique key violation error occurs. The delete marked record is then updated to an existing record, and we must write an undo log record on the delete marked record.
				 					           	b btr_pcur_open
				 					           	b row_ins_duplicate_error_in_clust_online // Checks for a duplicate when the table is being rebuilt online.
				 					           	b btr_cur_optimistic_insert //  Tries to perform an insert to a page in an index tree, next to cursor.
				 					           	 b rec_get_converted_size // caculate the size of a data tuple when converted to a physical record.
				 					           	 /* Now, try the insert */ 2800
				 					           	 b btr_cur_ins_lock_and_undo // For an insert, checks the locks and does the undo logging if desired.
				 					           	 b page_cur_tuple_insert // 
				 					           	  b rec_convert_dtuple_to_rec // Builds a physical record out of a data tuple and stores it beginning from the start of the given buffer.
				 					           	  b page_cur_insert_rec_low // Inserts a record next to page cursor on an uncompressed page.
				 					           	 b btr_search_update_hash_on_insert // 
				 					            b mtr.commit() 
				 					        b node->index = node->index->next()
				 				    b innobase_srv_conc_exit_innodb // 
				 				   <InnoDB insert END>

			 		 			 b binlog_log_row

			 		 	     // Now all rows are inserted finally!!!!  Time to update logs and sends response to user.

			 		 	     b binlog_query	// After the all calls to ha_*_row() functions; After any writes to system tables
			 		 	      b binlog_flush_pending_rows_event 
			 		 	       b flush_and_set_pending_rows_event

			 		b (if) trans_rollback_stmt
			 		b trans_commit_stmt
			 		  b ha_commit_trans [1592-1838] 
				 		  b acquire_lock // Acquire one lock with waiting for conflicting locks to go away if needed
				 		  b tc_log->prepare(thd, all) 
				 		  	b binlog_prepare
				 		  	b innobase_xa_prepare
				 		  // prepare phase So that we can flush prepared records of transactions to the log of storage engine in a group right before flushing them to binary log during binlog group commit flush stage.

				 		  b tc_log->commit(thd, all)
				 		  	b MYSQL_BIN_LOG::commit()
				 		  		b binlog_cache_data::finalize() // write events to cache
				 		  		 b flush_pending_event
				 		  		 b write_event
				 		  		 b compress
				 		    	b MYSQL_BIN_LOG::ordered_commit() [8702-8978] // 
				 		    		 # stage1: flushing transactions to binary log
					 		    	 b MYSQL_BIN_LOG::process_flush_stage_queue() // flush cache
					 		    	  b mysql_mutex_assert_owner(&LOCK_log); // get LOCK_log mutex 
					 		    	  b assign_automatic_gtids_to_flush_group // generate GTID !!! 
					 		    	  b flush_thread_caches // flush cache for session.
					 		    	   b binlog_cache_mngr::flush // null
					 		    	   b binlog_cache_data::flush //  Write the Gtid_log_event to the binary log and prior to writing the statement or transaction cache.
					 		    	 b MYSQL_BIN_LOG::flush_cache_to_file() // Flush the binary log to the binlog file

					 		    	 # stage2: Syncing binary log file to disk
					 		    	 b MYSQL_BIN_LOG::sync_binlog_file
					 		    	  b m_binlog_file->sync()
					 		    	   b IO_CACHE_ostream::sync()
					 		    	    b inline_mysql_file_sync()
					 		    	     b my_sync
					 		    	      b fdatasync

					 		    	 # stage3: Commit all transactions in order
					 		    	 b process_commit_stage_queue // Commit a sequence of sessions
					 		    	 	b ha_commit_low // storage engine commit

					 		    	 b finish_commit 
					      b inline_mysql_commit_transaction()
					      b thd->mdl_context.release_lock // release MDL commit lock
					      b trn_ctx->cleanup() // Free resources and perform other cleanup

				    
				    b lex->cleanup()
				    	b Query_expression::cleanup // Cleanup this query expression object

				    b close_thread_tables()
				    	b close_open_tables
				    	 b close_thread_table 
				    	 	b release_or_close_table
				    	 	 b Table_cache::release_table // Put used TABLE instance back to the table cache and mark it as unused
				    	 	 	b el->used_tables.remove(table);
				    	 	 	b el->free_tables.push_front(table);
				    	 	 	b link_unused_table(table);
				    	 	 	b free_unused_tables_if_necessary(thd);

				    b release_transactional_locks
				    b binlog_gtid_end_transaction // finalize GTID life-cycle


				b thd->lex->destroy()
				b thd->end_statement()
				b thd->cleanup_after_query()  // Cleanup and free items
					b cleanup_items(item_list());
					b free_items();

			b DBUG_PRINT("info", ("query ready"));

  			b /* Finalize server status flags after executing a command. */
				  thd->update_slow_query_status();
				  if (thd->killed) thd->send_kill_message();
				  thd->send_statement_status();

			CLIENT:
			mysql> insert into db.t (id,c) values (20006,20006);                                                       
			Query OK, 1 row affected (9 hours 36 min 28.50 sec)

			b   /* After sending response, switch to clone protocol */
				  if (clone_cmd != nullptr) {
				    assert(command == COM_CLONE);
				    error = clone_cmd->execute_server(thd);
				  }	

			b log_slow_statement 
			 b log_slow_do // write slow log

			b thd->set_command(COM_SLEEP)
			b thd->lex->sql_command = SQLCOM_END

			b thd->mem_root->ClearForReuse(); 
			b thd->mem_root->Clear(); (if  allocated a lot of memory, free it)



