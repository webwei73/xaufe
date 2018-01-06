# 各语言获取URL内容实例

wangwei#xaufe.xaufe.edu.cn

**编辑时间:20180106**

#### 一、Asp
````
......
  dim http 
  set http=Server.createobject("Msxml2.ServerXMLHTTP") 
  Http.open "GET",url,false 
  Http.send() 
  if Http.readystate<>4 then
  	exit function 
  end if 
  getHTTPPage=bytesToBSTR(Http.responseBody,"GB2312")

  set http=nothing
......
````
实例：

#### 二、PHP

````
  ......
	$ch = curl_init();
	// 设置URL和相应的选项
	curl_setopt($ch, CURLOPT_URL, $url);
	// 忽略ssl证书
	curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, FALSE);
	//不返回请求头信息
	curl_setopt($ch, CURLOPT_HEADER, false);
	//不直接显示结果，用于存储变量
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
	// 变量赋值
	$data = curl_exec($ch);
	//关闭cURL资源，并且释放系统资源
	curl_close($ch);
  ......
````
注：若不设置````curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);````参数，将直接输出curl的结果。
实例：

#### 三、Linux

1. 测试URL连通性，如：

````
#curl http://www.xaufe.edu.cn/
````
2. URL保存至文件，如：

````
#curl http://www.xaufe.edu.cn/ > /usr/src/index.html
````
