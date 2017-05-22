<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="zh-Hans">
<head>
<title>Gateway design: Security</title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script language="JavaScript" type="text/javascript">

function wl_security_update()
{
	var i, cur, algos;
	var wl_ure = <% wl_ure_enabled(); %>;
	var wl_ibss = <% wl_ibss_mode(); %>;
	var wl_nmode = <% wl_nmode_enabled(); %>;

	/* Save current crypto algorithm */
	for (i = 0; i < document.forms[0].wl_crypto.length; i++) {
		if (document.forms[0].wl_crypto[i].selected) {
			cur = document.forms[0].wl_crypto[i].value;
			break;
		}
	}

	/* Define new crypto algorithms */
	if (<% wl_corerev(); %> >= 3) {
		if (wl_ibss == "1") {
			algos = new Array("AES");
		}
		else if (wl_nmode == "1") {
			algos = new Array("AES", "TKIP+AES");
		}
		else {
			algos = new Array("TKIP", "AES", "TKIP+AES");
		}
	} else {
		if (wl_ibss == "0")
			algos = new Array("TKIP");
		else
			algos = new Array("");
	}

	/* Reconstruct algorithm array from new crypto algorithms */
	document.forms[0].wl_crypto.length = algos.length;
	for (var i in algos) {
		document.forms[0].wl_crypto[i] = new Option(algos[i], algos[i].toLowerCase());
		document.forms[0].wl_crypto[i].value = algos[i].toLowerCase();
		if (document.forms[0].wl_crypto[i].value == cur)
			document.forms[0].wl_crypto[i].selected = true;
	}

    //if (<% wl_corerev(); %> >= 3 && wl_nmode == "1") {
        document.forms[0].wl_wep.selectedIndex = 1;
    //}
}

function wpapsk_window() 
{
	var psk_window = window.open("", "", "toolbar=no,scrollbars=yes,width=400,height=100");
	psk_window.document.write("密码是<% nvram_get("wl_wpa_psk"); %>");
	psk_window.document.close();
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
</script>
</head>

<body onLoad="wl_security_update();">
<div id="overDiv" style="position:absolute; visibility:hidden; z-index:1000;"></div>

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
            <li><a class="active" href="security.asp">安全配置</a></li>
            <li><a href="pwps.asp">WPS</a></li>
            <li><a href="wds.asp">WDS</a></li>
        </ul>
        </li>
        <li><a href="firmware.asp">固件升级</a></li>
    </ul>
</nav>

<form method="post" action="security.asp">
<input type="hidden" name="page" value="security.asp">
<input type="hidden" name="wl_wps_mode" value="<% nvram_get("wl_wps_mode"); %>">
<input type="hidden" name="wl_akm">
<div class="table-conf">
<br />
<table>
  <tr>
    <th width="310">SSID:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_unit" onChange="submit();">
	  <% wl_list("INCLUDE_SSID" , "INCLUDE_VIFS"); %>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">802.11认证方式:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_auth">
	  <option value="1" <% nvram_match("wl_auth", "1", "selected"); %> disabled>共享</option>
	  <option value="0" <% nvram_invmatch("wl_auth", "1", "selected"); %>>开放</option>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">WPA-PSK:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_akm_psk" >
	  <option value="enabled" <% nvram_inlist("wl_akm", "psk", "selected"); %>>启用</option>
	  <option value="disabled" <% nvram_invinlist("wl_akm", "psk", "selected"); %>>关闭</option>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">WPA2-PSK:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_akm_psk2" >
	  <option value="enabled" <% nvram_inlist("wl_akm", "psk2", "selected"); %>>启用</option>
	  <option value="disabled" <% nvram_invinlist("wl_akm", "psk2", "selected"); %>>关闭</option>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">WEP加密:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_wep">
	  <option value="enabled" <% nvram_match("wl_wep", "enabled", "selected"); %> disabled>启用</option>
	  <option value="disabled" <% nvram_invmatch("wl_wep", "enabled", "selected"); %>>关闭</option>
 	</select>
    </td>
  </tr>
  <tr>
    <th width="310">
	<div id="wl_wpa_encrypt_div">
	WPA加密:&nbsp;&nbsp;
	</div>
    </th>
    <td>
	<select name="wl_crypto">
	  <option value="tkip" <% nvram_match("wl_crypto", "tkip", "selected"); %>>TKIP</option>
	  <option value="aes" <% nvram_match("wl_crypto", "aes", "selected"); %>>AES</option>
	  <option value="tkip+aes" <% nvram_match("wl_crypto", "tkip+aes", "selected"); %>>TKIP+AES</option>
 	</select>
    </td>
  </tr>
  <tr>
    <th width="310">
	<div id="wl_wpa_psk_div">
	WPA密码:&nbsp;&nbsp;
	</div>
    </th>
    <td><input name="wl_wpa_psk" value="<% nvram_get("wl_wpa_psk"); %>" type="password" size="16">
    <A HREF="javascript:wpapsk_window()">显示密码</A></td>
  </tr>
</table>
</div>

<div class="table-apply">
<br />
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
<br /><br /><br /><br /><br />
<p class="footer">&#169;2016-2017 LanNet Corporation. All rights reserved.</p>
-->

</body>
</html>
