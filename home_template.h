



// Template for page end
PROGMEM const char HOMEPAGE[] = R"=====(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate" />
  <meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"/>
  <link rel="shortcut icon" href="#" />
  <title>{title}</title>                        
</head> 
<style>
.card {
  border: 1px solid #cbcaca;
  box-shadow: 0 2px 4px 0 rgba(0, 0, 0, 0.2);
  padding: 10px;
  text-align: center;
  background-color: #ffffff;
  margin-bottom: 8px;
  border-radius: 5px;
}
</style>
<body>
<div class="container">
  <div class="row">
    <div class="column">
      <div class="card">
        <h2>{host_name}</h2>
      </div>
    
      <div class="card">
        Frames perdues : {totalLost}&nbsp;/&nbsp;{totalReceived}&nbsp;({percentLost}%)
      </div> <!-- card -->

      <div class="card">
        <button type="button" title="Config" onClick="window.location.href = '/cfg'" name="">Config</button>
        <button type="button" title="Reboot device" onClick="if(!confirm('Reboot ?')) return false;" name="_RBT">Reboot</button>
      </div> <!-- card -->
    </div> <!-- column -->
  </br>
  <center><small id="msg" style="color: lightgray;">ArtnetNode<br/>({scriptVersion})</small></center>
  </br>
  </div> <!-- row -->
</div> <!-- container -->
</body>
</html>
)=====";


void sendHomePage(WEB_SERVER * server, String hostName, int totalShown, int totalLost) {
  String out = String(HOMEPAGE);
  out.replace("{title}",hostName);
  out.replace("{host_name}",hostName);
  out.replace("{totalLost}",String(totalLost));
  out.replace("{totalReceived}",String(totalLost+totalShown));
  out.replace("{percentLost}", (totalLost + totalShown > 0) ? String(totalLost*100/(totalLost+totalShown)) : "--");
  out.replace("{scriptVersion}",SCRIPT_VERSION);
  server->send(200,"text/html",out);
}







