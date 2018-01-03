# CentOS DHCP服务器配置

by **WangWei**

创建时间：**20180103**

**说明：**

1. 显示DHCP状态参考：[[原创] Linux下ISC dhcpd分配状态显示](https://github.com/bg6cq/ITTS/blob/master/app/dhcp/dhcpd-pool/README.md)
2. dhcpd-pools采用汉化版：[https://github.com/bg6cq/dhcpd-pools-cn](https://github.com/bg6cq/dhcpd-pools-cn)
3. 省略CentOS安装过程，从Webmin配置的注意事项开始



一、安装perl模块（webmin1.85版本后需要ssl）

````
#yum -y install openssl perl perl-Net-SSLeay perl-IO-Tty perl-Crypt-SSLeay
````
二、防火墙添加Webmin的端口（如：使用8675端口）
````
#firewall-cmd --zone=public --add-port=8675/tcp --permanent   
````
三、安装Webmin，启用ssl，端口设置为8675

四、防火墙添加DHCP规则

五、关闭SELinux

修改````/etc/selinux/config```` 文件，
将````SELINUX=enforcing````改为````SELINUX=disabled````，保存后重启机器即可

六、添加NPT服务器，同步系统时间

七、Webmin添加DHCP模块，或
````
#/bin/yum -y install dhcp
````
八、修改配置文件````etc/dhcp/dhcpd.conf````

假设DHCP服务器IP：10.0.0.100
````
#拒绝不正确的IP地址的要求
authoritative;
#动态DNS方式
#必须写，否则无法启动
ddns-update-style interim;
#忽略客户端更新
ignore client-updates;
# 缺省租约时间24h
default-lease-time 86400;
# 最大租约时间24h
max-lease-time 86400;
# 与格林威治时间偏差
option time-offset -18000;
# DNS
option domain-name-servers 114.114.114.114, 8.8.8.8;
#自定义日志级别，为了单独保存dhcp日志文件
#默认日志文件：/var/log/messages
log-facility local4;

# DHCP Server Gateway
# 必须添加DHCP服务器所在网段，否则无法启动
subnet 10.0.0.64 netmask 255.255.255.192 {
	option routers 10.0.0.126;

	}

#子网描述，出现在webmin的Description列
#Vlan101
subnet 10.245.5.0 netmask 255.255.255.192 { 
	option routers 10.245.5.62;
	range dynamic-bootp 10.245.5.1 10.245.5.61;
	}
#Vlan102
subnet 10.245.5.64 netmask 255.255.255.192 { 
	option routers 10.245.5.126;
	range dynamic-bootp 10.245.5.65 10.245.5.125;
	}
#Vlan103
subnet 10.245.5.128 netmask 255.255.255.192 { 
	option routers 10.245.5.190;
	range dynamic-bootp 10.245.5.129 10.245.5.189;
	}
#Vlan104
subnet 10.245.5.192 netmask 255.255.255.192 { 
	option routers 10.245.5.254;
	range dynamic-bootp 10.245.5.193 10.245.5.253;
	}
	
······

````

九、修改日志保存路径

1. 建立````/var/log/dhcp/dhcp.log````文件，权限600
2. 修改````/etc/rsyslog.conf````文件

指定local4日志文件，添加
````
local4.*             /var/log/dhcp/dhcp.log
````

在messages中屏蔽local4日志，修改
````
*.info;mail.none;authpriv.none;cron.none;                /var/log/messages
````
为
````
*.info;mail.none;authpriv.none;cron.none;local4.none                /var/log/messages
````
3. 重启rsyslog和DHCP
4. 添加日志回滚

webmin的Log File Rotation中添加对```` /var/log/dhcp/*.log````日志的回滚操作（每天压缩，保存7天）


十、安装dhcpd-pools，监控dhcp状态（汉化版安装方式相同，源码链接在说明中）

1. 下载dhcpd-pools-3.0.tar.xz，[https://sourceforge.net/projects/dhcpd-pools/files/](https://sourceforge.net/projects/dhcpd-pools/files/)
2. 解压至/usr/src
3. 下载需要的uthash.h
````
#curl https://raw.githubusercontent.com/troydhanson/uthash/master/src/uthash.h > /usr/include/uthash.h
````
4. 参数配置

````
#cd /usr/src/dhcpd-pools-3.0
#./configure --with-dhcpd-leases=/var/lib/dhcpd/dhcpd.leases
#make（需安装必要包:automake，gcc等）
````
5. 测试运行：
````
#./dhcpd-pools
````
6. 添加定时任务，每分钟重写一次网页
````
* * * * * /usr/src/dhcpd-pools-3.0/dhcpd-pools -A -f H > /usr/libexec/webmin/dhcp.html
````
**说明** 

- -A：dhcpd-pools默认只显示shared-network名字，-A是把subnet作为shared-network显示（CIDR格式）

- -f H：输出完整html文本

- ````/usr/libexec/webmin/````
是miniserv定义的webmin根目录，新建一个空的````dhcp.html````文件，赋予写权限。

7. 通过````https://WebminIP:8675/dhcp.html````访问即可。
