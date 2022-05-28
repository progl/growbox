// h += "<form action='' method='POST' id='set'></form>\n";
// h += "<table><caption>Root pomp</caption>\n";

if (server.arg("RootPomp") != "")
  preferences.putInt("RootPomp", server.arg("RootPomp").toInt());

if (server.arg("SelectedRootPomp") != "")
  preferences.putString("SelectedRootPomp", server.arg("SelectedRootPomp"));

if (server.arg("RootPwdMax") != "")  preferences.putInt("RootPwdMax", server.arg("RootPwdMax").toInt());
RootPwdMax=preferences.getInt("RootPwdMax", 254);

if (server.arg("RootPwdMin") != "")   preferences.putInt("RootPwdMin", server.arg("RootPwdMin").toInt());
RootPwdMin=preferences.getInt("RootPwdMin", 0);

// h += "<tr><td>RootPomp<td><input type='text' name='RootPomp' value='" + fFTS(preferences.getInt("RootPomp", 0), 0) + "' form='set'></tr>\n";
// h += "</table>\n";
// h += "<input type='submit' value='save' form='set'/></form>\n";

String RootPomp=fFTS(preferences.getInt("RootPomp", 0), 0);
String SelectedRootPomp=preferences.getString("SelectedRootPomp", "DRV1_A");

h +="<form action='' method='POST' id='set'></form>\n";
h +=" <table>\n";
h +="     <caption>Root pomp</caption>\n";

h += "<tr><td>AirHum<td>" + fFTS(AirHum,3) + "%</tr>\n";
h += "<tr><td>RootTemp<td>" + fFTS(RootTemp,3) + "</tr>\n";
h += "<tr><td>AirTemp<td>" + fFTS(AirTemp,3) + "</tr>\n";
h += "<tr><td>AirTemp-RootTemp<td>" + fFTS(AirTemp-RootTemp,3) + "</tr>\n";

h +="     <tr>\n";
h +="         <td>Selected Root Pomp\n";
h +="        <td><input type='text' name='SelectedRootPomp' value='"+ SelectedRootPomp +"' form='set'>\n";
h +="     </tr>\n";

h +="     <tr>\n";
h +="         <td>Autocontrol Enable\n";
h +="         <td><input type='text' name='RootPomp' value='" + RootPomp + "' form='set'>\n";
h +="     </tr>\n";

// Максимальный уровень шим
h +="     <tr>\n";
h +="         <td>RootPwdMax\n";
h +="         <td><input type='text' name='RootPwdMax' value='" + String(RootPwdMax) + "' form='set'>\n";
h +="     </tr>\n";

// Минимальный уровень шим
h +="     <tr>\n";
h +="         <td>RootPwdMin\n";
h +="         <td><input type='text' name='RootPwdMin' value='" + String(RootPwdMin) + "' form='set'>\n";
h +="     </tr>\n";


h +=" </table>\n";
h +=" <input type='submit' value='save' form='set' /></form>\n";
h +=" <form action='' method='POST' id='set'></form>\n";