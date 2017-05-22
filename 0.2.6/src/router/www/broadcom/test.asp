<html>
<head>
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script src="jquery.min.js"></script>
<script src="lw.js"></script>
</head>

<body>

<br />
<nav>
    <ul>
        <li><a href="index.asp">Basic</a></li>
        <li><a href="lan.asp">LAN</a></li>
        <li><a href="wan.asp">WAN</a></li>
        <li><a  data-toggle="#list" href="#">Wireless</a>
        <ul class="in" id="list" style="display:block">
            <li><a href="radio.asp">Radio</a></li>
            <li><a href="ssid.asp">SSID</a></li>
            <li><a class="active" href="security.asp">Security</a></li>
        </ul>
        <li><a href="firmware.asp">Firmware</a></li>
    </ul>
</nav>

<form method="post" action="test.asp">
<div class="table-conf">
<table>
  <tr>
    <th width="310">
	Wireless Interface:&nbsp;&nbsp;
    </th>
    <td>
    test
    </td>
  </tr>
</table>
</div>
</form>

<footer>&#169;2016-2017 LanNet Corporation. All rights reserved.</footer>

</body>
</html>
