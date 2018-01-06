
<%
	function getHTTPPage(url) 
		
		dim http 
		set http=Server.createobject("Msxml2.ServerXMLHTTP") 
		Http.open "GET",url,false 
		Http.send() 
		if Http.readystate<>4 then
			exit function 
		end if 
		'getHTTPPage=bytes2BSTR(Http.responseBody) 
		getHTTPPage=bytesToBSTR(Http.responseBody,"GB2312")
getHTTPPage=mid(getHTTPPage,instr(getHTTPPage,"<pre>"),instr(getHTTPPage,"</pre>")-instr(getHTTPPage,"<pre>")-8)
		set http=nothing
		
	end function 
	
	Function BytesToBstr(body,Cset)
dim objstream
set objstream = Server.CreateObject("adodb.stream")
objstream.Type = 1
objstream.Mode =3
objstream.Open
objstream.Write body
objstream.Position = 0
objstream.Type = 2
objstream.Charset = Cset
BytesToBstr = objstream.ReadText 
objstream.Close
set objstream = nothing
End Function

	Function bytes2BSTR(vIn) 
		dim strReturn 
		dim i1,ThisCharCode,NextCharCode 
		strReturn = "" 
		For i1 = 1 To LenB(vIn) 
			ThisCharCode = AscB(MidB(vIn,i1,1)) 
			If ThisCharCode < &H80 Then 
				strReturn = strReturn & Chr(ThisCharCode) 
			Else 
				NextCharCode = AscB(MidB(vIn,i1+1,1)) 
				strReturn = strReturn & Chr(CLng(ThisCharCode) * &H100 + CInt(NextCharCode)) 
				i1 = i1 + 1 
			End If 
		Next 
		bytes2BSTR = strReturn 
	End Function 

function getip()
on error resume next 
dim ip,start,lenit

ip=gethttppage(url)
start=instrrev(ip,"-")+1
lenit=len(ip)
ip=right(ip,lenit-start)
while (instr(ip,"  ") >= 1)
  ip= Replace(ip,"  "," ")
wend
dim wip,wtip,i,eip



wip=split(ip,chr(10))

eip=""
ccc=ubound(wip)+1
for i=0 to ubound(wip)
wtip=split(wip(i)," ")
eip=eip&wtip(0)&" "&wtip(2)&chr(10)
next



	
if err.number<>0 then 
	application("eip")="没有获得气象信息"
	application("iperror")="yes"
	err.Clear
else
	getip=eip
end if 
end function

function cDec(num)
cDecstr=0
if len(num)>0 and isnumeric(num) then
for inum=0 to len(num)-1
cDecstr=cDecstr+2^inum*cint(mid(num,len(num)-inum,1))
next
end if
cDec=cDecstr
end function

dim ccc,url
url="http://www.nic.edu.cn/member-cgi/ShowMyFIL"

ip=getip()
response.write "<pre>"
response.write vbcrlf
response.write "获取来源：http://www.nic.edu.cn/member-cgi/ShowMyFIL"
response.write vbcrlf
response.write "主表（不含附表），共计："&ccc&"条"
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
response.write vbcrlf
response.write "掩码的："
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
response.write ip
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
response.write "转换为聚合的:"
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
ip=replace(ip," 255.255.255.255","/32")
ip=replace(ip," 255.255.255.254","/31")
ip=replace(ip," 255.255.255.252","/30")
ip=replace(ip," 255.255.255.248","/29")
ip=replace(ip," 255.255.255.240","/28")
ip=replace(ip," 255.255.255.224","/27")
ip=replace(ip," 255.255.255.192","/26")
ip=replace(ip," 255.255.255.128","/25")
ip=replace(ip," 255.255.255.0","/24")
ip=replace(ip," 255.255.254.0","/23")
ip=replace(ip," 255.255.252.0","/22")
ip=replace(ip," 255.255.248.0","/21")
ip=replace(ip," 255.255.240.0","/20")
ip=replace(ip," 255.255.224.0","/19")
ip=replace(ip," 255.255.192.0","/18")
ip=replace(ip," 255.255.128.0","/17")
ip=replace(ip," 255.255.0.0","/16")
ip=replace(ip," 255.254.0.0","/15")
ip=replace(ip," 255.252.0.0","/14")
ip=replace(ip," 255.248.0.0","/13")
ip=replace(ip," 255.240.0.0","/12")
ip=replace(ip," 255.224.0.0","/11")
ip=replace(ip," 255.192.0.0","/10")
ip=replace(ip," 255.128.0.0","/9")
ip=replace(ip," 255.0.0.0","/8")
ip=replace(ip," 254.0.0.0","/7")
ip=replace(ip," 252.0.0.0","/6")
ip=replace(ip," 248.0.0.0","/5")
ip=replace(ip," 240.0.0.0","/4")
ip=replace(ip," 224.0.0.0","/3")
ip=replace(ip," 192.0.0.0","/2")
ip=replace(ip," 128.0.0.0","/1")
ip=replace(ip," 0.0.0.0","/0")
response.write ip
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
response.write "排列（Bind View用）："
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
ip=replace(ip," ","")
ip=replace(ip,chr(10),";"&chr(10))

response.write ip
response.write vbcrlf
response.write vbcrlf
response.write "</pre>"




url="http://www.nic.edu.cn/RS/ipstat/internalip/real.html"
ip=getip()
response.write vbcrlf
response.write "<pre>"
response.write vbcrlf
response.write "获取来源：http://www.nic.edu.cn/RS/ipstat/internalip/real.html"
response.write vbcrlf
response.write "主表+所有附表，共计："&ccc&"条"
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
response.write vbcrlf
response.write "掩码的："
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
response.write ip
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
response.write "转换为聚合的:"
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
ip=replace(ip," 255.255.255.255","/32")
ip=replace(ip," 255.255.255.254","/31")
ip=replace(ip," 255.255.255.252","/30")
ip=replace(ip," 255.255.255.248","/29")
ip=replace(ip," 255.255.255.240","/28")
ip=replace(ip," 255.255.255.224","/27")
ip=replace(ip," 255.255.255.192","/26")
ip=replace(ip," 255.255.255.128","/25")
ip=replace(ip," 255.255.255.0","/24")
ip=replace(ip," 255.255.254.0","/23")
ip=replace(ip," 255.255.252.0","/22")
ip=replace(ip," 255.255.248.0","/21")
ip=replace(ip," 255.255.240.0","/20")
ip=replace(ip," 255.255.224.0","/19")
ip=replace(ip," 255.255.192.0","/18")
ip=replace(ip," 255.255.128.0","/17")
ip=replace(ip," 255.255.0.0","/16")
ip=replace(ip," 255.254.0.0","/15")
ip=replace(ip," 255.252.0.0","/14")
ip=replace(ip," 255.248.0.0","/13")
ip=replace(ip," 255.240.0.0","/12")
ip=replace(ip," 255.224.0.0","/11")
ip=replace(ip," 255.192.0.0","/10")
ip=replace(ip," 255.128.0.0","/9")
ip=replace(ip," 255.0.0.0","/8")
ip=replace(ip," 254.0.0.0","/7")
ip=replace(ip," 252.0.0.0","/6")
ip=replace(ip," 248.0.0.0","/5")
ip=replace(ip," 240.0.0.0","/4")
ip=replace(ip," 224.0.0.0","/3")
ip=replace(ip," 192.0.0.0","/2")
ip=replace(ip," 128.0.0.0","/1")
ip=replace(ip," 0.0.0.0","/0")
response.write ip
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
response.write "排列（Bind View用）："
response.write vbcrlf
response.write "--------------------------------------"
response.write vbcrlf
ip=replace(ip," ","")
ip=replace(ip,chr(10),";"&chr(10))

response.write ip
response.write vbcrlf
response.write vbcrlf
response.write "</pre>"
%>
