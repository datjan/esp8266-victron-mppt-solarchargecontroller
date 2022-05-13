const char index_page[] PROGMEM = R"=====(
<!DOCTYPE HTML>
<HTML>
  <HEAD>
    <META name='viewport' content='width=device-width, initial-scale=1'>
    <TITLE>from Datjan with love...</TITLE>
    <SCRIPT>
    var xmlHttp01=createXmlHttpObject();

    function createXmlHttpObject(){
     if(window.XMLHttpRequest){
        xmlHttp=new XMLHttpRequest();
     }else{
        xmlHttp=new ActiveXObject('Microsoft.XMLHTTP');
     }
     return xmlHttp;
    }

    function process(){
      if(xmlHttp01.readyState==0 || xmlHttp01.readyState==4){
        xmlHttp01.open('GET','rest',true);
        xmlHttp01.onreadystatechange=handleServerResponse;
        xmlHttp01.send(null);
      }
      setTimeout('process()',2000);
    }

    function handleServerResponse(){
     if(xmlHttp01.readyState==4 && xmlHttp01.status==200){
      const json_obj = JSON.parse(xmlHttp01.response);
      
      var result_text = "<table>";
      for (var i = 0; i < json_obj.length; i++){
        var obj = json_obj[i];
        result_text = result_text + "<tr><td style=\"text-align: right;\">" + obj.id + " | </td><td style=\"text-align: left;\">" + obj.value + "</td></tr>";
      }
      result_text = result_text + "</table>";

      document.getElementById('response_items').innerHTML=result_text;
     }
    }
    </SCRIPT>
    <STYLE>
      h1 {
        font-size: 120%;
        color: blue;
        margin: 0 0 10px 0;
      }
       table{
        border-collapse: collapse;
      }     
      table, th, td {
        border: 0px solid blue;
      }
    </STYLE>
  </HEAD>
  <BODY onload='process()'>
    <CENTER>
    <br><br>
    <span style="font-family: Arial, Helvetica, sans-serif;font-size: 20px;font-weight: bold;color: #a5a5a5;"><A id='response_items'></A></span> 

    </CENTER>
  </BODY>
</HTML>
)=====";
