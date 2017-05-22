<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>wifi设置页面</title>
  <link rel="stylesheet" href="stylewireless.css">
  <script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
  <script language="JavaScript" type="text/javascript" src="lw.js"></script>
  <script language="JavaScript" type="text/javascript">
  var count=0;
  var timer;
  function autoRefresh()
  {
	timer=setInterval("doRefresh()", 1 * 1000 );
  }

  function doRefresh()
  {
	if ("<%nvram_match("wl_wps_mode", "enabled","1");%>"=="0") {
		document.forms[0].setup.disabled=true;
		return;
	}
	var value="<% nvram_get("wps_proc_status");%>";
        if(value=="1") {
	document.getElementById('wps_status').innerHTML = "正在配对...";
        document.getElementById('wps_status').innerHTML +=count++;
	} else if(value=="2" || value=="7") {
	document.getElementById('wps_status').innerHTML = "配对成功";
	clearInterval(timer);
	}else if(value=="3") {
	document.getElementById('wps_status').innerHTML = "Fail due to WPS message exchange error!";
	}else if(value=="4") {
	document.getElementById('wps_status').innerHTML = "Fail due to WPS time out!";
	}else if(value=="8") {
	document.getElementById('wps_status').innerHTML = "Fail due to WPS session overlap!";
	}else {
	document.getElementById('wps_status').innerHTML = "未开始";
	} 
        if(count>5) {
	 document.forms[0].submit();
	}
        
  }

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
        document.forms[0].paried.disabled=true;
    }
 }
  function loadwps() {
	if ("<%nvram_match("wl_wps_mode", "enabled","1");%>"=="1") {
		document.forms[0].setup.value="已开启";
		document.forms[0].setup.disabled=true;
	} else{
		document.forms[0].setup.value="开启";
		document.forms[0].setup.disabled=false;
	}
	wps_valid_check();
        autoRefresh();
  }

  function setwps(){
	document.forms[0].wl_wps_mode.value="enabled";
	document.forms[0].submit();
  }
  $(document).ready(function(){
	$("input[name='action']").click(function(){
		var actionpair = [
			['开启','Apply'],
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
<body onLoad="loadwps();">
<form method="post" action="wireless.asp">
<% wps_init(); %>
  <input type="hidden" name="wl_unit" value="0">
  <input type="hidden" name="wps_action" value="AddEnrollee">
  <input name="wps_sta_pin" value="" type="hidden">
  <input type="hidden" name="refresh" value="0">
  <input type="hidden" name="wl_wps_mode" value="">
  <div class="main">
    <div class="aside">
      <ul>
        <li><a href ="basicset.asp">基本设置</a></li>
        <li><a href ="seniorset.asp">高级设置</a></li>
        <li class="check"><a href ="wireless.asp">无线扩展</a></li>
        <li><a href ="netstatus.asp">网络状态</a></li>
      </ul>
    </div>
    <div class="cont">
      <div class="cont-link">
        <h3>WPS配对连接</h3>
        <div class="cont-link-start">
	  <input type="submit" id="setup" name="action" value="开启" onclick="setwps();"> <input id="paried" type="submit" name="action" value="开始配对">
          <p>配对状态：<span><label id="wps_status">未启动</label></span></p>
        </div>
        <h4>操作步骤</h4>
        <div class="cont-link-procedure">
          <ul>
	    <li>第一步：点击开启按钮，打开WPS配对功能</li>
            <li>第二步：点击开始配对钮</li>
            <li>第三步：打开手机或者其他设备的wps连接</li>
	    <li style="color: #f00">  
               <div id="closed_network">
		注意:WPS仅能在SSID广播启用的情况下使用，请先在页中启用SSID广播
	       </div>
	       <div id="sharedauth_meth">
		注意:WPS仅能在认证方式为开放的情况下使用，请先在页中选择802.11认证方式为开放模式
		</div>
		<div id="psk2">
		注意:WPS仅能在启用wpa2-psk的情况下使用，请先在页中启用wpa2-psk
		</div>
		<div id="aes">
		注意:WPS仅能在加密方式为aes或tkip+aes的情况下使用，请先在页中选择WPA加密为aes或者tkip+aes
		</div>
		<div id="wep">
		注意:WPS仅能在wep加密关闭的情况下使用，请先在页中禁用wep加密
		</div>
	    </li>
          </ul>
        </div>
      </div>
    </div>
  </div>
</form>
</body>
</html>
