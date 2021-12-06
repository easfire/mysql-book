b init_server_components
	b delegates_init // 做了一些授权
	 b new Trans_delegate
	 b new Binlog_storage_delegate
	 b new Server_state_delegate
	 b new Binlog_transmit_delegate
	 b new Binlog_relay_IO_delegate

	b 检查 binlog/relaylog 目录及文件

	b plugin_register_early_plugins
	 	b plugin_init_internals

	b plugin_register_builtin_and_init_core_se
		// 初始化 MyISAM, InnoDB and CSV
		b builtin_binlog_plugin
		b builtin_mysql_password_plugin
		b builtin_csv_plugin
		b builtin_heap_plugin
		b builtin_innobase_plugin
			// Innodb 参数初始化
			b innodb_buffer_pool_evict
			b innodb_buffer_pool_load_now

	b plugin_register_dynamic_and_init_all

	b 打开 slow log/general log

	b 打开 binlog

b 从 gtid_executed 表和binlog 初始化 GLOBAL.GTID_EXECUTED and GLOBAL.GTID_PURGED

b init_ssl

b network_init

b acl_init

b grant_init

b servers_init
	b servers_reload // 初始化 mysql.servers 表数据 (具体实现涉及锁、metadata等 TODO)
		b open_trans_system_tables_for_read
		b servers_load

b init_slave // 初始化 slave 结构 
	b create_slave_info_objects
	b start_slave_threads

b initialize_performance_schema_acl

b initialize_information_schema_acl

b execute_ddl_log_recovery

b start_signal_handler  // 启动单独的线程signal_hand来处理信号，如SIGTERM, SIGQUIT, SIGHUP, SIGUSR1 and SIGUSR2  

b start_processing_signals

b create_compress_gtid_table_thread

b start_processing_signals



