<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>wifi设置页面</title>
  <link rel="stylesheet" href="stylewireless.css">
    <script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script language="JavaScript" type="text/javascript">
function autoRefresh()
{
	
	setTimeout("doRefresh()", 10 * 1000 );
	
}

function doRefresh()
{
	document.forms[0].submit();
}

function disconnect()
{
	document.forms[0].wan_proto.value="disabled";
	document.forms[0].submit();
}

function connect()
{
	document.forms[0].wan_proto.value="dhcp";
	document.forms[0].submit();
}
function setDns() {
	var dns=new Array();
	var webdns="";
        dns[0]="<% nvram_list("wan0_dns", 0); %>";
	dns[1]="<% nvram_list("wan0_dns", 1); %>";
 	dns[2]="<% nvram_list("wan0_dns", 2); %>";
	if(dns[0].length!=0) {
	 webdns=webdns.concat(dns[0]);
	}
	if(dns[1].length!=0) {
	 webdns=webdns.concat(",").concat(dns[1]);
	}
	if(dns[2].length!=0) {
	 webdns=webdns.concat(",").concat(dns[2]);
	}
	$("#wl_dns").html(webdns);
}

function modifyssid() {
	var strs=new Array();
        var wl0_ssid,temp_ssid="";
	var wl0_ssid_header = "<% nvram_get("wl0_ssidheader"); %>";
        wl0_ssid = "<% nvram_get("wl0_ssid"); %>";
        var wl0_ssidheder_len = wl0_ssid_header.length;
	var wl0_tailer = wl0_ssid.substr(wl0_ssidheder_len);
	strs=wl0_tailer.split(":");
        for(i=0;i<strs.length && strs;i++)
	{
		temp_ssid=temp_ssid.concat(strs[i]);
	}
        wl0_ssid=wl0_ssid_header.concat(temp_ssid);
	$("#wl_ssid").html(wl0_ssid);
}
function loadstat() { 
        var value = "<% nvram_get("wl_radio"); %>";
	if(value=="1") {
        document.getElementById('wl_wireless_enable_td').innerHTML = "启用"; 
	} else {
	document.getElementById('wl_wireless_enable_td').innerHTML = "关闭";
	}
	setDns();
	modifyssid();
        autoRefresh();
}

$(document).ready(function(){
	$("input[name='action']").click(function(){
		var actionpair = [
			['断开','Apply'],
			['连接','Apply']
		];
		var curaction = $(this).val();
		var newaction;
		for(i = 0; i < actionpair.length; i++)
		{
			if (curaction == actionpair[i][0]) {
				newaction = actionpair[i][1];
				$(this).val(newaction);
			}
		}
	});
  });

//-->
</script>
</head>
<body onLoad="loadstat();">
<form method="post" action="netstatus.asp">
<input type="hidden" name="wan_proto" value="">
  <div class="main">
    <div class="aside">
      <ul>
        <li><a href ="basicset.asp">基本设置</a></li>
        <li><a href ="seniorset.asp">高级设置</a></li>
        <li><a href ="wireless.asp">无线扩展</a></li>
        <li class="check"><a href ="netstatus.asp">网络状态</a></li>
      </ul>
    </div>
    <div class="cont">
      <div class="cont-net">
        <table cellpadding="0" cellspacing="0" style="width:80%">
          <tr>
            <th colspan="2">无线状态</th>
          </tr>
          <tr>
            <td>无线功能</td>
            <td id="wl_wireless_enable_td">关闭 </td>
          </tr>
          <tr>
            <td>运行模式</td>
            <td id="wl_running_mode_td"><% nvram_get("wl_mode"); %></td>
          </tr>
          <tr>
            <td>无线模式</td>
            <td id="wl_wireless_mode_td"><% wl_cur_phytype(); %> </td>
          </tr>
          <tr>
            <td>无线名称</td>
            <td><label id="wl_ssid"></label></td>
          </tr>
          <tr>
            <td>无线信道</td>
            <td>自动（<% nvram_get("wl_channel"); %>）</td>
          </tr>
          <tr>
            <td>MAC地址</td>
            <td><% nvram_get("wl0_hwaddr"); %></td>
          </tr>
          <tr>
            <th colspan="2">WAN状态</th>
          </tr>
          <tr>
            <td>连接状态</td>
            <td><% nvram_get("wan0_proto"); %></td>
          </tr>
          <tr>
            <td>MAC地址</td>
            <td><% nvram_get("wan0_hwaddr"); %></td>
          </tr>
          <tr>
            <td>IP地址</td>
            <td><% nvram_get("wan0_ipaddr"); %></td>
          </tr>
          <tr>
            <td>子网掩码</td>
            <td><% nvram_get("wan0_netmask"); %></td>
          </tr>
          <tr>
            <td>网关</td>
            <td><% nvram_get("wan0_gateway"); %></td>
          </tr>
          <tr>
            <td>DNS服务器</td>
            <td><a href="" ><label id="wl_dns"></label></td>
          </tr>
          <tr>
            <th colspan="2">LAN状态</th>
          </tr>
          <tr>
            <td>MAC地址</td>
            <td><% nvram_get("lan_hwaddr"); %></td>
          </tr>
          <tr>
            <td>IP地址</td>
            <td><% nvram_get("lan_ipaddr"); %></td>
          </tr>
          <tr>
            <td>子网掩码</td>
            <td><% nvram_get("lan_netmask"); %></td>
          </tr>
          <tr>
            <td>网关</td>
            <td><% nvram_get("lan_gateway"); %></td>
          </tr>
        </table>
      </div>
    </div>
  </div>
</form>
</body>
</html>
