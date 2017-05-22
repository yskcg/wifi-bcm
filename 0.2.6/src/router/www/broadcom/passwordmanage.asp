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
<body>
<form method="post" action="stbapply.cgi">
<input type="hidden" name="page" value="passwordmanage.asp">
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
          <td>登陆用户名：</td>
          <td><input name="http_username" value="<% nvram_get("http_username"); %>"</td>
        </tr>
        <tr>
        <td>登陆密码:</td>
	<td> <input name="http_passwd" value="<% nvram_get("http_passwd"); %>" type="password"></td>
        </tr>
        <tr>
          <td width="120"></td>
          <td>
            <input type="submit" name="action" value="应用">
          </td>
          <td>
            <input type="reset" name="action" value="取消">
          </td>
        </tr>
      </table>
    </div>
  </div>
</form>
</body>
</html>
