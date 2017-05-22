<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="zh-Hans">
<head>
<title>Gateway design: WAN</title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script language="JavaScript" type="text/javascript">
function wan_pppoe_demand_change()
{
	var wan_pppoe_demand = document.forms[0].wan_pppoe_demand[document.forms[0].wan_pppoe_demand.selectedIndex].value;

	if (wan_pppoe_demand == "1")
		document.forms[0].wan_pppoe_idletime.disabled = 0;
	else
		document.forms[0].wan_pppoe_idletime.disabled = 1;
}
/*
*/
/*
*/
function wan_proto_change()
{
	var wan_proto = document.forms[0].wan_proto[document.forms[0].wan_proto.selectedIndex].value;

	if (wan_proto == "pppoe") {
        document.getElementById("pppoesettings").style.display = "table";

		document.forms[0].wan_pppoe_username.disabled = 0;
		document.forms[0].wan_pppoe_passwd.disabled = 0;
		document.forms[0].wan_pppoe_service.disabled = 0;
		document.forms[0].wan_pppoe_ac.disabled = 0;
		document.forms[0].wan_pppoe_keepalive.disabled = 0;
		document.forms[0].wan_pppoe_demand.disabled = 0;
		document.forms[0].wan_pppoe_idletime.disabled = 0;
		document.forms[0].wan_pppoe_mru.disabled = 0;
		document.forms[0].wan_pppoe_mtu.disabled = 0;

		wan_pppoe_demand_change();
	} else {
        document.getElementById("pppoesettings").style.display = "none";

		document.forms[0].wan_pppoe_username.disabled = 1;
		document.forms[0].wan_pppoe_passwd.disabled = 1;
		document.forms[0].wan_pppoe_service.disabled = 1;
		document.forms[0].wan_pppoe_ac.disabled = 1;
		document.forms[0].wan_pppoe_keepalive.disabled = 1;
		document.forms[0].wan_pppoe_demand.disabled = 1;
		document.forms[0].wan_pppoe_idletime.disabled = 1;
		document.forms[0].wan_pppoe_mru.disabled = 1;
		document.forms[0].wan_pppoe_mtu.disabled = 1;
	}
/*
*/
/*
*/
	if (wan_proto != "static" && wan_proto != "disabled") {
		document.forms[0].wan_ipaddr.disabled = 1;
		//document.forms[0].wan_domain.disabled = 1;
		document.forms[0].wan_netmask.disabled = 1;
		document.forms[0].wan_gateway.disabled = 1;
		document.forms[0].wan_dns0.disabled = 1;
		document.forms[0].wan_dns1.disabled = 1;
		document.forms[0].wan_dns2.disabled = 1;
		document.forms[0].wan_wins0.disabled = 1;
		document.forms[0].wan_wins1.disabled = 1;
		document.forms[0].wan_wins2.disabled = 1;
	} else {
		document.forms[0].wan_ipaddr.disabled = 0;
		//document.forms[1].wan_domain.disabled = 0;
		document.forms[0].wan_netmask.disabled = 0;
		document.forms[0].wan_gateway.disabled = 0;
		document.forms[0].wan_dns0.disabled = 0;
		document.forms[0].wan_dns1.disabled = 0;
		document.forms[0].wan_dns2.disabled = 0;
		document.forms[0].wan_wins0.disabled = 0;
		document.forms[0].wan_wins1.disabled = 0;
		document.forms[0].wan_wins2.disabled = 0;
	}
    
	var connection_status = [
		['Ｎ/A','未知'],
		['Unknown','未知'],
		['Disconnected','断开'],
		['Connecting','连接中'],
		['Connected','已连接']
	];

    var wan_link_status = "<% wan_link(); %>";
    for(i = 0; i < 5; i++)
	{
		if(connection_status[i][0] == wan_link_status)
			$("#wan_link_status").text(connection_status[i][1]);
	}
}

$(document).ready(function(){
	$("input[name='action']").click(function(){
		var actionpair = [
			['应用','Apply'],
			['释放IP地址','Release'],
			['重新请求IP地址','Renew']
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

</script>
</head>

<body onLoad="wan_proto_change();">
<div id="overDiv" style="position:absolute; visibility:hidden; z-index:1000;"></div>

<br />
<table border="0" cellpadding="0" cellspacing="0" width="100%">
  <tr>
    <td colspan="2" align="left" ><img border="0" src="gxlogo.gif" alt=""></td>
  </tr>
</table>

<br />
<nav>
    <ul>
        <li><a href="index.asp">基本信息</a></li>
        <li><a href="lan.asp">LAN配置</a></li>
        <li><a class="active" href="wan.asp">WAN配置</a></li>
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

<form method="post" action="wan.asp">
<input type="hidden" name="page" value="wan.asp">
<input type="hidden" name="wan_unit" value="0">
<input type="hidden" name="wan_primary" value="1">
<input type="hidden" name="wan_desc" value="<% nvram_get("wan_desc"); %>">

<div class="table-conf">
<div class="table-conf-title">
WAN上网设置
</div>

<br />
<table>
  <tr>
    <th width="310"
	onMouseOver="return overlib('Sets the connection name.', LEFT);"
	onMouseOut="return nd();">
	连接&nbsp;&nbsp;
    </th>
    <td>默认WAN连接</td>
  </tr>
  <tr>
    <th width="310">
	地址分配方式&nbsp;&nbsp;
    </th>
    <td>
	<select name="wan_proto" onChange="wan_proto_change();">
	  <option value="dhcp" <% nvram_match("wan_proto", "dhcp", "selected"); %>>DHCP</option>
	  <option value="pppoe" <% nvram_match("wan_proto", "pppoe", "selected"); %>>PPPoE</option>
	  <option value="static" <% nvram_match("wan_proto", "static", "selected"); %>>静态分配</option>
	</select>
    </td>
  </tr>
</table>

<br />
<table id="pppoesettings" style="display:none">
  <tr>
    <th width="310"
	onMouseOver="return overlib('Sets the username to use when authenticating with a PPPoE server.', LEFT);"
	onMouseOut="return nd();">
	PPPoE用户名&nbsp;&nbsp;
    </th>
    <td><input name="wan_pppoe_username" value="<% nvram_get("wan_pppoe_username"); %>"></td>
  </tr>
  <tr>
    <th width="310">
	PPPoE密码&nbsp;&nbsp;
    </th>
    <td><input name="wan_pppoe_passwd" value="<% nvram_get("wan_pppoe_passwd"); %>" type="password"></td>
  </tr>
  <tr>
    <th width="310">
	PPPoE服务名称&nbsp;&nbsp;
    </th>
    <td><input name="wan_pppoe_service" value="<% nvram_get("wan_pppoe_service"); %>"></td>
  </tr>
  <tr>
    <th width="310">
	PPPoE接入集中器&nbsp;&nbsp;
    </th>
    <td><input name="wan_pppoe_ac" value="<% nvram_get("wan_pppoe_ac"); %>"></td>
  </tr>
  <tr>
    <th width="310">
	PPPoE自动连接&nbsp;&nbsp;
    </th>
    <td>
	<select name="wan_pppoe_demand" onChange="wan_pppoe_demand_change();">
	  <option value="1" <% nvram_match("wan_pppoe_demand", "1", "selected"); %>>Enabled</option>
	  <option value="0" <% nvram_match("wan_pppoe_demand", "0", "selected"); %>>Disabled</option>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">
	PPPoE最大空闲周期&nbsp;&nbsp;
    </th>
    <td><input name="wan_pppoe_idletime" value="<% nvram_get("wan_pppoe_idletime"); %>" size="4" maxlength="4"></td>
  </tr>
  <tr>
    <th width="310">
	PPPoE自动心跳保活&nbsp;&nbsp;
    </th>
    <td>
	<select name="wan_pppoe_keepalive">
	  <option value="1" <% nvram_match("wan_pppoe_keepalive", "1", "selected"); %>>Enabled</option>
	  <option value="0" <% nvram_match("wan_pppoe_keepalive", "0", "selected"); %>>Disabled</option>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">
	PPPoE MRU&nbsp;&nbsp;
    </th>
    <td><input name="wan_pppoe_mru" value="<% nvram_get("wan_pppoe_mru"); %>" size="4" maxlength="4"></td>
  </tr>
  <tr>
    <th width="310">
	PPPoE链路最大传输单元&nbsp;&nbsp;
    </th>
    <td><input name="wan_pppoe_mtu" value="<% nvram_get("wan_pppoe_mtu"); %>" size="4" maxlength="4"></td>
  </tr>
</table>

<br />
<table>
  <tr>
    <th width="310">
	MAC Address&nbsp;&nbsp;
    </th>
    <td><input name="wan_hwaddr" readonly value="<% nvram_get("wan_hwaddr"); %>" size="18" maxlength="17"></td>
  </tr>
  <tr>
    <th width="310">
	IP地址&nbsp;&nbsp;
    </th>
    <td><input name="wan_ipaddr" value="<% nvram_get("wan_ipaddr"); %>" size="16" maxlength="15"></td>
  </tr>
  <tr>
    <th width="310">
	子网掩码&nbsp;&nbsp;
    </th>
    <td><input name="wan_netmask" value="<% nvram_get("wan_netmask"); %>" size="16" maxlength="15"></td>
  </tr>
  <tr>
    <th width="310">
	默认网关&nbsp;&nbsp;
    </th>
    <td><input name="wan_gateway" value="<% nvram_get("wan_gateway"); %>" size="16" maxlength="15"></td>
  </tr>
  <tr>
    <th width="310" valign="center" rowspan="3">
	<input type="hidden" name="wan_dns" value="4">
	DNS服务器&nbsp;&nbsp;
    </th>
    <td><input name="wan_dns0" value="<% nvram_list("wan_dns", 0); %>" size="15" maxlength="15"></td>
  </tr>
  <tr>
    <td><input name="wan_dns1" value="<% nvram_list("wan_dns", 1); %>" size="15" maxlength="15"></td>
  </tr>
  <tr>
    <td><input name="wan_dns2" value="<% nvram_list("wan_dns", 2); %>" size="15" maxlength="15"></td>
  </tr>
  <tr>
    <th width="310" valign="center" rowspan="3">
	<input type="hidden" name="wan_wins" value="4">
	WINS服务器&nbsp;&nbsp;
    </th>
    <td><input name="wan_wins0" value="<% nvram_list("wan_wins", 0); %>" size="15" maxlength="15"></td>
  </tr>
  <tr>
    <td><input name="wan_wins1" value="<% nvram_list("wan_wins", 1); %>" size="15" maxlength="15"></td>
  </tr>
  <tr>
    <td><input name="wan_wins2" value="<% nvram_list("wan_wins", 2); %>" size="15" maxlength="15"></td>
  </tr>
</table>

<br />
<table>
  <tr>
    <th width="311"
	onMouseOver="return overlib('Shows the state of the connection.', LEFT);"
	onMouseOut="return nd();">
	链接状态&nbsp;&nbsp;
    </th>
    <td id="wan_link_status">"未连接"</td>
  </tr>
  <tr>
    <th width="311"
	onMouseOver="return overlib('Shows the IP address lease info.', LEFT);"
	onMouseOut="return nd();">
	IP地址过期时间&nbsp;&nbsp;
    </th>
	<td><% wan_lease(); %></td>
  </tr>
</table>
</div>

<div class="table-apply">
<br />
<table width="420px">
  <tr>
    <td>
	<input type="submit" name="action" value="应用">
	<input type="reset" name="action" value="放弃">
	<input type="submit" name="action" value="释放IP地址">
	<input type="submit" name="action" value="重新请求IP地址">
    </td>
  </tr>
</table>
</div>
</form>

<!--
<p class="footer">&#170;2016-2017 LanNet Corporation. All rights reserved.</p>
-->

</body>
</html>
