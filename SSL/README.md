# Nginx反向代理多网站SSL证书

by **WangWei**

创建时间：**20171215** 修改时间：**20180314**

**参考：**
- [[原创]使用Let's encrypt免费SSL证书](https://github.com/bg6cq/ITTS/blob/master/security/ssl/letsencrypt/README.md)
- [[原创]nginx反向代理服务器](https://github.com/bg6cq/ITTS/blob/master/app/nginx/README.md)
- 泛域名[[原创]使用Let's encrypt免费SSL证书](https://github.com/bg6cq/ITTS/blob/master/security/ssl/acme.sh/README.md)


***
20180314更新(Let's encrypt支持泛域名证书)

1. 获取脚本

从[https://github.com/Neilpang/acme.sh](https://github.com/Neilpang/acme.sh)下载脚本，复制到````/usr/src/acme.sh````

给文件夹下的acme.sh赋予执行权限

2. 验证服务器

````
cd /usr/src/acme.sh
./acme.sh --issue --dns  -d *.xaufe.edu.cn
````
执行后退出，提示有：
```
Add the following txt record:
Domain:'_acme-challenge.xaufe.edu.cn'
Txt value:'9ihDbjYfTExAYeDs4DBUeuTo18KBzwvTEjUnSwd32-c'

```

3. 修改DNS

DNS中添加：

```
_acme-challenge.xaufe.edu.cn. IN TXT "9ihDbjYfTExAYeDs4DBUeuTo18KBzwvTEjUnSwd32-c"
```

4. 获取证书

```
./acme.sh --renew  -d *.xaufe.edu.cn
```
默认生成的证书存放在```/root/.acme.sh/*.xaufe.edu.cn```

5. 生成nginx需要证书

```
acme.sh --install-cert -d *.xaufe.edu.cn 
--key-file /etc/nginx/ssl/xaufe.edu.cn.key 
--fullchain-file /etc/nginx/ssl/xaufe.edu.cn.pem
--reloadcmd "/bin/systemctl restart nginx.service"
```
nginx配置文件相应修改为：
```
ssl_certificate_key /etc/nginx/ssl/xaufe.edu.cn.key;
ssl_certificate /etc/nginx/ssl/xaufe.edu.cn.pem;
```

6. 增加计划任务，执行4、5步骤即可



***



**假设**

3个采用nginx反向代理的http网站：

- ````http://pass.xaufe.edu.cn````
- ````http://cas.xaufe.edu.cn````
- ````http://my.xaufe.edu.cn````

为以上网站配置https，源网站不做任何修改。

### 一、安装

首次安装时使用，后期增加不同域名时不需要。

````
mkdir /usr/src/getssl
cd /usr/src/getssl
curl --silent https://raw.githubusercontent.com/srvrco/getssl/master/getssl > getssl ; chmod 700 getssl 
````

### 二、生成配置

首次生成时使用，后期不需要执行
````
/usr/src/getssl/getssl -c pass.xaufe.edu.cn
````

### 三、修改配置

1. 修改````/root/.getssl/getssl.cfg````文件（首次使用）

````
CA="https://acme-v01.api.letsencrypt.org"
ACCOUNT_EMAIL="wangwei@xaufe.edu.cn"
````
2. 修改````/root/.getssl/pass.xaufe.edu.cn/getssl.cfg````文件

````
#只有一个域名时不需要SANS
#pass.xaufe.edu.cn域名不用写
SANS="cas.xaufe.edu.cn,my.xaufe.edu.cn"

#ACL和域名一一对应
#此处待验证，是否需要不同路径？
ACL=('/usr/share/nginx/html/pass/.well-known/acme-challenge'
     '/usr/share/nginx/html/cas/.well-known/acme-challenge'
     '/usr/share/nginx/html/my/.well-known/acme-challenge')

#证书保存路径
#Nginx的server段需要这个路径
DOMAIN_CERT_LOCATION="/etc/nginx/ssl/pass.xaufe.edu.cn.crt"
DOMAIN_KEY_LOCATION="/etc/nginx/ssl/pass.xaufe.edu.cn.key"
CA_CERT_LOCATION="/etc/nginx/ssl/chain.crt"
#合并的pem文件，否则有些浏览器验证不通过
DOMAIN_CHAIN_LOCATION="/etc/nginx/ssl/fullchain.pem"
DOMAIN_PEM_LOCATION="/etc/nginx/ssl/privkey.pem"


#更新证书后重启Nginx命令
RELOAD_CMD="/bin/systemctl restart nginx.service"
````

### 四、添加````.well-known````目录

根据````/root/.getssl/pass.xaufe.edu.cn/getssl.cfg````文件的````ACL````配置，为每个域名添加````.well-known````目录

- ````/usr/share/nginx/html/pass/.well-known/acme-challenge````
- ````/usr/share/nginx/html/cas/.well-known/acme-challenge````
- ````/usr/share/nginx/html/my/.well-known/acme-challenge````

### 五、修改Nginx配置

每个域名的server段添加以上路径的解析，````.well-known````不做反向代理，解析到每个域名的根路径：
````
location /.well-known {
      root	/usr/share/nginx/html/pass;
      #pass用于区分不同域名
      #其它域名如下修改
      #/usr/share/nginx/html/cas;
      #/usr/share/nginx/html/my;
}
````

### 六、获取证书

````
#/usr/src/getssl/getssl -d pass.xaufe.edu.cn
````

### 七、配置SSL

编辑每个域名的server段，如：````cas.xaufe.edu.cn````

````
server {
    listen 80;
    server_name cas.xaufe.edu.cn;
    #访问http自动跳转https
    rewrite     ^   https://$host$request_uri? permanent;
}

server {
    listen 443 ssl;
    ssl on;
    server_name	cas.xaufe.edu.cn;

    #证书路径，多域名使用同一证书
    ssl_certificate /etc/nginx/ssl/fullchain.pem;
    ssl_certificate_key /etc/nginx/ssl/pass.xaufe.edu.cn.key;
    ssl_session_timeout 5m;
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
    
    location /.well-known {
        root	/usr/share/nginx/html/cas; 
    }

    location / {
        proxy_pass http://192.168.123.111:8080;
        proxy_redirect	http://192.168.123.111:8080 http://$host:$server_port;	
    }

}

````
### 八、测试证书

采用[https://www.ssllabs.com/ssltest/analyze.html?d=cas.xaufe.edu.cn](https://www.ssllabs.com/ssltest/analyze.html?d=cas.xaufe.edu.cn)验证证书是否有效


### 九、证书更新

使用crontab每天3点执行一次更新证书。
````/etc/cron.d/getssl.cron````文件：

````
MAILTO=root

0  3	* * *	root	/usr/src/getssl/getssl -d pass.xaufe.edu.cn
````



### 十、增加域名

以增加````ecard````域名为例：
1. 备份并删除````/root/.getssl/pass.xaufe.edu.cn/getssl.cfg````文件
2. 执行````/usr/src/getssl/getssl -c pass.xaufe.edu.cn````
3. 修改备份的````getssl.cfg````文件，增加SAN及ACL
````
SANS="cas.xaufe.edu.cn,my.xaufe.edu.cn,ecard.xaufe.edu.cn"
ACL=('/usr/share/nginx/html/pass/.well-known/acme-challenge'
	 '/usr/share/nginx/html/cas/.well-known/acme-challenge'
	 '/usr/share/nginx/html/my/.well-known/acme-challenge'
	 '/usr/share/nginx/html/ecard/.well-known/acme-challenge')
	 
````
4. 覆盖````/root/.getssl/pass.xaufe.edu.cn/getssl.cfg````文件

5. 配置ecard的server段

````
location /.well-known {
	root	/usr/share/nginx/html/ecard; 
}
````
6. 删除````/root/.getssl/pass.xaufe.edu.cn/pass.xaufe.edu.cn.crt````文件 

7. 执行
````
/usr/src/getssl/getssl -d pass.xaufe.edu.cn
````
	 
8. 验证证书是否生效

采用[https://www.ssllabs.com/ssltest/analyze.html?d=ecard.xaufe.edu.cn](https://www.ssllabs.com/ssltest/analyze.html?d=ecard.xaufe.edu.cn)验证证书

9. 修改eacrd 的server段，添加ssl配置

````
server {
listen 80;
server_name ecard.xaufe.edu.cn;
rewrite     ^   https://$host$request_uri? permanent;
}

server {
listen 443 ssl;
ssl on;
    server_name	ecard.xaufe.edu.cn;
    ......
}
````
### 十一、 Tomcat获取参数错误问题

Tomcat使用http，通过Nginx反向代理实现ssl时，会出现参数获取不正确的问题。

1. Tomcat的````server.xml````文件中添加如下代码

````
<!---->
<Valve className="org.apache.catalina.valves.RemoteIpValve"
	internalProxies=".*"
	remoteIpHeader ="x-forwarded-for"
   	proxiesHeader="x-forwarded-by"
   	protocolHeader="x-forwarded-proto"
	trustedProxies=".*"
       />
````

其中````remoteIpHeader````是重要参数，网上很多文章都没写这个，不配置不会生效。````".*"````通配任何地址，也可设置为已知代理ip

2. Nginx的````location````段添加

````
proxy_set_header  Host $host;  
proxy_set_header  X-Real-IP  $remote_addr;  
proxy_set_header  X-Forwarded-For $proxy_add_x_forwarded_for;  
proxy_set_header  X-Forwarded-Proto  $scheme; 
````
