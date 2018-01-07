## 钉钉自定义机器人

by WangWei 

**修改时间：20180107**

参考：[钉钉文档中心](https://open-doc.dingtalk.com/docs/doc.htm?spm=a219a.7629140.0.0.karFPe&treeId=257&articleId=105735&docType=1)

一、参考文档

1. 发送文本
````
{
    "msgtype": "text", 
    "text": {
        "content": "文本内容"
    }, 
    "at": {
        "atMobiles": [
            "需要@的手机号1", 
            "手机号码2"
        ], 
        "isAtAll": false
    }
}
````
2. 发送链接
````
{
    "msgtype": "link", 
    "link": {
        "text": "文本内容", 
        "title": "标题", 
        "picUrl": "图片链接", 
        "messageUrl": "URL链接"
    }
}
````
3. markdown格式
````
{
     "msgtype": "markdown",
     "markdown": {
         "title":"杭州天气",
         "text": "#### 杭州天气\n" +
                 "> 9度，西北风1级，空气良89，相对温度73%\n\n" +
                 "> ![screenshot](快照链接)\n"  +
                 "> ###### 10点20分发布 [天气](http://www.thinkpage.cn/) \n"
     },
    "at": {
        "atMobiles": [
            "156xxxx8827", 
            "189xxxx8325"
        ], 
        "isAtAll": false
    }
 }
````

二、测试

1. 原理：POST到指定URL即可

2. 实例

Linux下新建一个脚本，测试发送内容到指定群，手动、定时、触发执行都可以

````
#!/bin/bash

WEBHOOK_LOCATION="https://oapi.dingtalk.com/robot/send?access_token=指定群的token"
CUSTOM_HEADER="Content-Type: application/json"
JSON_DATA='{"msgtype": "text", 
            "text": {
            "content": "发送文本内容"
            },
            "at": {
        "atMobiles": [
            "13572858399"
        ], 
        "isAtAll": false
    }}'
curl "$WEBHOOK_LOCATION" -H "$CUSTOM_HEADER" -d "$JSON_DATA"
#end of the file
````
