/* 
 * ezinstall javascript file
 */
String.prototype.startsWith = function(str)
{ return (this.match("^" + str) == str) }

String.prototype.endsWith = function(str) 
{return (this.match(str+"$")==str)}

var textlen = 0;

function createXMLHttpRequest() {
  try { return new XMLHttpRequest(); } catch (e) { }
  try { return new ActiveXObject("Msxml2.XMLHTTP"); } catch (e) { }
  alert("XMLHttpRequest non supportato");
  return null;
}
function do_ajax(arg1, arg2, url) {
  var http_request = createXMLHttpRequest();

  if (!http_request) {
    alert('Javascript error: no XMLHTTP instance');
    return false;
  }
  var querystring = '?' + arg1 + '+' + arg2;
  http_request.open('GET', url + querystring, false);
  http_request.send(null);
  var div = document.getElementById("from_semaphore");
  var cont = document.getElementById("continue");
  var text = http_request.responseText;
  if (text !== '*') {
    if (text && text.length >= textlen) {
      div.innerHTML = text;
      textlen = text.length;
      cont.disabled = true;
    }
    setTimeout(function(){do_ajax(arg1, arg2, url)},750);
  }
  else {
    cont.disabled = false;
  }
}
