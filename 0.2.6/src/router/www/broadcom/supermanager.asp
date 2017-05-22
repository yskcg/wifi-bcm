<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>超级管理页面</title>
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
      <table class="cont-high-table">
        <tr>
          <td width="200">网络接入模式</td>
          <td><select name="" id="">
            <option value="">广电宽带网络</option>
            <option value="">广电移动网络</option>
          </select></td>
        </tr>
        <tr>
          <td width="200">SSID1名称前缀</td>
          <td><select name="" id="">
            <option value="">不允许修改</option>
            <option value="">允许修改</option>
          </select></td>
        </tr>
        <tr>
          <td width="200">SSID2名称修改</td>
          <td><select name="" id="">
            <option value="">HiTV</option>
            <option value="">HiTV</option>
          </select></td>
        </tr>
	<tr>
          <td width="200">WAN Option60</td>
          <td> <input type="text" value="" placeholder="gxcatv.nat.v1.0.0"></td>
        </tr>
        <tr>
          <td width="200"></td>
          <td>
            <button>应用</button>
            <button>取消</button>
          </td>
        </tr>
      </table>
    </div>
  </div>
</body>
</html>
