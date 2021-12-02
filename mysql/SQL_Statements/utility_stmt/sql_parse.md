入口函数:

JOIN::optimize()
b sql/sql_optimizer.cc:262

1\ bool optimize_cond(THD *thd, Item **cond, COND_EQUAL **cond_equal,
                   mem_root_deque<TABLE_LIST *> *join_list,
                   Item::cond_result *cond_value)
b sql/sql_optimizer.cc:9967

优化大概分下面几个步骤 

1	condition_processing
2	substitute_generated_columns
3	table_dependencies
4	ref_optimizer_key_uses
5	rows_estimation
6	considered_execution_plans
7	attaching_conditions_to_tables
8	finalizing_table_conditions
9	refine_plan
