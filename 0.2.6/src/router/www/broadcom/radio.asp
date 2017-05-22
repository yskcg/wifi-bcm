<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="zh-Hans">
<head>
<title>Gateway design: Radio</title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
<script language="JavaScript" type="text/javascript" src="jquery.min.js"></script>
<script language="JavaScript" type="text/javascript" src="lw.js"></script>
<script language="JavaScript" type="text/javascript">

function wl_country_list_change(nphy_set)
{
	var phytype;
	var band;
	var cur = 0;
	var sel = 0;

	if (nphy_set == "1") {
		phytype = "n";
		band  = "2";
	}

	/* Save current country */
	for (i = 0; i < document.forms[0].wl_country_code.length; i++) {
		if (document.forms[0].wl_country_code[i].selected) {
			cur = document.forms[0].wl_country_code[i].value;
			break;
		}
	}

	if (phytype == "a") {
		<% wl_country_list("a"); %>
	} else if (phytype == "n") {
		if (band == "1") {
			<% wl_country_list("n", 1); %>
		} else if (band == "2") {
			<% wl_country_list("n", 2); %>
		}
	} else {
		<% wl_country_list("b"); %>
	}

	/* Reconstruct country_code array from new countries */
	document.forms[0].wl_country_code.length = countries.length;
	for (var i in countries) {
		document.forms[0].wl_country_code[i].value = countries[i];
		if (countries[i] == cur) {
			document.forms[0].wl_country_code[i].selected = true;
			sel = 1;
		}
        else{
			document.forms[0].wl_country_code[i].disabled = true;
        }
	}

	if (sel == 0)
		document.forms[0].wl_country_code[0].selected = true;
}

function wl_channel_list_change(nphy_set)
{
	var phytype;
	var band;
	var nbw_cap;
	var nctrlsb;
	var country = document.forms[0].wl_country_code[document.forms[0].wl_country_code.selectedIndex].value;
	var channels = new Array(0);
	var cur = 0;
	var sel = 0;
	var nmode;

	if (nphy_set == "1") {
		phytype = "n";
		band  = "2";
		nctrlsb = document.forms[0].wl_nctrlsb[document.forms[0].wl_nctrlsb.selectedIndex].value;

		nmode = document.forms[0].wl_nmode[document.forms[0].wl_nmode.selectedIndex].value;
		if (nmode == "0") {
			document.forms[0].wl_nbw_cap.selectedIndex = 0;
			document.forms[0].wl_nbw_cap.disabled = 1;
		}
		nbw_cap = document.forms[0].wl_nbw_cap[document.forms[0].wl_nbw_cap.selectedIndex].value;

	}
	

	/* Save current channel */
	for (i = 0; i < document.forms[0].wl_channel.length; i++) {
		if (document.forms[0].wl_channel[i].selected) {
			cur = document.forms[0].wl_channel[i].value;
			break;
		}
	}

	if (phytype == "a") {
		<% wl_channel_list("a"); %>
	} else if (phytype == "n") {
		/* 5 Ghz == a */
		if (band == "1") {
			var nsb = document.forms[0].wl_nctrlsb[document.forms[0].wl_nctrlsb.selectedIndex].value;
			if ((nbw_cap == "1") || (nbw_cap == "2")) {
				if (nsb == "upper") {
					<% wl_channel_list("n", 1, 40, "upper"); %>
				} else {
					<% wl_channel_list("n", 1, 40, "lower"); %>
				}
			} else if (nbw_cap == "0") {
				<% wl_channel_list("n", 1, 20); %>
			}
		} else if (band == "2")	 {
			var nsb = document.forms[0].wl_nctrlsb[document.forms[0].wl_nctrlsb.selectedIndex].value;
			if (nbw_cap == "1") {
				if (nsb == "upper") {
					<% wl_channel_list("n", 2, 40, "upper"); %>
				} else {
					<% wl_channel_list("n", 2, 40, "lower"); %>
				}
			} else if ((nbw_cap == "0") || (nbw_cap == "2")) {
				<% wl_channel_list("n", 2, 20); %>
			}
		}
	} else {
		<% wl_channel_list("b"); %>
	}

	/* Reconstruct channel array from new channels */
	document.forms[0].wl_channel.length = channels.length;
	for (var i in channels) {
		if (channels[i] == 0)
			document.forms[0].wl_channel[i] = new Option("Auto", channels[i]);
		else
			document.forms[0].wl_channel[i] = new Option(channels[i], channels[i]);
		document.forms[0].wl_channel[i].value = channels[i];
		if (channels[i] == cur) {
			document.forms[0].wl_channel[i].selected = true;
			sel = 1;
		}
	}

	if (sel == 0 && document.forms[0].wl_channel.length > 0)
		document.forms[0].wl_channel[0].selected = true;
}

function wl_ewc_options(nphy_set)
{
	var bw;
	var nbw_cap;
	var band;
	var channel;
    var nmode;

	if (nphy_set == "0")
		return;

	if (document.forms[0].wl_channel.length > 0)
		channel = document.forms[0].wl_channel[document.forms[0].wl_channel.selectedIndex].value;
	else
		channel = 0;
	nmode = document.forms[0].wl_nmode[document.forms[0].wl_nmode.selectedIndex].value;
	if (channel == "0" && nmode != "0") {
		document.forms[0].wl_nctrlsb.disabled = 1;
		<% wl_nphyrates("0"); %>
        $("#wl_nmcsidx option[value='-2']").remove();
		return;
	}

	document.forms[0].wl_channel.disabled = 0;
	if (document.forms[0].wl_nphyrate != null)
		document.forms[0].wl_nphyrate.disabled = 0;
	document.forms[0].wl_nctrlsb.disabled = 0;
	document.forms[0].wl_nbw_cap.disabled = 0;

/*
*/
	/* If nmode is disabled, allow only 20Mhz selection */
    if (nmode == "0") {
		document.forms[0].wl_nbw_cap.selectedIndex = 0;
		document.forms[0].wl_nbw_cap.disabled = 1;
	}

	band = document.forms[0].wl_nband[document.forms[0].wl_nband.selectedIndex].value;
	nbw_cap = document.forms[0].wl_nbw_cap[document.forms[0].wl_nbw_cap.selectedIndex].value;

	if (((band == "1") && ((nbw_cap == "1") || (nbw_cap == "2"))) || 
	    ((band == "2") && (nbw_cap == "1"))) 
		bw = "40";
	else
		bw = "20";

	/* Control sb is allowed only for 40MHz BW Channels */	
	if (bw == "40") {
		document.forms[0].wl_nctrlsb.disabled = 0;
		<% wl_nphyrates("40"); %>
	} else if (bw == "20") {
		document.forms[0].wl_nctrlsb.selectedIndex = 0;
		document.forms[0].wl_nctrlsb.disabled = 1;
		<% wl_nphyrates("20"); %>
	}

    $("#wl_nmcsidx option[value='-2']").remove();
	if (nmode == "0") {
		document.forms[0].wl_nreqd.disabled = 1;
		document.forms[0].wl_nmcsidx.disabled = 1;
	} else {
		document.forms[0].wl_nreqd.disabled = 0;
		document.forms[0].wl_nmcsidx.disabled = 0;
	}
}

function wl_mode_onchange()
{
	//The following variable is set when the user changes wl_mode
	document.forms[0].wl_mode_changed = 1;
	//wl_mode_change();
}

function wl_nmode_change()
{
<% wl_nphy_comment_beg(); %>
	var crypto = "<% wl_crypto(); %>";
	var wep = "<% wl_wep(); %>";
	var i, cur, options;

	/* Save current selected nmode option */
	cur = document.forms[0].wl_nmode[document.forms[0].wl_nmode.selectedIndex].value;

	if (crypto != "tkip" && wep != "enabled") {
		document.forms[0].wl_nmode.length = 2;
		document.forms[0].wl_nmode[0] = new Option("自动", "-1");
		document.forms[0].wl_nmode[0].value = "-1";
		if (document.forms[0].wl_nmode[0].value == cur)
			document.forms[0].wl_nmode[0].selected = true;
		document.forms[0].wl_nmode[1] = new Option("关闭", "0");
		document.forms[0].wl_nmode[1].value = "0";
		if (document.forms[0].wl_nmode[1].value == cur)
			document.forms[0].wl_nmode[1].selected = true;
	} else {
		document.forms[0].wl_nmode.length = 1;
		document.forms[0].wl_nmode[0] = new Option("关闭", "0");
		document.forms[0].wl_nmode[0].value = "0";
		if (document.forms[0].wl_nmode[0].value == cur)
			document.forms[0].wl_nmode[0].selected = true;
	}
<% wl_nphy_comment_end(); %>
}

function wl_recalc()
{
 	var nphy_set = "<% wl_nphy_set(); %>";

	if (nphy_set != "1")
		wl_phytype_change();
	wl_country_list_change(nphy_set);
	wl_channel_list_change(nphy_set);
	wl_nmode_change();
	wl_ewc_options(nphy_set);

    $("#wl_txchain option[value=" + 1 +"]").attr('disabled','disabled');
    $("#wl_rxchain option[value=" + 1 +"]").attr('disabled','disabled');
    $("#wl_wme_no_ack option[value=" + "on" +"]").attr('disabled','disabled');
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
<input type=hidden name="wl_mode_changed" value=0>
<input type=hidden name="wl_ure_changed" value=0>

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
            <li><a class="active" href="radio.asp">射频配置</a></li>
            <li><a href="ssid.asp">SSID配置</a></li>
            <li><a href="security.asp">安全配置</a></li>
            <li><a href="pwps.asp">WPS</a></li>
            <li><a href="wds.asp">WDS</a></li>
        </ul>
        </li>
        <li><a href="firmware.asp">固件升级</a></li>
    </ul>
</nav>

<form method="post" action="radio.asp">
<input type="hidden" name="page" value="radio.asp">

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
    <th width="310">工作模式:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_mode" onChange="wl_mode_onchange();">
	<% wl_mode_list();%>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">国家地区:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_country_code" onChange="wl_recalc();">
	  <option value="<% wl_cur_country(); %>" selected></option>
	</select>
	&nbsp;&nbsp;当前: <% wl_cur_country(); %>
    </td>
  </tr>
  <tr>
    <th width="310">启用:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_radio">
	  <option value="0" <% nvram_match("wl_radio", "0", "selected"); %>>关闭</option>
	  <option value="1" <% nvram_match("wl_radio", "1", "selected"); %>>启用</option>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">工作频段:&nbsp;&nbsp;</th>
    <td>
	<select name=<% wl_phytype_name(); %>  onChange="wl_recalc();">
	  <% wl_phytypes(); %>
	</select>
	&nbsp;&nbsp;<% wl_cur_phytype(); %>
    </td>
  </tr>
  <tr>
    <th width="310">控制信道:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_channel" onChange="wl_recalc();">
	  <option value="<% nvram_get("wl_channel"); %>" selected></option>
	</select>
	&nbsp;&nbsp;<% wl_cur_channel(); %>
    </td>
  </tr>
<% wl_nphy_comment_beg(); %>
  <tr>
    <th width="310">802.11 n-mode:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_nmode" onChange="wl_recalc();">
	  <option value="-1" <% nvram_match("wl_nmode", "-1", "selected"); %>>自动</option>
	  <option value="0" <% nvram_match("wl_nmode", "0", "selected"); %>>关闭</option>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">使用频段带宽:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_nbw_cap" onChange="wl_recalc();">
	  <option value="0" <% nvram_match("wl_nbw_cap", "0", "selected"); %>>20 MHz</option>
	  <option value="1" <% nvram_match("wl_nbw_cap", "1", "selected"); %>>40 MHz</option>
	</select>
	&nbsp;&nbsp;<% wl_cur_nbw(); %>
    </td>
  </tr>
  <tr>
    <th width="310">控制信道上下边带(40Mhz)</th>
    <td id="wl_nctrlsb_td">
	<select name="wl_nctrlsb" onChange="wl_recalc();">
	  <option value="lower" <% nvram_match("wl_nctrlsb", "lower", "selected"); %>>下边带</option>
	  <option value="upper" <% nvram_match("wl_nctrlsb", "upper", "selected"); %>>上边带</option>
	</select>
	&nbsp;&nbsp;<% wl_cur_nctrlsb(); %>
    </td>
  </tr>
  <tr>
    <th width="310">11N速率:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_nmcsidx" id="wl_nmcsidx">
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">11N发送天线数量:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_txchain" id="wl_txchain" onChange="wl_recalc();">
        <% wl_txchains_list();%>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">11N接受天线数量:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_rxchain" id="wl_rxchain" onChange="wl_recalc();">
        <% wl_rxchains_list();%>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">仅接入11N设备:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_nreqd">
	  <option value="1" <% nvram_match("wl_nreqd", "1", "selected"); %>>启用</option>
	  <option value="0" <% nvram_match("wl_nreqd", "0", "selected"); %>>关闭</option>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">支持WMM:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_wme" onChange="wl_recalc();">
	  <option value="auto" <% nvram_match("wl_wme", "auto", "selected"); %>>自动</option>
	  <option value="off" <% nvram_match("wl_wme", "off", "selected"); %>>关闭</option>
	  <option value="on" <% nvram_match("wl_wme", "on", "selected"); %>>启用</option>
	</select>
    </td>
  </tr>
  <tr>
    <th width="310">No-Ack支持:&nbsp;&nbsp;</th>
    <td>
	<select name="wl_wme_no_ack" id="wl_wme_no_ack">
	  <option value="off" <% nvram_match("wl_wme_no_ack", "off", "selected"); %>>关闭</option>
	  <option value="on" <% nvram_match("wl_wme_no_ack", "on", "selected"); %>>启用</option>
	</select>
    </td>
  </tr>
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
