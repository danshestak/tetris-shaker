const char html_index[] PROGMEM = R"(
<h3>Your Score</h3>
<table cellpadding=4>
  <tr><td>Score:</td><td><label id='lbl_sco'></label></td></tr>
</table>
<script>
function w(s) {document.write(s);}
function id(s){return document.getElementById(s);}
function loadValues() {
  var xhr=new XMLHttpRequest();
  xhr.onreadystatechange=function() {
    if(xhr.readyState==4 && xhr.status==200) {
      var jd=JSON.parse(xhr.responseText);
      id('lbl_sco').innerHTML=jd.score;
    }
  };
  xhr.open('GET','js',true);
  xhr.send();
}
setInterval(loadValues, 500);
</script>
)";