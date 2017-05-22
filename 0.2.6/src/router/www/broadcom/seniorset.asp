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
  $(document).ready(function(){
	$("input[name='action']").click(function(){
		var actionpair = [
			['重启无线路由','Reboot'],
			['恢复出厂默认','Restore']
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
<input type="hidden" name="page" value="seniorset.asp">
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
      <ul class="cont-high">
        <li><a href ="wirelessseniorsettings.asp">无线高级设置</a></li>
	<li><a href ="wirelessbrigle.asp">无线桥接设置</a></li>
        <li><a href ="wirelessterminallist.asp">连接终端列表</a></li>
        <li><a href ="passwordmanage.asp">登录密码管理</a></li>
        <li><input type="submit" name="action" value="恢复出厂默认"></li>
        <li><input type="submit" name="action" value="重启无线路由"></li>
      </ul>
    </div>
  </div>
</form>
</body>
</html>
