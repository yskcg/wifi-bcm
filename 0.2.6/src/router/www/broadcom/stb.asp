<!--
Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

$Id: status.asp,v 1.34 2011-01-11 18:43:43 Exp $
-->

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html lang="en">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>LanNet Gateway design: Status to STB</title>
<link rel="stylesheet" type="text/css" href="style.css" media="screen">
<script language="JavaScript" type="text/javascript" src="overlib.js"></script>
</head>

<body>
<div id="overDiv" style="position:absolute; visibility:hidden; z-index:1000;"></div>

<table border="0" cellpadding="0" cellspacing="0" width="100%">
  <tr>
    <td colspan="2" align="center" ><img border="0" src="gxlogo.gif" alt=""></td>
  </tr>
</table>

<table border="0" cellpadding="0" cellspacing="0" width="100%" bgcolor="#000000">
  <% asp_list(); %>
</table>

<p>
<table border="0" cellpadding="0" cellspacing="0">
  <tr>
    <th width="310"
	onMouseOver="return overlib('Shows the system up time since the router was booted.', LEFT);"
	onMouseOut="return nd();">
	System Up Time:&nbsp;&nbsp;
    </th>
    <td>&nbsp;&nbsp;</td>
    <td><% sysuptime(); %></td>
  </tr>
</table>

<p>
<table border="0" cellpadding="0" cellspacing="0">
  <tr>
    <th width="310" valign="top"
	onMouseOver="return overlib('Shows a log of recent connection attempts.', LEFT);"
	onMouseOut="return nd();">
	Connection Log:&nbsp;&nbsp;
    </th>
    <td>&nbsp;&nbsp;</td>
    <td><% dumplog(); %></td>
  </tr>
</table>

<p class="label">&#169;2016-2017 LanNet Corporation. All rights reserved.</p>

</body>
</html>
