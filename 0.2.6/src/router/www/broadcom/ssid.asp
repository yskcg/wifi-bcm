<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="zh-Hans">
<head>
<title>Gateway design: SSID</title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script language="JavaScript" type="text/javascript">

function wl_recalc()
{
   //var ure_disable = "<% nvram_get("ure_disable"); %>";
   var wme_enabled = "<% nvram_get("wl_wme"); %>";
   var mode = "<% nvram_get("wl_mode"); %>";

   document.forms[0].wl_mode.disabled = 1;
   /*
   if (ure_disable == "0") {
		document.forms[0].wl_bridge.disabled = 1;
   }*/

   if (wme_enabled != "on") {
		document.forms[0].wl_wme_bss_disable.disabled = 1;
   }
   if (mode == "sta") {
		document.forms[0].wl_wmf_bss_enable.disabled = 1;
		document.forms[0].wl_bss_maxassoc.disabled = 1;
   }
   if(mode == "ap") {
   }
   var wl_bssid = document.getElementById("wl_bssid_idx");
   var wl_bssid_idx = wl_bssid.options[wl_bssid.selectedIndex].value;

   if(wl_bssid_idx == "1") {
       var wl1_ssid = "<% nvram_get("wl0.1_ssid"); %>";
       $("#wl_ssid_td").text(wl1_ssid);
       $("#wl_ssid_tailer").prop("disabled", true);
       $("#wl_ssid").val(wl1_ssid);

       $("#wl_bridge_td").text("桥接");
   }

   if(wl_bssid_idx == "0") {
      var wl0_ssid = "<% nvram_get("wl0_ssid"); %>";
      var wl0_ssid_header = "<% nvram_get("wl0_ssidheader"); %>";
      var wl0_ssidheder_len = wl0_ssid_header.length;
      $("#wl_ssid_tailer").val(wl0_ssid.substr(wl0_ssidheder_len));
      $("#wl_ssid").val(wl0_ssid);
      
      $("#wl_bridge_td").text("路由");
   }
   $("#wl_wmf_bss_enable option[value=" + 1 +"]").attr('disabled','disabled');

   $("#wl_ssid_tailer").change(function() {
        var wl_bssid_idx = wl_bssid.options[wl_bssid.selectedIndex].value;
        if(wl_bssid_idx == "0"){
          var wl0_ssid = $("#wl_ssid_td").text().concat($("#wl_ssid_tailer").val());
          $("#wl_ssid").val(wl0_ssid);
        }
        else if(wl_bssid_idx == "1"){
            $("#wl_ssid").val($("#wl_ssid_td").text());
        }
   });

/*
   $("#wl_bssid_idx").change(function() {
        var wl_bssid_idx = wl_bssid.options[wl_bssid.selectedIndex].value;
        if(wl_bssid_idx == "0"){
          var wl0_ssid = $("#wl_ssid_td").text().concat($("#wl_ssid_tailer").val());
          $("#wl_ssid").val(wl0_ssid);
        }
        else if(wl_bssid_idx == "1"){
            $("#wl_ssid").val($("#wl_ssid_td").text());
        }
   });*/

   $("form").submit(function() {
        var wl_bssid_idx = wl_bssid.options[wl_bssid.selectedIndex].value;
        if(wl_bssid_idx == "0")
          $("#wl_ssid_tailer").remove();
   });
}

function wl_closed_check()
{
/*
*/
<% wps_closed_check_display(); %>
/*
*/
}

function wl_macmode_check()
{
/*
*/
<% wps_macmode_check_display(); %>
/*
*/
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

//-->
</script>
</head>

<body onLoad="wl_recalc();">
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
        <li><a href="wan.asp">WAN配置</a></li>
        <li><a data-toggle="#list" href="#">无线配置</a>
        <ul class="in" id="list" style="display:block">
            <li><a href="radio.asp">射频配置</a></li>
            <li><a class="active" href="ssid.asp">SSID配置</a></li>
            <li><a href="security.asp">安全配置</a></li>
            <li><a href="pwps.asp">WPS</a></li>
            <li><a href="wds.asp">WDS</a></li>
        </ul>
        </li>
        <li><a href="firmware.asp">固件升级</a></li>
    </ul>
</nav>

<form method="post" action="ssid.asp">
<input type="hidden" name="page" value="ssid.asp">
<input type="hidden" name="wl_mode_changed" value=0>
<input type="hidden" name="wl_ure_changed" value=0>
<input type="hidden" name="wl_ssid" id="wl_ssid" value="HiTV_01:02:03">

<div class="table-conf">
<table>
  <tr>
    <th width="310">WIFI网卡地址:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_unit" onChange="submit();">
	  <% wl_list(); %>
	</select>
    </td>
  </tr>
    <tr>
        <th width="310">SSID/Mac:&nbsp;&nbsp;</th>
        <td colspan="2">
	        <select name="wl_bssid" id="wl_bssid_idx" onChange="submit()">
            <% wl_bssid_list(); %>
            </select>
        </td>
    </tr>
    <tr>
        <th width="310">工作模式:&nbsp;&nbsp;</th>
        <td colspan="2">
            <select name="wl_mode">
            <% wl_mode_list();%>
        </select>
        </td>
    </tr>
    <tr>
        <th width="310">启用:&nbsp;&nbsp;</th>
        <td colspan="2">
            <select name="wl_bss_enabled">
            <option value="0" <% nvram_match("wl_bss_enabled", "0", "selected"); %>>关闭</option> 
            <option value="1" <% nvram_match("wl_bss_enabled", "1", "selected"); %>>启用</option>
            </select>
        </td>
    </tr>  
    <tr>
        <th width="310">SSID:&nbsp;&nbsp;</th>
        <td id="wl_ssid_td"><% nvram_get("wl0_ssidheader"); %><input name="wl_ssid_tailer" id="wl_ssid_tailer" value="" maxlength="26" type="text"></td>
    </tr>  
    <tr>
        <th width="310">SSID广播:&nbsp;&nbsp;</th>
        <td colspan="2">
            <select name="wl_closed" onChange="wl_closed_check();">
	            <option value="0" <% nvram_match("wl_closed", "0", "selected"); %>>广播</option>
                <option value="1" <% nvram_match("wl_closed", "1", "selected"); %>>隐藏</option>
            </select>
        </td>
    </tr>
    <tr>
        <th width="310">AP隔离:&nbsp;&nbsp;</th>
        <td colspan="2">
	        <select name="wl_ap_isolate">
	            <option value="0" <% nvram_match("wl_ap_isolate", "0", "selected"); %>>关闭</option>
                <option value="1" <% nvram_match("wl_ap_isolate", "1", "selected"); %>>启用</option>
            </select>
        </td>
    </tr>
    <tr>
        <th width="310">路由/桥接:&nbsp;&nbsp;</th>
        <td colspan="2" id="wl_bridge_td">
            Nat
        </td>
    </tr>  
    <tr>
        <th width="310">最大终端数量:&nbsp;&nbsp;</th>
        <td colspan="2">
            <input name="wl_bss_maxassoc" value="<% nvram_get("wl_bss_maxassoc"); %>" size="4" maxlength="4">
        </td>
    </tr>
    <tr>
        <th width="310">WMM:&nbsp;&nbsp;</th>
        <td colspan="2">
	        <select name="wl_wme_bss_disable">
	            <option value="0" <% nvram_match("wl_wme_bss_disable", "0", "selected"); %>>启用</option>
	            <option value="1" <% nvram_match("wl_wme_bss_disable", "1", "selected"); %>>禁用</option>
            </select>
        </td>
    </tr>
    <tr>
        <th width="310">WMF:&nbsp;&nbsp;</th>
        <td colspan="2">
	    <select name="wl_wmf_bss_enable" id="wl_wmf_bss_enable">
	        <option value="1" <% nvram_match("wl_wmf_bss_enable", "1", "selected"); %>>启用</option>
	        <option value="0" <% nvram_match("wl_wmf_bss_enable", "0", "selected"); %>>禁用</option>
        </select>
    </td>
    </tr>
</table>
</div>

<br />
<div class="wifi-assoc">
<h3>
Authenticated Stations
</h3>
<table>
	  <tr align="center">
	    <td width="100px">MAC地址</td>
	    <td>上线时间</td>
	    <td>认证状态</td>
	    <td>WMM状态</td>
	    <td>节能状态</td>
	    <td>APSD状态</td>
	  </tr>
	  <% wl_auth_list(); %>
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
<p class="footer">&#169;2016-2017 LanNet Corporation. All rights reserved.</p>
-->

</body>
</html>
