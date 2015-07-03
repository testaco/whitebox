/*
 * Copyright (C) 2015 Algoram. Dual-licensed: Affero GPL 3 is the default,
 * and a commercial license is available for sale. Contact Bruce Perens K6BP
 * via email to bruce@perens.com to commercially license this program for your
 * device.
 *
 * Web radio front panel, by Bruce Perens K6BP.
 */
'use strict';


function emit(name, i)
{
  if ( i[name] ) {
    return "<dt>" + name + "</dt><dd><b>" + i[name] + "</b></dd>";
  }
  else {
    return "";
  }
}

// Entry point to this script.
function go(foo) {
  var s = "<dl>";
  var i = window.client_info;
  s += emit("certificate_is_valid", i);
  s += emit("callsign", i);
  s += emit("name", i);
  s += emit("email", i);
  s += emit("hostname", i);
  s += emit("ip_address", i);
  s += emit("user_agent", i);
  s += "</dl>";
  window.noticeBox = document.getElementById('Notice');
  notice(s);
}

function notice(message) {
  if (window.currentNotice != message) {
    window.currentNotice = message;
    window.noticeBox.innerHTML = message;
  }
}
