<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>无线桥接设置</title>
  <link rel="stylesheet" href="stylewireless.css">
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
  <form method="post" action="stbwds.asp">
  <input type="hidden" name="page" value="wirelessbrigle.asp">
  <input type="hidden" name="wl_unit" value="0">
  <input type="hidden" name="wl_wds" value="1">
  <input type="hidden" name="wl_lazywds" value="1">
  <input type="hidden" name="wl_wds_timeout" value="1">
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
       <table>
  	<tr>
    	<th width="200">桥接AP MAC:&nbsp;&nbsp;</th>
    	<td><input name="wl_wds0" value="<% nvram_list("wl_wds", 0); %>" size="17" maxlength="17"></td>
  	</tr>
  	<tr>
    	<th width="200">桥接状态:&nbsp;&nbsp;</th>
    	<td><% wl_wds_status(0); %></td>
  	</tr>
  	<tr>
      </table>
      <div class="table-apply">
	<br />
	<table>
  	<tr>
        <td width="100"></td>
    	<td><input type="submit" name="action" value="应用"></td>
	<td><input type="reset" name="action" value="放弃"></td>
 	 </tr>
	</table>
	 <div class="table-conf-title">
	注意:WDS只能在非加密模式下使用，请先在页中禁用WPA-PSK/WPA2-PSK
	</div>
      </div>
     </div>
    </div>
  </div>
</form>
</body>
</html>
