<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="zh-Hans">
<head>
<title>Gateway design: Firmware</title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script>

var start = new Date();
var maxTime = 55000;
var timeoutVal = Math.floor(maxTime/100);

function updateProgress(percentage) {
    $('#pbar_innerdiv').css("width", percentage + "%");
    $('#pbar_innertext').text(percentage + "%");
}

function animateUpdate() {
    var now = new Date();
    var timeDiff = now.getTime() - start.getTime();
    var perc = Math.round((timeDiff/maxTime)*100);
	if (perc <= 100) {
		updateProgress(perc);
		setTimeout(animateUpdate, timeoutVal);
	}
}

$(document).ready(function(){
	$("input[name='action']").click(function(){
		var actionpair = [
			['升级到新的固件','Upload new Firmware']
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
		
		$(this).hide();
		$("#pbar").show();
		start = new Date();
		animateUpdate();
	});

	$("#pbar").hide();
});
</script>
</head>

<body>
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
        <ul class="out" id="list">
            <li><a href="radio.asp">射频配置</a></li>
            <li><a href="ssid.asp">SSID配置</a></li>
            <li><a href="security.asp">安全配置</a></li>
            <li><a href="pwps.asp">WPS</a></li>
        </ul>
        <li><a class="active" href="firmware.asp">固件升级</a></li>
    </ul>
</nav>

<form method="post" action="upgrade.cgi" enctype="multipart/form-data">
<input type="hidden" name="page" value="firmware.asp">

<div class="table-conf">
<table>
  <tr>
    <th width="310">Boot Loader版本号:&nbsp;&nbsp;</th>
    <td><% nvram_get("pmon_ver"); %></td>
  </tr>
  <tr>
    <th width="310">Linux版本号:&nbsp;&nbsp;</th>
    <td><% nvram_match("os_name", "linux", "Linux"); nvram_match("os_name", "vx", "VxWorks"); nvram_match("os_name", "eCos", "eCos"); %> <% kernel_version(); %> <% nvram_get("os_version"); %></td>
  </tr>
  <tr>
  <th width="310">WL驱动版本号:&nbsp;&nbsp;</th>
  <td><% nvram_get("wl_version"); %></td>
  </tr>
  <tr>
  <th width="310">固件版本号:&nbsp;&nbsp;</th>
  <td><% firmware_version(); %></td>
  </tr>
  <tr>
    <th width="310">选择新固件:&nbsp;&nbsp;</th>
    <td><input type="file" name="file"></td>
  </tr>
  <tr>
</table>

<br />
<div id="pbar">
<div style="color:red; font-size:30px">
<b>升级中，请勿断电!!!!!!</b>
</div>
<div id="pbar_outerdiv" style="width: 300px; height: 20px; border: 1px solid grey; z-index: 1; position: relative; border-radius: 5px; -moz-border-radius: 5px;">
<div id="pbar_innerdiv" style="background-color: lightblue; z-index: 2; height: 100%; width: 0%;"></div>
<div id="pbar_innertext" style="z-index: 3; position: absolute; top: 0; left: 0; width: 100%; height: 100%; color: black; font-weight: bold; text-align: center;">0%</div>
</div>

</div>

<div class="table-apply">
<br />
</table>
  <tr>
    <td><input type="submit" id="submit" name="action" value="升级到新的固件"></td>
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
