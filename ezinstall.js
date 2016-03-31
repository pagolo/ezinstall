/* 
 * ezinstall javascript file
 */
String.prototype.startsWith = function(str)
{ return (this.match("^" + str) == str) }

String.prototype.endsWith = function(str) 
{return (this.match(str+"$")==str)}

var textlen = 0;
var file_sent = "file sent";
var with_7zip = false;
var parseXml;

if (typeof window.DOMParser != "undefined") {
    parseXml = function(xmlStr) {
        return ( new window.DOMParser() ).parseFromString(xmlStr, "text/xml");
    };
} else if (typeof window.ActiveXObject != "undefined" &&
       new window.ActiveXObject("Microsoft.XMLDOM")) {
    parseXml = function(xmlStr) {
        var xmlDoc = new window.ActiveXObject("Microsoft.XMLDOM");
        xmlDoc.async = "false";
        xmlDoc.loadXML(xmlStr);
        return xmlDoc;
    };
} else {
    throw new Error("No XML parser found");
}

function createXMLHttpRequest() {
  try { return new XMLHttpRequest(); } catch (e) { }
  try { return new ActiveXObject("Msxml2.XMLHTTP"); } catch (e) { }
  alert("XMLHttpRequest non supportato");
  return null;
}
function do_ajax(arg1, arg2, url, timestamp) {
  var http_request = createXMLHttpRequest();

  if (!http_request) {
    alert('Javascript error: no XMLHTTP instance');
    return false;
  }
  var querystring = '?' + arg1 + '+' + arg2 + '+' + timestamp;
  http_request.open('GET', url + querystring, false);
  http_request.send(null);
  var div = document.getElementById("from_semaphore");
  var cont = document.getElementById("continue");
  var text = http_request.responseText;
  if (text !== '*') {
    if (text && text.length >= textlen - 10) {
      div.innerHTML = text;
      textlen = text.length;
      cont.disabled = true;
    }
    if (text.indexOf('end of ajax generated html') == -1)
      setTimeout(function(){do_ajax(arg1, arg2, url, new Date().getTime())},750);
    else
      cont.disabled = false;
  }
  else {
    cont.disabled = false;
  }
}

/***********************************
 * Ajax files upload               *
 * *********************************/

function do_ajax_upload_zip(url, filename) {
  var http_request = createXMLHttpRequest();
  if (!http_request) {
    alert('Javascript error: no XMLHTTP instance');
    return false;
  }
  var progress = document.querySelector('.percent');
  progress.style.width = '0%';
  progress.textContent = '0%';
  document.getElementById('progress_bar').className = 'loading';

  var data = new FormData(document.getElementById('upload_form')); //document.getElementById('zip').getFormData();
  http_request.upload.addEventListener('progress', function(e){
    var percentLoaded = Math.round((e.loaded/e.total) * 100);
    if (percentLoaded > 100) percentLoaded = 100;
    progress.style.width = percentLoaded + '%';
    progress.textContent = percentLoaded + '%';
  }, false);
  http_request.onreadystatechange = function() {
    if (http_request.readyState != 4) return;
    if (http_request.status == 200 && http_request.responseText == 'ok') {
      var zipfile = document.getElementsByName('zipfile');
      var ini = document.getElementById('ini');
      var zip = document.getElementById('zip');
      zipfile[0].value = filename;
      zip.disabled = true;
      document.getElementById('continue').disabled = false;
      document.getElementById('zip_sent').innerHTML = file_sent;
      progress.style.width = '100%';
      progress.textContent = '100%';
    } else {
      alert(http_request.status == 200 ? http_request.responseText : 'Server error code: ' + http_request.status);
    }      
  };

  http_request.open("POST", url+"?202");
  http_request.send(data);
}

function do_ajax_upload_ini(url, text) {
  var http_request = createXMLHttpRequest();
  if (!http_request) {
    alert('Javascript error: no XMLHTTP instance');
    return false;
  }
  http_request.open("POST", url+"?201", false);
  http_request.setRequestHeader("Content-type", "text/xml");
  http_request.send(text);
  var resp = http_request.responseText;
  var inifile = document.getElementsByName('inifile');
  var ini = document.getElementById('ini');
  var zip = document.getElementById('zip');
  var progress = document.querySelector('.percent');
  progress.style.width = '100%';
  progress.textContent = '100%';
  document.getElementById('progress_bar').className = 'loading';
  if (resp.indexOf("ezin") == 0) {
    inifile[0].value = resp;
    ini.disabled = true;
    zip.disabled = false;
    document.getElementById('ini_sent').innerHTML = file_sent;
    document.getElementById('zip').addEventListener('change', handleZIPFile, false);
  } else {
    alert(resp);
    return false;
  }
  return true;
}

/****
 * File error handling, thanks to Eric Bidelman http://www.html5rocks.com/
 ****/

  function errorHandler(evt) {
    switch(evt.target.error.code) {
      case evt.target.error.NOT_FOUND_ERR:
        alert('File Not Found!');
        break;
      case evt.target.error.NOT_READABLE_ERR:
        alert('File is not readable');
        break;
      case evt.target.error.ABORT_ERR:
        break; // noop
      default:
        alert('An error occurred reading this file.');
    };
  }

function handleZIPFile(evt) {
  var f = evt.target.files[0]; // file object
  // Only process compressed archives.
  var ext =  f.name.split('.').pop().toLowerCase();
  if ( ext!='zip' && ext!='bz2' && ext!='gz' && ext!='tgz' && ext!='tbz' && ( with_7zip==false || ext!='7z' ) ) {
    alert('zip/gzip/bzip2 files only');
    return false;
  }
  do_ajax_upload_zip(evt.target.url, f.name);
  return true;
}


function handleXMLFile(evt) {
    var f = evt.target.files[0]; // file object
      // Only process xml files.
      if (!f.type.match('text/xml')) {
        alert('xml files only');
        return false;
      }
      var reader = new FileReader();
      reader.onerror = errorHandler;
      reader.onload = function(e) {
        var xml = parseXml(e.target.result);
        if (xml.documentElement.tagName != 'ezinstaller') {
          alert('incompatible xml file');
          return false;
        }
        do_ajax_upload_ini(evt.target.url, xml);
      };
      reader.readAsText(f);
      return true;
}

function InitAjax(url, file_sent_text, sevenzip) {
  if (!(window.File && window.FileReader)) {
    document.getElementById('continue').style.display = "none";
    document.getElementById('submit').style.display = "inline";
    document.getElementById('reset').style.display = "inline";
    document.getElementById('zip').disabled = false;
    return;
  }
  file_sent = file_sent_text;
  with_7zip = sevenzip;
  document.getElementById('ini').url = url;
  document.getElementById('zip').url = url;
  document.getElementById('ini').addEventListener('change', handleXMLFile, false);
}

function toggle_upload(el) {
  down = document.getElementById('ini_download');
  up = document.getElementById('ini_upload');
  down_row = document.getElementById('down_row');
  up_row = document.getElementById('up_row');
  if (el.checked) {
    up_row.style.display = 'block';
    down_row.style.display = 'none';
    up.disabled = false;
    down.disabled = true;
  } else {
    up_row.style.display = 'none';
    down_row.style.display = 'block';
    up.disabled = true;
    down.disabled = false;
  }
}

