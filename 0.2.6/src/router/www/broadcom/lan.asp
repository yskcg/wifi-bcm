<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="zh-Hans">
<head>
<title>Gateway design: LAN</title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script language="JavaScript" type="text/javascript">
<!--
function inet_aton(a)
{
	var n;

	n = a.split(/\./);
	if (n.length != 4)
		return 0;

	return ((n[0] << 24) | (n[1] << 16) | (n[2] << 8) | n[3]);
}
	
function inet_ntoa(n)
{
	var a;

	a = (n >> 24) & 255;
	a += "."
	a += (n >> 16) & 255;
	a += "."
	a += (n >> 8) & 255;
	a += "."
	a += n & 255;

	return a;
}

function lan_ipaddr_change()
{
	var lan_netaddr, lan_netmask, dhcp_start, dhcp_end, lan_gateway;
	
	lan_netaddr = inet_aton(document.forms[0].lan_ipaddr.value);
	lan_netmask = inet_aton(document.forms[0].lan_netmask.value);
	lan_netaddr &= lan_netmask;

	dhcp_start = inet_aton(document.forms[0].dhcp_start.value);
	dhcp_start &= ~lan_netmask;
	dhcp_start |= lan_netaddr;
	dhcp_end = inet_aton(document.forms[0].dhcp_end.value);
	dhcp_end &= ~lan_netmask;
	dhcp_end |= lan_netaddr;

    lan_gateway = lan_netaddr | 0x01;

	document.forms[0].dhcp_start.value = inet_ntoa(dhcp_start);
	document.forms[0].dhcp_end.value = inet_ntoa(dhcp_end);
	document.forms[0].lan_gateway.value = inet_ntoa(lan_gateway);
	
}

function lan_dhcp_change(index)
{
	var dhcp = document.forms[0].lan_dhcp[document.forms[0].lan_dhcp.selectedIndex].value;
	
	if (index == "0"){
		if (document.forms[0].lan_dhcp.disabled == 1 || dhcp == "0") {
			document.forms[0].lan_gateway.disabled = 0;
			document.forms[0].lan_netmask.disabled = 0;
			document.forms[0].lan_ipaddr.disabled = 0;
		}
		else {
			document.forms[0].lan_gateway.disabled = 1;
			document.forms[0].lan_netmask.disabled = 1;
			document.forms[0].lan_ipaddr.disabled = 1;
		}
	}
}

function lan_dhcp_server_change(index)
{
	if (index == 0){
		document.forms[0].dhcp_start.disabled = 0;
		document.forms[0].dhcp_end.disabled = 0;
		document.forms[0].lan_lease.disabled = 0;
	}
}

/*
*/
function lan_update()
{
	document.forms[0].lan_dhcp.disabled = 1;
	document.forms[0].dhcp_start.disabled = 0;
	document.forms[0].dhcp_end.disabled = 0;
	document.forms[0].lan_lease.disabled = 0;
	document.forms[0].lan_lease.readOnly = true;
    document.forms[0].lan_ifname.value = "<% nvram_get("lan_ifname"); %>";
}

$(document).ready(function(){
	$("input[name='action']").click(function(){
		var actionpair = [
			['应用','Apply']
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

/*
*/ 
/*
*/
//-->
</script>
</head>

<body onLoad="lan_update();">
<div id="overDiv" style="position:absolute; visibility:hidden; z-index:1000;"></div>

<br />
<table border="0" cellpadding="0" cellspacing="0" width="100%">
  <tr>
    <td colspan="2" align="left" ><a href="http://www.96335.com"><img border="0" src="gxlogo.gif" alt=""></a></td>
  </tr>
</table>

<br />
<nav>
    <ul>
        <li><a href="index.asp">基本信息</a></li>
        <li><a class="active" href="lan.asp">LAN配置</a></li>
        <li><a href="wan.asp">WAN配置</a></li>
        <li><a data-toggle="#list" href="#">无线配置</a>
        <ul class="out" id="list">
            <li><a href="radio.asp">射频配置</a></li>
            <li><a href="ssid.asp">SSID配置</a></li>
            <li><a href="security.asp">安全配置</a></li>
            <li><a href="pwps.asp">WPS</a></li>
            <li><a href="wds.asp">WDS</a></li>
        </ul>
        </li>
        <li><a href="firmware.asp">固件升级</a></li>
    </ul>
</nav>

<form method="post" action="apply.cgi">
<input type="hidden" name="page" value="lan.asp">
<!-- These are set by the Javascript functions above --> 
<input type="hidden" name="num_lan_ifaces" value="1">
<input type="hidden" name="lan_ifname" value="" >
<input type="hidden" name="lan_proto" value="dhcp" >

<div class="table-conf">
<div class="table-conf-title">
设备管理网络设置
</div>
<table border="0" cellpadding="0" cellspacing="0">
  <tr>
    <th width="310">网络配置描述&nbsp;&nbsp;</th>
    <td>默认Lan网络</td>
  </tr>
  <tr>
    <th width="310">MAC地址&nbsp;&nbsp;</th>
    <td><% nvram_get("lan_hwaddr"); %></td>
  </tr> 
  <tr>
    <th width="310">地址分配方式&nbsp;&nbsp;</th>
    <td>
	<select name="lan_dhcp" onChange="lan_dhcp_change(0);">
	  <option value="1" >DHCP</option>
	  <option value="0" selected >静态指定</option>
	</select>
    </td>
  </tr>  
  <tr>
    <th width="310">IP地址&nbsp;&nbsp;</th>
    <td><input name="lan_ipaddr" value="<% nvram_get("lan_ipaddr"); %>" size="15" maxlength="15" onChange="lan_ipaddr_change();"></td>
  </tr>
  <tr>
    <th width="310">子网掩码&nbsp;&nbsp;</th>
    <td><input name="lan_netmask" value="<% nvram_get("lan_netmask"); %>" size="15" maxlength="15" onChange="lan_ipaddr_change();"></td>
  </tr>
  <tr>
    <th width="310">默认网关&nbsp;&nbsp;</th>
    <td><input name="lan_gateway" value="<% nvram_get("lan_gateway"); %>" size="15" maxlength="15"></td>
  </tr>
</table>

<br />
<br />
<div class="table-conf-title">
DHCP服务参数配置
</div>
<table border="0" cellpadding="0" cellspacing="0">
  <tr>
    <th width="310">DHCP地址池起始地址&nbsp;&nbsp;</th>
    <td><input name="dhcp_start" value="<% nvram_get("dhcp_start"); %>" size="15" maxlength="15"></td>
  </tr>
  <tr>
    <th width="310">DHCP地址池结束地址&nbsp;&nbsp;</th>
    <td><input name="dhcp_end" value="<% nvram_get("dhcp_end"); %>" size="15" maxlength="15"></td>
  </tr>
  <tr>
    <th width="310">DHCP租约时间&nbsp;&nbsp;</th>
    <td><input name="lan_lease" value="<% nvram_get("lan_lease"); %>" size="6" maxlength="6"></td>
  </tr>
</table>

<br />
<br />
<div class="table-conf-title">
DHCP已分配地址信息
</div>
<table>
    <tr>
        <td class="label" style="width:auto">主机名</td>
        <td class="label" style="width:auto">MAC地址</td>
        <td class="label" style="width:auto">IP地址</td>
        <td class="label" style="width:auto">剩余租约时间</td>
        <td class="label" style="width:auto">网络描述</td>
    </tr>
    <% lan_leases(); %>
</table>
</div>

<div class="table-apply">
<br />
<table>
  <tr>
    <td>
	<input type="submit" name="action" value="应用">
	<input type="reset" name="action" value="放弃">
    </td>
  </tr>
</table>
</div>
</form>

<!--
<br />
<br />
<p class="footer">&#169;2016-2017 LanNet Corporation. All rights reserved.</p>
-->

</body>
</html>
