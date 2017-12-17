# Nginx反向代理多网站SSL证书

by **WangWei**

修改时间：**20171215**

**参考：**
- [[原创]使用Let's encrypt免费SSL证书](https://github.com/bg6cq/ITTS/blob/master/security/ssl/letsencrypt/README.md)
- [[原创]nginx反向代理服务器](https://github.com/bg6cq/ITTS/blob/master/app/nginx/README.md)

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
    ssl_certificate /etc/nginx/ssl/pass.xaufe.edu.cn.crt;
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

使用crontab每天执行一次更新证书。
````/etc/cron.daily/getssl.cron````文件：

````
/usr/src/getssl/getssl -d pass.xaufe.edu.cn
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
