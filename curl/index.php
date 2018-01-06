<?php
	function getip() { 
		
		$unknown = 'unknown'; 
		
		if ( isset($_SERVER['HTTP_X_FORWARDED_FOR']) && $_SERVER['HTTP_X_FORWARDED_FOR'] && strcasecmp($_SERVER['HTTP_X_FORWARDED_FOR'], $unknown) ) { 
			$ip = $_SERVER['HTTP_X_FORWARDED_FOR']; 
		} elseif ( isset($_SERVER['REMOTE_ADDR']) && $_SERVER['REMOTE_ADDR'] && strcasecmp($_SERVER['REMOTE_ADDR'], $unknown) ) { 
			$ip = $_SERVER['REMOTE_ADDR']; 
		} 
		/* 处理多层代理的情况 或者使用正则方式：$ip = preg_match("/[\d\.]{7,15}/", $ip, $matches) ? $matches[0] : $unknown; */
		
		if (false !== strpos($ip, ',')) 
			$ip = reset(explode(',', $ip)); 
			return $ip; 
	} 
?>

<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">
<HTML>
	<HEAD>
		<META HTTP-EQUIV=Content-Type content="text/html; charset=UTF-8">
		<META HTTP-EQUIV="Refresh" CONTENT="300">
		<META HTTP-EQUIV="Cache-Control" content="no-cache">
		<META HTTP-EQUIV="Pragma" CONTENT="no-cache">
		<TITLE>Cernet IP地址列表</TITLE>
	</HEAD>
	<BODY BGCOLOR="#ffffff">
	<FONT FACE="arial,sans-serif" size="3" color="#000000">
	<CENTER><H2>Cernet IP地址列表</H2></CENTER>
	<H3><?php echo "您的IP地址信息:".getip();?></H3>
	<h4>西安财经学院教育网地址：218.195.32.0/21，219.244.32.0/21</h4>
	<HR WIDTH='98%'>

<?php
	$myUrl="http://www.nic.edu.cn/member-cgi/ShowMyFIL";
	$myAdd=getAddress($myUrl);
	//按行分隔字符串
	$wip=explode("\n",$myAdd);
	//地址数量
	$ipCount=count($wip);	
	//重写掩码格式地址
	$eip="";
	
	for ($x=0; $x<=$ipCount; $x++) {
		$tempIP=explode(" ",$wip[$x]);
		$eip=$eip.$tempIP[0]." ".$tempIP[2]."\n";
	}
	
	$eip=trim($eip)."\n";
	
	echo "<pre style=\"word-wrap: break-word; white-space: pre-wrap;\">\n";
	echo "获取来源：".$myUrl;
	echo "\n主表（不含附表），共计：".$ipCount."条\n";
	echo "<HR WIDTH='98%'>";
	echo "-----------------------------------------------\n";
	echo "掩码格式：\n";
	echo "-----------------------------------------------\n";
	echo $eip;
	echo "\n\n\n";
	echo "-----------------------------------------------\n";
	echo "聚合格式：\n";
	echo "-----------------------------------------------\n";
	
	$eip=getCIDR($eip);
	
	echo $eip;
	echo "\n\n\n";
	echo "-----------------------------------------------\n";
	echo "排列（Bind View用）：\n";
	echo "-----------------------------------------------\n";
	$eip=str_ireplace("\n",";\n",$eip);
	echo $eip;
	echo "</pre>";

	//主表+附表地址
	$myUrl="http://www.nic.edu.cn/RS/ipstat/internalip/real.html";
	$myAdd=getAddress($myUrl);
	//按行分隔字符串
	$wip=explode("\n",$myAdd);
	//地址数量
	$ipCount=count($wip);	
	//重写掩码格式地址
	$eip="";

	for ($x=0; $x<=$ipCount; $x++) {
		$tempIP=explode(" ",$wip[$x]);
		$eip=$eip.$tempIP[0]." ".$tempIP[2]."\n";
	}

	$eip=trim($eip)."\n";

	echo "<HR WIDTH='98%'>";
	echo "<pre style=\"word-wrap: break-word; white-space: pre-wrap;\">\n";
	echo "获取来源：".$myUrl;
	echo "\n主表+所有附表，共计：".$ipCount."条\n";
	echo "<HR WIDTH='98%'>";
	echo "-----------------------------------------------\n";
	echo "掩码格式：\n";
	echo "-----------------------------------------------\n";
	echo $eip;
	echo "\n\n\n";
	echo "-----------------------------------------------\n";
	echo "聚合格式：\n";
	echo "-----------------------------------------------\n";
	$eip=getCIDR($eip);
	echo $eip;
	echo "\n\n\n";
	echo "-----------------------------------------------\n";
	echo "排列（Bind View用）：\n";
	echo "-----------------------------------------------\n";
	$eip=str_ireplace("\n",";\n",$eip);
	echo $eip;
	echo "</pre>";

	function getCIDR($str) {
		$cstr=str_ireplace(" 255.255.255.255","/32",$str);
		$cstr=str_ireplace(" 255.255.255.254","/31",$cstr);
		$cstr=str_ireplace(" 255.255.255.252","/30",$cstr);
		$cstr=str_ireplace(" 255.255.255.248","/29",$cstr);
		$cstr=str_ireplace(" 255.255.255.240","/28",$cstr);
		$cstr=str_ireplace(" 255.255.255.224","/27",$cstr);
		$cstr=str_ireplace(" 255.255.255.192","/26",$cstr);
		$cstr=str_ireplace(" 255.255.255.128","/25",$cstr);
		$cstr=str_ireplace(" 255.255.255.0","/24",$cstr);
		$cstr=str_ireplace(" 255.255.254.0","/23",$cstr);
		$cstr=str_ireplace(" 255.255.252.0","/22",$cstr);
		$cstr=str_ireplace(" 255.255.248.0","/21",$cstr);
		$cstr=str_ireplace(" 255.255.240.0","/20",$cstr);
		$cstr=str_ireplace(" 255.255.224.0","/19",$cstr);
		$cstr=str_ireplace(" 255.255.192.0","/18",$cstr);
		$cstr=str_ireplace(" 255.255.128.0","/17",$cstr);
		$cstr=str_ireplace(" 255.255.0.0","/16",$cstr);
		$cstr=str_ireplace(" 255.254.0.0","/15",$cstr);
		$cstr=str_ireplace(" 255.252.0.0","/14",$cstr);
		$cstr=str_ireplace(" 255.248.0.0","/13",$cstr);
		$cstr=str_ireplace(" 255.240.0.0","/12",$cstr);
		$cstr=str_ireplace(" 255.224.0.0","/11",$cstr);
		$cstr=str_ireplace(" 255.192.0.0","/10",$cstr);
		$cstr=str_ireplace(" 255.128.0.0","/9",$cstr);
		$cstr=str_ireplace(" 255.0.0.0","/8",$cstr);
		$cstr=str_ireplace(" 254.0.0.0","/7",$cstr);
		$cstr=str_ireplace(" 252.0.0.0","/6",$cstr);
		$cstr=str_ireplace(" 248.0.0.0","/5",$cstr);
		$cstr=str_ireplace(" 240.0.0.0","/4",$cstr);
		$cstr=str_ireplace(" 224.0.0.0","/3",$cstr);
		$cstr=str_ireplace(" 192.0.0.0","/2",$cstr);
		$cstr=str_ireplace(" 128.0.0.0","/1",$cstr);
		$cstr=str_ireplace(" 0.0.0.0","/0",$cstr);
		return $cstr;
	}
	
	function getAddress($url) {
	// 创建一个新cURL资源
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
	$start_str=stripos($data,"<pre>");
	$end_str=stripos($data,"</pre>");
	$data = substr($data, $start_str, $end_str-$start_str-1);
	$data = trim(substr($data, strripos($data,"-")+1));
	//多个空格保留一个
	$data = preg_replace ( "/\s(?=\s)/","\\1", $data);

	return $data;
}
?>	
	<BR /><HR WIDTH='98%'>

	</FONT>
</BODY>
</HTML>
