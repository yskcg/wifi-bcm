<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>无线终端列表</title>
  <link rel="stylesheet" href="stylewireless.css">
</head>
<body>
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
      <div class="cont-net">
        <table>
          <th colspan="6">无线终端列表</td>
	  <tr align="center">
	    <td width="100px">MAC地址</td>
	    <td>上线时间</td>
	    <td>认证状态</td>
	    <td>WMM状态</td>
	    <td>节能状态</td>
	    <td>APSD状态</td>
	  </tr>
	  <% wl_auth_list(); %>
	</table>
        <table>
            <th colspan="6">DHCP 终端列表</th>
	    <tr align="center">
		<td class="label" style="width:auto">主机名</td>
		<td class="label" style="width:auto">MAC地址</td>
		<td class="label" style="width:auto">IP地址</td>
		<td class="label" style="width:auto">剩余租约时间</td>
		<td class="label" style="width:auto">网络描述</td>
	    </tr>
    	<% lan_leases(); %>
	</table>
      </div>
    </div>
  </div>
</body>
</html>
