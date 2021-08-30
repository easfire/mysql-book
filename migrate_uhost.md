###  Uhost资源迁移

主机创建 预检查o/os 资源是否充足？
	理论上 IGetPCAvailableCapacity 接口返回的 count 可以估算出指定配置剩余可创建的uhost
	但资源系统计算不准，存在count>0 但不能创建情况。废弃[暂不采用，待底层检查完毕]

    升级内存资源不足，迁移接口
   	MigrateUHostInstance 接口，调度系统不是那么可信，也不能获取资源充足的set。[暂不采用，待底层检查完毕]


老机房，O/OS 不在一个POD；因为OS机型是后来上线的，都在新的POD
然而，不同POD下存储不能迁移

但新机房，O/OS 基本都在一个POD；可以存储迁移


运维平台迁移Uhost
1、查找同POD下，哪些set还有资源（这需要SRE手动查找,依赖资源调度系统）(如果不需要切换set，理论上resize已经尝试做了迁移)
2、如果需要跨O/OS，那么不同类型主机一定在不同set，需要勾选跨set (先resize漂盘都在同set进行)
3、禁止跨POD迁移，会影响集群存储

MapDBTypeToID
MapChargetype
MapDBTypeToDBVersion
MapDBTypeIDToDBType

invisible_uhost_udb-swfm1rnc

curl -H "Content-type: application/json" -X POST -d @- --trace-ascii - "http://internal.api.ucloud.cn" << EOF
{
    "Action": "MigrateUHostInstance",
	"Backend":"UHost-Migrate",
    "LiveMigrate": true,
    "MigrateWay": "live",
    "UHostId": "4a754c07-7b44-473c-a5f3-5a306caba287",
    "zone_id": 3002,
    "request_uuid": "cd7f6381-0ea9-467a-a3df-a126cccd"
}
EOF

curl -H "Content-type: application/json" -X POST -d @- --trace-ascii - "http://internal.api.ucloud.cn" << EOF
{
	"Backend": "UHost-Migrate",
	"Action": "QueryMigrationTask",
	"ZoneId": 3002,
	"ByTaskID": {
		"TaskID": "26c0605461a819cc6e59ea3e549b53e0ca9cf4c2"
	}
}
EOF


INIT
ALLOCATE_NIC_VF
STOP_TIMEMACHINE
MIGRATE_DOMAIN
MIGRATE_SUCCESS


{
    "Id": "invisible_uhost_udb-oe13lh3s",
    "ResourceId": "4a754c07-7b44-473c-a5f3-5a306caba287",
    "Status": 1,
    "ResourceType": 276,
    "OrganizationId": 50400561,
    "TopOrganizationId": 6025,
    "RegionId": 1000004,
    "ZoneId": 3002,
    "BusinessId": "",
    "Created": 1614915730,
    "Updated": 1614915732,
    "Deleted": 0,
    "VPCId": "uvnet-acediwkx",
    "SubnetId": "subnet-402mbs0o"
}

{"RetCode":0,"Action":"MigrateUHostInstanceResponse","Message":"OK","TaskID":"3c13590ecfc0bfd3bfd7526d41858197ad39f8ed"}



udbha-s3vu3wtw

invisible_uhost_udb-oe13lh3s



"Name":"1860d58e-df12-4134-bc2a-5a6aaa79c848"

curl -H "Content-type: application/json" -X POST -d @- --trace-ascii - "http://internal.api.ucloud.cn" << EOF
{
    "Backend": "UHost",
    "Action": "DescribeUHostInstance",
    "az_group": 1000004,
    "Region": "hk",
    "organization_id": 50400561,
    "top_organization_id": 6025,
    "UHostIds": [
        "invisible_uhost_udb-oe13lh3s"
    ],
    "NoEIP": false,
    "request_uuid": "471fd884-d19b-431e-9caa-4dc37f619544"
}
EOF



curl -H "Content-type: application/json" -X POST -d @- --trace-ascii - "http://internal.api.ucloud.cn" << EOF
{
	"Backend": "UHost-Migrate",
	"Action": "QueryMigrationTask",
	"ZoneId": 5001,
	"ByTaskID": {
		"TaskID": "3c13590ecfc0bfd3bfd7526d41858197ad39f8ed"
	}
}
EOF



{"RetCode":0,"Action":"MigrateUHostInstanceResponse","Message":"OK","TaskID":"34e5f739ecd11061f6085f8154a4d7df5f0c8d66"}

{"RetCode":0,"Action":"MigrateUHostInstanceResponse","Message":"OK","TaskID":"c2d7b9089e86f53fd1c99a03b9015ced86b9adf8"}== Info: Connection #0 to host internal.api.ucloud.cn left intact                                     

"UHostType":"OS"
"SetId":32
"PodId":"7001_25GE_D_R002"


"HotplugFeature": false, [TODO]

那么 我只能通过 换主机类型(O/OS)来 重试 创建Uhost 

curl -H "Content-type: application/json" -X POST -d @- --trace-ascii - "http://internal.api.ucloud.cn" << EOF
{
	"Backend":"UHost",
	"Action":"IGetPCAvailableCapacity",
	"request_uuid":"cf219227-0a45-4731-a32b-35980c5456f7",
	"Zone":"hk-02",
	"Region":"hk",
	"az_group":1000004,
	"zone_id":3002,
    "CPU": 4,
    "Memory": 4096,
    "Disks": [
        {
            "IsBoot": "True",
            "Size": 20,
            "Type": "CLOUD_RSSD"
        },
        {
            "IsBoot": "False",
            "Size": 20,
            "Type": "CLOUD_RSSD"
        }
    ],
    "MachineType": "O",
    "MinimalCpuPlatform": "Intel/Auto",
	"NetCapability": "Ultra",
	"HotplugFeature": false,
    "organization_id": 55757413,
    "top_organization_id": 56033886
}
EOF

