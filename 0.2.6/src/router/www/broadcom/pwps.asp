<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="zh-Hans">
<head>
<title>Gateway design: WPS</title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script language="JavaScript" type="text/javascript" src="circle-progress.min.js"></script>
<script language="JavaScript" type="text/javascript">
<!--

/*	
*/

var interval = 5;
var nvinterval = "<% nvram_get("wfi_refresh_interval"); %>" ;
var wfi = "<% nvram_match("wl_wfi_enable", "1", "1"); %>";

if(nvinterval)
{
	interval = parseInt(nvinterval);
}

function autoRefresh()
{
	if(wfi == 1){
		setTimeout("doRefresh()", interval * 1000 );
	}
}

function doRefresh()
{

	document.forms[0].action.value = "Select";
	document.forms[0].submit();
}
function writeStrRefres()
{
	if(wfi == 1){
		document.writeln("Auto refreshing every 30 seconds");
	}
}
/*	
*/

function wps_config_change()
{
    <% wps_config_change_display(); %>;
    $("#apply-table").show();
}

function wps_current_psk_window() 
{
<% wps_current_psk_window_display(); %>
}

function wps_psk_window() 
{
<% wps_psk_window_display(); %>
}

/*
function wps_akm_change()
{
	var akm = document.forms[0].wps_akm[document.forms[0].wps_akm.selectedIndex].value;
	var action = document.forms[0].wps_action.value;
	
	if (action == "ConfigAP" || action == "AddEnrollee") {
		if (akm == 0) {
			document.forms[0].wps_crypto.disabled = 1;
			document.forms[0].wps_crypto.value = "0";
			document.forms[0].wps_psk.disabled = 1;
		}
		else {
			document.forms[0].wps_crypto.disabled = 0;
			document.forms[0].wps_psk.disabled = 0;
		}
	}
}*/

function wps_valid_check()
{
    var wpsisvalid = true;

    closed_network = "<% nvram_match("wl_closed", "1", "1"); %>";
    sharedauth_meth = "<% nvram_match("wl_auth", "1", "1"); %>";
    psk2 = "<% nvram_inlist("wl_akm", "psk2", "1"); %>";
    aes = "<% nvram_match("wl_crypto", "aes", "1"); %>";
    tkipaes = "<% nvram_match("wl_crypto", "tkip+aes", "1"); %>";
    wepenable = "<% nvram_match("wl_wep", "enabled", "selected"); %>";

    if (!closed_network.localeCompare("1"))
    {
        wpsisvalid = false;
    }
    else
    {
        $("#closed_network").hide();
    }

    if (!sharedauth_meth.localeCompare("1"))
    {
        wpsisvalid = false;
    }
    else
    {
        $("#sharedauth_meth").hide();
    }

    if (!psk2.localeCompare("1"))
    {
        $("#psk2").hide();
    }
    else
    {
        wpsisvalid = false;
    }

    if (!aes.localeCompare("1") || !tkipaes.localeCompare("1"))
    {
        $("#aes").hide();
    }
    else
    {
        wpsisvalid = false;
    }

    if (!wepenable.localeCompare("1"))
    {
        wpsisvalid = false;
    }
    else
    {
        $("#wep").hide();
    }

    if(!wpsisvalid)
    {
        $("#wl_wps_mode option[value=" + "enabled" +"]").attr('disabled','disabled');
    }
}

function wps_get_ap_config_submit()
{
	<% wps_get_ap_config_submit_display(); %>
}

function pre_submit()
{
	var action = document.forms[0].wps_action.value;
	var akm = document.forms[0].wps_akm[document.forms[0].wps_akm.selectedIndex].value;

	if (action == "ConfigAP" || action == "AddEnrollee") {
		/* Check WPS in OPEN security */
		if (akm == "0")
			return confirm("Are you sure to configure WPS in Open security?");
	}
	if (action == "GetAPConfig")
		return wps_get_ap_config_submit();
	return true;
}

function doreload()
{
    location.reload(true);
}

function reloadpage()
{
    setTimeout("doreload()", 5 * 1000 );
}

function wps_onLoad()
{
/*	
*/
//   autoRefresh();
/*	
*/
	//wps_akm_change();

    wps_valid_check();

    var wlwpsmode = $("#wl_wps_mode").val();
    if(!wlwpsmode.localeCompare("enabled"))
        $("#apply-table").hide();
    else
        $("#apply-table").show();

}

function showprocessing()
{
  $('#circle').circleProgress({
    value: 1.0,
    size: 40,
    thickness: 6,
    fill: {
      gradient: ["red", "orange"]
    }
  });
}

$(document).ready(function(){
	$("input[name='action']").click(function(){
		var actionpair = [
			['应用','Apply'],
			['开始配对','Add Enrollee'],
			['停止配对','STOPWPS'],
			['再次配对','PBC Again'],
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

    $("#circle").on('circle-animation-end', function(event) {
        $("#circle").circleProgress();
    });

});

//-->
</script>
</head>

<body onload="wps_onLoad();">
<div id="overDiv" style="position: absolute; visibility: hidden; z-index: 1000;"></div>

<br />
<table>
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
        <li><a  data-toggle="#list" href="#">无线配置</a>
        <ul class="in" id="list" style="display:block">
            <li><a href="radio.asp">射频配置</a></li>
            <li><a href="ssid.asp">SSID配置</a></li>
            <li><a href="security.asp">安全配置</a></li>
            <li><a class="active" href="pwps.asp">WPS</a></li>
            <li><a href="wds.asp">WDS</a></li>
        </ul>
        </li>
        <li><a href="firmware.asp">固件升级</a></li>
    </ul>
</nav>

<form method="post" action="pwps.asp">
<input name="page" value="pwps.asp" type="hidden">
<input name="invite_name" value="0" type="hidden">
<input name="invite_mac" value="0" type="hidden">
<input name="wl_wfi_enable" value="0" type="hidden">
<input name="wl_wfi_pinmode" value="0" type="hidden">

<div class="table-conf">
<br />
<table>
  <tr>
    <th width="310">WiFi网络:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_unit" onChange="submit();">
	  <% wl_list("INCLUDE_SSID" , "INCLUDE_VIFS"); %>
	</select>
    </td>
  </tr>
</table>

<br />

<div class="table-conf-title">
<div id="closed_network">
注意:WPS仅能在SSID广播启用的情况下使用，请先在<a href="ssid.asp">SSID配置</a>页中启用SSID广播
</div>
<div id="sharedauth_meth">
注意:WPS仅能在认证方式为开放的情况下使用，请先在<a href="security.asp">安全配置</a>页中选择802.11认证方式为开放模式
</div>
<div id="psk2">
注意:WPS仅能在启用wpa2-psk的情况下使用，请先在<a href="security.asp">安全配置</a>页中启用wpa2-psk
</div>
<div id="aes">
注意:WPS仅能在加密方式为aes或tkip+aes的情况下使用，请先在<a href="security.asp">安全配置</a>页中选择WPA加密为aes或者tkip+aes
</div>
<div id="wep">
注意:WPS仅能在wep加密关闭的情况下使用，请先在<a href="security.asp">安全配置</a>页中禁用wep加密
</div>
</div>

<% pwps_display(); %>
</div>

<div class="table-apply" id="apply-table">
<br />
<table>
    <tr>
      <td width="310"></td>
      <td>&nbsp;&nbsp;</td>
      <td>
	  <input type="submit" name="action" value="应用" onclick="wps_config_change()";>
	  <input type="reset" name="action" value="放弃">
      </td>
    </tr>
</table>
</div>
</form>

<!--
<p class="label">&#169;2016-2017 LanNet Corporation. All rights reserved.</p>
-->

</body>
</html>
