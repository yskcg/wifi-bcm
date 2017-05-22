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

function wl_switch()
{
	if(document.forms[0].wl_radio.value==0){
	  document.forms[0].wl_wpa_psk.disabled=true;
	  document.forms[0].wl_ssid_tailer.disabled=true;
	  document.forms[0].wl_auth.disabled=true;
	  document.forms[0].wl_auth_select.disabled=true;
	} else {
	   document.forms[0].wl_wpa_psk.disabled=false;
	  document.forms[0].wl_ssid_tailer.disabled=false;
	  document.forms[0].wl_auth.disabled=false;
          document.forms[0].wl_auth_select.disabled=false;
	}
	wl_auth_event();
}

function wl_auth_event()
{
	if(document.forms[0].wl_auth_select.value==0){
	  document.forms[0].wl_wpa_psk.disabled=true;
	  document.forms[0].wl_wep.value="disabled";
	  document.forms[0].wl_akm_psk2.value="disabled";
	  document.forms[0].wl_akm_psk.value="disabled";
	} else {
          if(document.forms[0].wl_radio.value==1)
	  document.forms[0].wl_wpa_psk.disabled=false;
	  document.forms[0].wl_wep.value="disabled";
	  document.forms[0].wl_akm_psk2.value="enabled";
	  document.forms[0].wl_akm_psk.value="disabled";
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
});

function load() {
	$("#wl_ssid_tailer").change(function() {
        var wl_value = document.forms[0].wl_radio.value;
        if(wl_value == "1"){
          var wl0_ssid = $("#wl_ssid_td").text().concat($("#wl_ssid_tailer").val());
          $("#wl_ssid").val(wl0_ssid);
        }
        else if(wl_value == "0"){
            $("#wl_ssid").val($("#wl_ssid_td").text());
        }
  	});
	var wl0_ssid_header = "<% nvram_get("wl0_ssidheader"); %>";
        var wl0_ssid = "<% nvram_get("wl0_ssid"); %>";
	var wl0_ssidheder_len = wl0_ssid_header.length;
	var wl0_tailer = wl0_ssid.substr(wl0_ssidheder_len);
	$("#wl_ssid_tailer").val(wl0_tailer);
        $("#wl_ssid").val(wl0_ssid);
        wl_switch();
	/*var strs=new Array();
        var wl0_ssid,temp_ssid="";
	var wl0_ssid_header = "<% nvram_get("wl0_ssidheader"); %>";
	 $("#wl_ssid_tailer").change(function() {
        var wl_value = document.forms[0].wl_radio.value;
        if(wl_value == "1"){
          wl0_ssid = wl0_ssid_header.concat($("#wl_ssid_tailer").val());
          $("#wl_ssid").val(wl0_ssid);
        }
        else if(wl_value == "0"){
            $("#wl_ssid").val($("#wl_ssid_tailer").val());
        }
  	});
        wl0_ssid = "<% nvram_get("wl0_ssid"); %>";
        var wl0_ssidheder_len = wl0_ssid_header.length;
	var wl0_tailer = wl0_ssid.substr(wl0_ssidheder_len);
	strs=wl0_tailer.split(":");
        for(i=0;i<strs.length && strs;i++)
	{
		temp_ssid=temp_ssid.concat(strs[i]);
	}
        wl0_ssid=wl0_ssid_header.concat(temp_ssid);
        $("#wl_ssid_tailer").val(wl0_ssid);
        $("#wl_ssid").val(wl0_ssid);
        wl_switch();*/
}

//-->
</script>
</head>
<body onLoad="load();">
  <form method="post" action="basicset.asp">
  <input type="hidden" name="page" value="basicset.asp">
  <input type="hidden" name="wl_ssid" id="wl_ssid" value="HiTV_01:02:03">
  <input type="hidden" name="wl_akm_psk2" value="disabled">
  <input type="hidden" name="wl_wep" value="disabled">
  <input type="hidden" name="wl_akm_psk" value="disabled">
  <input type="hidden" name="wl_crypto" value="tkip+aes">
  <input type="hidden" name="wl_akm">
  <input type="hidden" name="wl_auth" value="0">
  <input type="hidden" name="wl_unit" value="0">
  <div class="main" style="height:85%">
    <div class="aside">
      <ul>
        <li class="check"><a href ="basicset.asp">基本设置</a></li>
        <li><a href ="seniorset.asp">高级设置</a></li>
        <li><a href ="wireless.asp">无线扩展</a></li>
        <li><a href ="netstatus.asp">网络状态</a></li>
      </ul>
    </div>
    <div class="cont">
      <table class="cont-base">
        <tr>
          <td width="120">无线开关</td>
          <td>
	   <select name="wl_radio" onChange="wl_switch();">
	  <option value="0" <% nvram_match("wl_radio", "0", "selected"); %>>关闭</option>
	  <option value="1" <% nvram_match("wl_radio", "1", "selected"); %>>启用</option>
	  </select>
         </td>
        </tr>
    <tr>
        <td width="120">无线名称</td>
        <td id="wl_ssid_td"><% nvram_get("wl0_ssidheader"); %><input name="wl_ssid_tailer" id="wl_ssid_tailer" value="" maxlength="26" type="text" style="width: 136px"></td>
    </tr>  
        <tr>
          <td width="120">加密模式</td>
          <td>
				<select name="wl_auth_select" onChange="wl_auth_event();">
	  			<option value="1" <% nvram_inlist("wl_akm", "psk2", "selected"); %>>加密</option>
	 			<option value="0" <% nvram_invinlist("wl_akm", "psk2", "selected"); %>>不加密</option>
				</select>
    	  </td>
        </tr>
        <tr>
          <td width="120">无线密码</td>
          <td><input name="wl_wpa_psk" value="<% nvram_get("wl_wpa_psk"); %>" size="16">
          </td>
        </tr>
        <tr>
	<td width="120">
	    <input type="submit" name="action" value="应用">
      	</td>
	<td>
	    <input type="reset" name="action" value="取消">
      	</td>
	</tr>
      </table>
    </div>
   <footer>
   <hr />
     56itech&nbsp;&nbsp;硬件版本： V1.0 &nbsp;&nbsp; 软件版本：V1.0.2 
   <hr />
   </footer>
    </form>
  </div>
</body>
</html>
