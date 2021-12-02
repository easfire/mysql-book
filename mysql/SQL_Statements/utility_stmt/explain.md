https://segmentfault.com/a/1190000008131735
https://www.kancloud.cn/taobaomysql/monthly/81380

8.0新的执行器
http://mysql.taobao.org/monthly/2020/07/01/


drop table test;
create  table test (id  int ,name varchar(10),index idx_id (id));
insert  into  test  values (1,'name1'),(2,'name2'),(3,'name3'),(4,'name4'),(5,'name5'),(6,'name6'),(7,'name7');

SET OPTIMIZER_TRACE="enabled=on",END_MARKERS_IN_JSON=on;

select  * from  test  where id=1;


分析 sql_optimizer trace


create table `cpa_order` (
`cpa_order_id` bigint(20) unsigned not null,
`settle_date` date default null comment '',
`id` bigint(20) not null,
primary key (`cpa_order_id`),
unique key `id` (`id`),
key `cpao_settle_date_id` (`settle_date`, `id`)
)engine=innodb default charset=gbk;


SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE \G;

