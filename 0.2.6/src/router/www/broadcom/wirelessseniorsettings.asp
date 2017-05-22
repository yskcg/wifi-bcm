<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>无线高级设置</title>
  <link rel="stylesheet" href="stylewireless.css">
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
	var country = "CN";
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
		document.forms[0].wl_nmcsidx.disabled = 1;
	} else {
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
}

function wl_recalc()
{
 	var nphy_set = "<% wl_nphy_set(); %>";

	if (nphy_set != "1")
		wl_phytype_change();
	wl_channel_list_change(nphy_set);
	wl_nmode_change();
	wl_ewc_options(nphy_set);
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
  <input type=hidden name="wl_mode_changed" value=0>
  <input type=hidden name="wl_ure_changed" value=0>
  <form method="post" action="stbradio.asp">
  <input type="hidden" name="page" value="wirelessseniorsettings.asp">
  <div class="main">
    <div class="aside">
      <ul>
        <li><a href ="basicset.asp">基本设置</a></li>
        <li class="check"><a href ="seniorset.asp">高级设置</a></li>
        <li><a href ="wireless.asp">无线扩展</a></li>
        <li><a href ="netstatus.asp">网络状态</a></li>
      </ul>
    </div>
  <div class="cont">
  <table class="cont-high-table">
  <tr>
    <td width="200">WIFI网卡地址:</td>
    <td>
	<select name="wl_unit" onChange="submit();">
	  <% wl_list(); %>
	</select>
    </td>
  </tr>
  <tr>
    <td width="200">工作模式:</td>
    <td>
	<select name="wl_mode" onChange="wl_mode_onchange();">
	<% wl_mode_list();%>
	</select>
    </td>
  </tr>
  <tr>
    <td width="200">无线广播:</td>
    <td>
	<select name=<% wl_phytype_name(); %>  onChange="wl_recalc();">
	  <% wl_phytypes(); %>
	</select>
    </td>
  </tr>
  <tr>
    <td width="200">信道选择:</td>
    <td>
	<select name="wl_channel" onChange="wl_recalc();">
	  <option value="<% nvram_get("wl_channel"); %>" selected></option>
	</select>
    </td>
  </tr>
  <tr>
    <td width="250">n-mode:</td>
    <td>
	<select name="wl_nmode" onChange="wl_recalc();">
	  <option value="-1" <% nvram_match("wl_nmode", "-1", "selected"); %>>自动</option>
	  <option value="0" <% nvram_match("wl_nmode", "0", "selected"); %>>关闭</option>
	</select>
    </td>
  </tr>
  <tr>
    <td width="200">频宽设置:</td>
    <td>
	<select name="wl_nbw_cap" onChange="wl_recalc();">
	  <option value="0" <% nvram_match("wl_nbw_cap", "0", "selected"); %>>20 MHz</option>
	  <option value="1" <% nvram_match("wl_nbw_cap", "1", "selected"); %>>40 MHz</option>
	</select>
    </td>
  </tr>
  <tr>
    <td width="200">上下边带</td>
    <td id="wl_nctrlsb_td">
	<select name="wl_nctrlsb" onChange="wl_recalc();">
	  <option value="lower" <% nvram_match("wl_nctrlsb", "lower", "selected"); %>>下边带</option>
	  <option value="upper" <% nvram_match("wl_nctrlsb", "upper", "selected"); %>>上边带</option>
	</select>
    </td>
  </tr>
  <tr>
    <td width="200">11N速率:</td>
    <td>
	<select name="wl_nmcsidx" id="wl_nmcsidx">
	</select>
    </td>
  </tr>
        <tr>
          <td width="200"></td>
          <td>
            <input type="submit" name="action" value="应用">
          </td>
          <td>
            <input type="reset" name="action" value="放弃">
          </td>
        </tr>
      </table>
    </div>
  </div>
</form>
</body>
</html>
