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
  document.getElementById('zip').addEventListener('change', handleZIPFile, false);
  alert('initialization file sent');
  return true;
}

/****
 * File handling, thanks to Eric Bidelman http://www.html5rocks.com/
 ****/
  var reader;
  var progress; // = document.querySelector('.percent');

  function abortRead() {
    reader.abort();
  }

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

  function updateProgress(evt) {
    // evt is an ProgressEvent.
    if (evt.lengthComputable) {
      var percentLoaded = Math.round((evt.loaded / evt.total) * 100);
      // Increase the progress bar length.
      if (percentLoaded < 100) {
        progress.style.width = percentLoaded + '%';
        progress.textContent = percentLoaded + '%';
      }
    }
  }
 
function handleZIPFile(evt) {
    var f = evt.target.files[0]; // file object
    /*
    var ext =  f.name.split('.').pop().toLowerCase();
      if ( ext!='zip' && ext!='bz2' && ext!='gz' && ext!='tgz' && ext!='tbz' ) {
        alert('zip/gzip/bzip2 files only');
        return false;
      }
    */
    // Reset progress indicator on new file selection.
    progress = document.querySelector('.percent');
    progress.style.width = '0%';
    progress.textContent = '0%';

    reader = new FileReader();
    reader.onerror = errorHandler;
    reader.onprogress = updateProgress;
    reader.onabort = function(e) {
      alert('File read cancelled');
    };
    reader.onloadstart = function(e) {
      document.getElementById('progress_bar').className = 'loading';
    };
    reader.onload = function(e) {
      // Ensure that the progress bar displays 100% at the end.
      progress.style.width = '100%';
      progress.textContent = '100%';
      setTimeout("document.getElementById('progress_bar').className='';", 2000);
    }

    // Read in the image file as a binary string.
    reader.readAsBinaryString(f);
}

function handleXMLFile(evt) {
    var f = evt.target.files[0]; // file object
      // Only process xml files.
      if (!f.type.match('text/xml')) {
        alert('xml files only');
        return false;
      }
      var reader2 = new FileReader();
      reader2.onload = function(e) {
        do_ajax_upload_ini(evt.target.url, e.target.result);
      };
      reader2.readAsText(f);
      return true;
}

function InitAjax(url) {
  document.getElementById('ini').url=url;
  document.getElementById('zip').url=url;
  document.getElementById('ini').addEventListener('change', handleXMLFile, false);
}