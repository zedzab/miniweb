#include <mysql/mysql.h>
#include <stdio.h>

int main(){
	MYSQL *mysql=mysql_init(NULL);//初始化
	if(mysql==NULL){
		printf("mysql_init() error\n");
        	return -1;
    	}
	mysql=mysql_real_connect(mysql,"localhost","root","19980215","scott",0,NULL,0);//连接数据库
	if(mysql==NULL){
		printf("mysql_real_connect() error\n");
        	return -1;
    	}
	printf("mysql api使用的默认编码: %s\n", mysql_character_set_name(mysql));
	mysql_set_character_set(mysql, "utf8");
    	printf("mysql api使用的修改之后的编码: %s\n", mysql_character_set_name(mysql));
    	printf("恭喜, 连接数据库服务器成功了...\n");

	const char* sql="select * from dept";
	int ret=mysql_query(mysql,sql);//执行这个sql语句
	//if(ret==0){//注意，返回值ret=0表示查询成功！
		//printf("mysql_query() a失败了, 原因: %s\n", mysql_error(mysql));
        	//return -1;
    	//}

	MYSQL_RES *res=mysql_store_result(mysql);//取出结果集
	if(res == NULL)
    	{
        	printf("mysql_store_result() 失败了, 原因: %s\n", mysql_error(mysql));
        	return -1;
    	}

	int num=mysql_num_fields(res);//得到结果集中的列数
	
	MYSQL_FIELD * fields=mysql_fetch_fields(res);//得到所有列的名字，并且输出
	for(int i=0; i<num; ++i)
    	{
		printf("%s\t\t", fields[i].name);
	}
    	printf("\n");
	
	MYSQL_ROW row;// 遍历结果集中所有的行
	while((row=mysql_fetch_row(res))!=NULL){
		for(int i=0;i<num;i++){//将当前行每一列信息列出
			printf("%s\t\t", row[i]);
        	}
        	printf("\n");
    	}

	mysql_free_result(res);//释放资源

    	// 9. 写数据库
   	// 以下三条是一个完整的操作, 对应的是一个事务
    	// 设置事务为手动提交
    	mysql_autocommit(mysql, 0); 
    	int ret1 = mysql_query(mysql, "insert into dept values(61, '海军', '圣地玛丽乔亚')");//查询语句
    	int ret2 = mysql_query(mysql, "insert into dept values(62, '七武海', '世界各地')");
    	int ret3 = mysql_query(mysql, "insert into dept values(63, '四皇', '新世界')");
    	printf("ret1 = %d, ret2 = %d, ret3 = %d\n", ret1, ret2, ret3);

	if(ret1==0 && ret2==0 && ret3==0)
    	{
        	// 提交事务
       	mysql_commit(mysql);
    	}
    	else
    	{
        mysql_rollback(mysql);
    	}

    // 释放数据库资源
    	mysql_close(mysql);
    
  	return 0;
}	
