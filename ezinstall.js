/* 
 * ezinstall javascript file
 */
String.prototype.startsWith = function(str)
{ return (this.match("^" + str) == str) }

String.prototype.endsWith = function(str) 
{return (this.match(str+"$")==str)}

var textlen = 0;
var file_sent = "file sent";

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
    setTimeout(function(){do_ajax(arg1, arg2, url, new Date().getTime())},750);
  }
  else {
    cont.disabled = false;
  }
}

function do_ajax_upload_zip(url, filename, binary) {
  var http_request = createXMLHttpRequest();
  if (!http_request) {
    alert('Javascript error: no XMLHTTP instance');
    return false;
  }
  var progress = document.querySelector('.percent');
  progress.style.width = '0%';
  progress.textContent = '0%';
  document.getElementById('progress_bar').className = 'loading';

  var start, stop;
      alert(filename);

    for (start = 0; start < binary.length; start += 512) {
      stop = start + 511;
      if (binary.length - 1 <= stop) stop = binary.length - 1;
      var part = binary.substr(start, stop + 1);
      //http_request.open("POST", url+"?202", false);
      //http_request.setRequestHeader("Content-type", "text/xml");
      //http_request.send(text);
      var percentLoaded = Math.round((stop / binary.length) * 100);
      if (percentLoaded > 100) percentLoaded = 100;
      progress.style.width = percentLoaded + '%';
      progress.textContent = percentLoaded + '%';
    }
    var zipfile = document.getElementsByName('zipfile');
    var ini = document.getElementById('ini');
    var zip = document.getElementById('zip');
    zipfile[0].value = filename;
    zip.disabled = true;
    document.getElementById('continue').disabled = false;
    document.getElementById('zip_sent').innerHTML = file_sent;
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
  inifile[0].value = resp;
  ini.disabled = true;
  zip.disabled = false;
  document.getElementById('ini_sent').innerHTML = file_sent;
  document.getElementById('zip').addEventListener('change', handleZIPFile, false);
  return true;
}

/****
 * File handling, thanks to Eric Bidelman http://www.html5rocks.com/
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

/*
function handleZIPFile(evt) {
    var file = evt.target.files[0]; // file object

    var ext =  file.name.split('.').pop().toLowerCase();
      if ( ext!='zip' && ext!='bz2' && ext!='gz' && ext!='tgz' && ext!='tbz' ) {
        alert('zip/gzip/bzip2 files only');
        return false;
      }

    // Reset progress indicator on new file selection.
    var progress = document.querySelector('.percent');
    progress.style.width = '0%';
    progress.textContent = '0%';
    document.getElementById('progress_bar').className = 'loading';

    var start, stop;
    var blob;
    var reader;

    // If we use onloadend, we need to check the readyState.
    for (start = 0; start < file.size; start += 512) {
      reader = new FileReader();
      reader.onerror = errorHandler;
      stop = start + 511;
      if (file.size - 1 <= stop) stop = file.size -1;
      reader.onload = function(evt) {
        if (evt.target.readyState == FileReader.DONE) { // DONE == 2
          alert(start+"!");
          //alert(evt.target.result);
        }
      };
      blob = file.slice(start, stop + 1);
      reader.readAsBinaryString(blob);
      var percentLoaded = Math.round((stop / file.size) * 100);
      if (percentLoaded > 100) percentLoaded = 100;
      progress.style.width = percentLoaded + '%';
      progress.textContent = percentLoaded + '%';
    }
    var zipfile = document.getElementsByName('zipfile');
    var ini = document.getElementById('ini');
    var zip = document.getElementById('zip');
    zipfile[0].value = file.name;
    alert(file.name);
    zip.disabled = true;
    document.getElementById('continue').disabled = false;
    document.getElementById('zip_sent').innerHTML = file_sent;
}
*/
function handleZIPFile(evt) {
    var f = evt.target.files[0]; // file object
      // Only process compressed archives.
    /*
    var ext =  f.name.split('.').pop().toLowerCase();
      if ( ext!='zip' && ext!='bz2' && ext!='gz' && ext!='tgz' && ext!='tbz' ) {
        alert('zip/gzip/bzip2 files only');
        return false;
      }
    */
      var reader = new FileReader();
      reader.onerror = errorHandler;
      reader.onload = function(e) {
        do_ajax_upload_zip(evt.target.url, f.name, e.target.result);
      };
      reader.readAsBinaryString(f);
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
        do_ajax_upload_ini(evt.target.url, e.target.result);
      };
      reader.readAsText(f);
      return true;
}

function InitAjax(url, file_sent_text) {
  file_sent = file_sent_text;
  document.getElementById('ini').url=url;
  document.getElementById('zip').url = url;
  document.getElementById('ini').addEventListener('change', handleXMLFile, false);
}