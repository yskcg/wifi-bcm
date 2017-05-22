<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="zh-Hans">
<head>
<title>Gateway design: Basic</title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script language="JavaScript" type="text/javascript">
function basicLoad()
{
    var br2strstart = $("#wanstr").text().indexOf("br2");
    var br2strend = $("#wanstr").text().indexOf(")" ,br2strstart);
    var br2str = $("#wanstr").text().slice(br2strstart, br2strend+1);
    $("#wanstr").text(br2str);

    var fw_disable = "<% nvram_get("fw_disable"); %>";
    var fw_disable_cell = document.getElementById("sysinfo").rows[3].cells;
    if(fw_disable.localeCompare("1") == 0) {
       fw_disable_cell[1].innerHTML = "Disableb";
    }

    var dhcp_server_enable = "<% nvram_get("lan_proto"); %>";
    var dhcp_server_enable_cell = document.getElementById("dhcpserver").rows[0].cells;
    if(dhcp_server_enable.localeCompare("dhcp") != 0) {
       dhcp_server_enable_cell[1].innerHTML = "Disabled";
    }

    var pppoeod = "<% nvram_get("wan_pppoe_demand"); %>";
    var pppoeod_cell = document.getElementById("pppoeinfo").rows[2].cells;
    if(pppoeod.localeCompare("1") == 0) {
       pppoe_cell[1].innerHTML = "Enabled";
    }

    var pppoekpa = "<% nvram_get("wan_pppoe_keepalive"); %>";
    var pppoekpa_cell = document.getElementById("pppoeinfo").rows[4].cells;
    if(pppoekpa.localeCompare("1") == 0) {
       pppoekpa_cell[1].innerHTML = "Enabled";
    }

    var wlband = "<% wl_cur_phytype(); %>";
    var wlband_cell = document.getElementById("wirelessinfo").rows[2].cells;
    wlband_cell[1].innerHTML = wlband.substr(3);

    var wlbandwidth = "<% wl_cur_nbw(); %>";
    var wlbandwidth_cell = document.getElementById("wirelessinfo").rows[3].cells;
    wlbandwidth_cell[1].innerHTML = wlbandwidth.substr(3);

    var wlsecuritypsk = "<% nvram_inlist("wl_akm", "psk", "1"); %>";
    var wlsecuritypsk2 = "<% nvram_inlist("wl_akm", "psk2", "1"); %>";
    var wlsecurity_cell = document.getElementById("wirelessinfo").rows[5].cells;
    if (!wlsecuritypsk.localeCompare("1") && !wlsecuritypsk2.localeCompare("1"))
        wlsecurity_cell[1].innerHTML = "WPA/WPA2 Mixed";
    else if (!wlsecuritypsk.localeCompare("1"))
        wlsecurity_cell[1].innerHTML = "WPA";
    else if (!wlsecuritypsk2.localeCompare("1"))
        wlsecurity_cell[1].innerHTML = "WPA2";
    else
        wlsecurity_cell[1].innerHTML = "Unknown";

    document.getElementById("pppoeinfo").style.display="none";
    document.getElementById("pppoetitle").style.display="none";
    var wan_proto = "<% nvram_get("wan_proto"); %>";
    if(!wan_proto.localeCompare("pppoe")){
        document.getElementById("pppoeinfo").style.display="table";
        document.getElementById("pppoetitle").style.display="table";
    }
}
</script>
</head>

<body onLoad="basicLoad()">
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
        <li><a class="active" href="index.asp">基本信息</a></li>
        <li><a href="lan.asp">LAN配置</a></li>
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

<div class="table-info">
<div class="table-info-title">
WAN连接信息
</div>
<table>
  <tr>
    <th width="310" rowspan="2" >连接描述&nbsp;&nbsp;</th>
    <td>默认连接</td>
  <tr>
    <td id="wanstr"><% wan_iflist(); %></td>
  </tr>
  <tr>
    <th width="310">WAN连接方式&nbsp;&nbsp;</th>
    <td><% nvram_get("wan0_proto"); %></td>
  </tr>
  <tr>
    <th width="310">MAC地址&nbsp;&nbsp;</th>
    <td><% nvram_get("wan0_hwaddr"); %></td>
  </tr>
  <tr>
    <th width="310">IP地址&nbsp;&nbsp;</th>
    <td><% nvram_get("wan0_ipaddr"); %></td>
  </tr>
  <tr>
    <th width="310">子网掩码&nbsp;&nbsp;</th>
    <td><% nvram_get("wan0_netmask"); %></td>
  </tr>
  <tr>
    <th width="310">默认网关&nbsp;&nbsp;</th>
    <td><% nvram_get("wan0_gateway"); %></td>
  </tr>
  <tr>
    <th width="310" rowspan="3">DNS查询服务器地址&nbsp;&nbsp;</th>
    <td><% nvram_list("wan0_dns", 0); %></td>
  </tr>
  <tr>
    <td><% nvram_list("wan0_dns", 1); %></td>
  </tr>
  <tr>
    <td><% nvram_list("wan0_dns", 2); %></td>
  </tr>
  <tr>
    <th width="310" rowspan="3">WINS服务器地址&nbsp;&nbsp;</th>
    <td><% nvram_list("wan0_wins", 0); %></td>
  </tr>
  <tr>
    <td><% nvram_list("wan0_wins", 1); %></td>
  </tr>
  <tr>
    <td><% nvram_list("wan0_wins", 2); %></td>
  </tr>
</table>

<div id="pppoetitle" class="table-info-title">
<br />
<br />
PPPoE information
</div>
<table id="pppoeinfo">
  <tr>
    <th width="310">PPPoE Username&nbsp;&nbsp;</th>
    <td><% nvram_get("wan_pppoe_username")%></td>
  </tr>
  <tr>
    <th width="310">PPPoE Password&nbsp;&nbsp;</th>
    <td><% nvram_get("wan_pppoe_passwd"); %></td>
  </tr>
  <tr>
    <th width="310">PPPoE Connect on Demand&nbsp;&nbsp;</th>
    <td>Disabled</td>
  </tr>
  <tr>
    <th width="310">PPPoE Max Idle Time&nbsp;&nbsp;</th>
    <td><% nvram_get("wan_pppoe_idletime"); %></td>
  </tr>
  <tr>
    <th width="310">PPPoE Keep Alive&nbsp;&nbsp;</th>
    <td>Disabled</td>
  </tr>
  <tr>
    <th width="310">PPPoE MRU&nbsp;&nbsp;</th>
    <td><% nvram_get("wan_pppoe_mru"); %></td>
  </tr>
  <tr>
    <th width="310">PPPoE MTU&nbsp;&nbsp;</th>
    <td><% nvram_get("wan_pppoe_mtu"); %></td>
  </tr>
</table>

<br />
<br />
<div class="table-info-title">
Lan连接信息
</div>
<table >
  <tr>
    <th width="310">连接描述&nbsp;&nbsp;</th>
    <td>默认Lan网络</td>
  </tr>
  <tr>
    <th width="310">MAC地址&nbsp;&nbsp;</th>
    <td><% nvram_get("lan_hwaddr"); %></td>
  </tr> 
  <tr>
     <th width="310">地址配置方式&nbsp;&nbsp;</th>
    <td>静态指定</td>
  </tr>  
  <tr>
    <th width="310">IP地址&nbsp;&nbsp;</th>
    <td><% nvram_get("lan_ipaddr"); %></td>
  </tr>
  <tr>
    <th width="310">子网掩码&nbsp;&nbsp;</th>
    <td><% nvram_get("lan_netmask"); %></td>
  </tr>
  <tr>
    <th width="310">默认网关&nbsp;&nbsp;</th>
    <td><% nvram_get("lan_gateway"); %></td>
  </tr>
</table>

<br />
<br />
<div class="table-info-title">
Lan DHCP服务地址分配信息
</div>
<table id="dhcpserver">
  <tr>
    <th width="310">DHCP服务启用状态&nbsp;&nbsp;</th>
    <td>启用</td>
  </tr>
  <tr>
    <th width="310">DHCP地址池起始地址&nbsp;&nbsp;</th>
    <td><% nvram_get("dhcp_start"); %></td>
  </tr>
  <tr>
    <th width="310">DHCP地址池结束地址&nbsp;&nbsp;</th>
    <td><% nvram_get("dhcp_end"); %></td>
  </tr>
  <tr>
    <th width="310">DHCP租约时间&nbsp;&nbsp;</th>
    <td><% nvram_get("lan_lease"); %></td>
  </tr>
</table>

<br />
<br />
<div class="table-info-title">
WIFI连接信息
</div>
<table id="wirelessinfo">
  <tr>
    <th width="310">SSID&nbsp;&nbsp;</th>
    <td><% nvram_get("wl0_ssid"); %></td>
  </tr>
  <tr>
    <th width="310">WIFI MAC地址&nbsp;&nbsp;</th>
    <td><% nvram_get("wl0_hwaddr"); %></td>
  </tr>
  <tr>
    <th width="310">802.11频段 &nbsp;&nbsp;</th>
    <td>2.4Ghz</td>
  </tr>
  <tr>
    <th width="310">占用频带宽度&nbsp;&nbsp;</th>
    <td><% wl_cur_nbw(); %></td>
  </tr>
  <tr>
    <th width="310"><% wl_control_string(); %> 工作信道:&nbsp;&nbsp;</th>
    <td><% nvram_get("wl_channel"); %></td>
  </tr>
  <tr>
    <th width="310">加密认证方式&nbsp;&nbsp;</th>
    <td>WPA/WPA2 mixed</td>
  </tr>
  <tr>
    <th width="310">WIFI工作模式&nbsp;&nbsp;</th>
    <td>AP</td>
  </tr>
</table>

<br />
<br />
<div class="table-info-title">
系统信息
</div>
<table id="sysinfo">
  <tr>
    <th width="310">系统已运行时间&nbsp;&nbsp;</th>
    <td><% sysuptime(); %></td>
  </tr>
  <tr>
    <th width="310">路由管理用户名&nbsp;&nbsp;</th>
    <td><% nvram_get("http_username"); %></td>
  </tr>
  <tr>
    <th width="310">路由管理密码&nbsp;&nbsp;</th>
    <td><% nvram_get("http_passwd"); %></td>
  </tr>
  <tr>
    <th width="310">禁止WAN口登录管理页面&nbsp;&nbsp;</th>
    <td>启用</td>
  </tr>
</table>

</div>

<!--
<p class="footer">&#169;2016-2017 LanNet Corporation. All rights reserved.</p>
-->

</body>
</html>
