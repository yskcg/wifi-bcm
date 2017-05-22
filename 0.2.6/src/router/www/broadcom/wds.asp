<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="zh-Hans">
<head>
<title>Gateway design: WDS</title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script language="JavaScript" type="text/javascript">
function wdsisvalid()
{
    var wlsecuritypsk = "<% nvram_inlist("wl_akm", "psk", "1"); %>";
    var wlsecuritypsk2 = "<% nvram_inlist("wl_akm", "psk2", "1"); %>";
    if (!wlsecuritypsk.localeCompare("1") || !wlsecuritypsk2.localeCompare("1"))
    {
        return false;
    }
    else
    {
        return true;
    }
    
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

    $("form").submit(function(e)
    {
        if(!wdsisvalid())
        {
            e.preventDefault();
            $("input[name='action']").val("应用");
        }
    });

    if(wdsisvalid())
    {
        $(".table-conf-title").hide();
    }
    else
    {
        $("input[name*='action']").prop("disabled", true);
    }
});
</script>
</head>

<body>
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
            <li><a href="security.asp">安全配置</a></li>
            <li><a href="pwps.asp">WPS</a></li>
            <li><a class="active" href="wds.asp">WDS</a></li>
        </ul>
        </li>
        <li><a href="firmware.asp">固件升级</a></li>
    </ul>
</nav>

<form method="post" action="wds.asp">
<input type="hidden" name="page" value="wds.asp">
<input type="hidden" name="wl_unit" value="0">
<input type="hidden" name="wl_wds" value="1">
<input type="hidden" name="wl_lazywds" value="1">
<input type="hidden" name="wl_wds_timeout" value="1">

<div class="table-conf">
<div class="table-conf-title">
注意:WDS只能在非加密模式下使用，请先在<a href="security.asp">安全设置</a>页中禁用WPA-PSK/WPA2-PSK
</div>
<br />
<br />
<table>
  <tr>
    <th width="310">桥接AP MAC:&nbsp;&nbsp;</th>
    <td><input name="wl_wds0" value="<% nvram_list("wl_wds", 0); %>" size="17" maxlength="17"></td>
  </tr>
  <tr>
    <th width="310">桥接状态:&nbsp;&nbsp;</th>
    <td><% wl_wds_status(0); %></td>
  </tr>
  <tr>
</table>
</div>

<div class="table-apply">
<br />
</table>
  <tr>
    <td><input type="submit" name="action" value="应用"></td>
	<input type="reset" name="action" value="放弃">
  </tr>
</table>
</div>
</form>

<!--
<br /><br /><br /><br /><br /><br /><br /><br /><br /><br />
<p class="footer">&#169;2016-2017 LanNet Corporation. All rights reserved.</p>
-->

</body>
</html>
