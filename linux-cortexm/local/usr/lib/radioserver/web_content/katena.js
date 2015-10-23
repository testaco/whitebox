/*
 * Copyright (C) 2015 Algoram. Dual-licensed: Affero GPL 3 is the default,
 * and a commercial license is available for sale. Contact Bruce Perens K6BP
 * via email to bruce@perens.com to commercially license this program for your
 * device.
 *
 * Web radio front panel, by Bruce Perens K6BP.
 */
'use strict';

console.log('Version 5');

var netSampleRate = 8192;

var constraint = {
  audio: true,
  video: false
};

// Web Audio API and WebSocket aren't final standards at this writing.
// So we have to convert from the "prefixed" versions of operators.
navigator.getUserMedia = navigator.getUserMedia || navigator.webkitGetUserMedia || navigator.mozGetUserMedia;

window.WebSocket = window.WebSocket || window.webkitWebSocket || window.mozWebSocket;

window.AudioContext = window.AudioContext || window.webkitAudioContext || window.mozAudioContext;

// Zero-copy FIFO derived from the one I wrote in C++ for DVS.
var FIFO = function(type, size) {
  this.type = type || Float32Array;
  this.size = size || 8192;

  this.buffer = new ArrayBuffer(size * this.type.BYTES_PER_ELEMENT);
  this.end = size;
  this.reset();
}

FIFO.prototype = {
  constructor: FIFO,

  error: function() {
    var s = "";
    for ( var i = 0; i < arguments.length; i++ ) {
      if ( i > 0 ) {
        s += ' ';
      }
      s += arguments[i];
    }
    throw new Error(s);
  },

  get: function(length) {
    if ( length > this.queuedLength() ) {
      this.error("FIFO outgoing data overrun.");
    }
    var view = new this.type(this.buffer, this.out * this.type.BYTES_PER_ELEMENT, length);
    this.out += length;
    if ( this.out == this.inp ) {
      this.reset();
    }
    return view;
  },

  put: function(length) {
    if ( this.inp + length >= this.end ) {
      return this.reorder(length);
    }
    else {
      var view = new this.type(this.buffer, this.inp * this.type.BYTES_PER_ELEMENT, length);
      this.inp += length;
      return view;
    }
  },

  queuedLength: function() {
    return (this.inp - this.out);
  },

  reorder: function(length) {
    if ( length > this.writableLength() ) {
      this.error("FIFO incoming data overrun.");
    }
    if ( this.out > 0 && (this.inp - this.out) > 0 ) {
      var copyView = new this.type(this.buffer, 0, this.inp);
      if ( copyView.copyWithin ) {
        copyView.copyWithin(0, this.out, this.inp);
      }
      else {
        copyWithin(copyView, this.out, this.queuedLength());
      }
      copyView = null;
      this.inp = (this.inp - this.out);
      this.out = 0;
    }

    var view = new this.type(this.buffer, this.inp * this.type.BYTES_PER_ELEMENT, length);
    this.inp += length;
    return view;
  },

  reset: function(length) {
    this.inp = 0;
    this.out = 0;
  },

  writableLength: function() {
    return (this.end - this.inp) + this.out;
  },
}

// Acquire the microphone.
function acquireMicrophone() {
  notice('Acquiring the microphone.');
  navigator.getUserMedia(constraint, gotMicrophone, didNotGetMicrophone);
}

// The close Promise has been fulfilled. Complete the shutdown operation.
function audioContextCloseFulfilled() {
  var r = window.radioclient;

  r.context = null;
}

// Something kept the audio context from closing.
function audioContextCloseRejected() {
}

// The suspend Promise has been fulfilled. Continue the shut-down operation.
function audioContextSuspendFulfilled() {
  var r = window.radioclient;

  if (r.inp.source) {
    // This is probably unnecessary, but the web audio API isn't standardized
    // so I'm being careful.
    r.inp.source.disconnect();
    r.inp.source = null;
    r.inp.compressor.disconnect();
    r.inp.compressor = null;
    r.inp.worker.disconnect();
    r.inp.worker = null;
    r.out.worker.disconnect();
    r.out.worker = null;
  }

  if (r.context.close) {
    var ret = ignoreErrors(function() {return r.context.close()});
    if (ret) {
      ret.then(audioContextCloseFulfilled, audioContextCloseRejected);
    }
  }
}

// Something kept the audio context from suspending.
function audioContextSuspendRejected() {
  console.log('Audio context suspend promise rejected.');
}

// Build the audio graph.
function buildAudioGraph(stream) {
  var r = window.radioclient;
  var inp = {};
  r.inp = inp;
  var out = {};
  r.out = out;

  // Build the input and output graphs.

  // Input Graph.

  notice('Building the audio context');
  r.context = new AudioContext();
  if ( !r.context ) {
    notice('Failed to create the audio context.');
    return;
  }

  r.inputResamplingContext = {
    inRate: r.context.sampleRate,
    outRate: netSampleRate,
    scale: 4096
  };

  r.outputResamplingContext = {
    inRate: netSampleRate,
    outRate: r.context.sampleRate,
    scale: 1 / 4096
  };

  if ( !r.context.createMediaStreamSource ) {
    notice("Missing API: AudioContext.createMediaStreamSource.");
    return;
  }

  // Source is the microphone input.
  // It won't be connected until it's time to start.
  inp.source = r.context.createMediaStreamSource(stream);

  // For testing without a working microphone.
  // inp.source = r.context.createOscillator();
  // inp.source.frequency = 1000;
  // inp.source.type = 'sine';

  // Level controls the microphone level.
  inp.level = r.context.createGain();
  inp.level.gain.value = 15;

  // Compressor/limiter on the microphone audio.
  inp.compressor = r.context.createDynamicsCompressor();

  // The amount of change, in dB, needed for a 1 dB change in output.
  inp.compressor.ratio.value = 10;

  // dB value above which compression will start taking effect.
  inp.compressor.threshold.value = -6;

  // Decibel value representing the range above the threshold where the curve
  // smoothly transitions to the compressed portion;
  inp.compressor.knee.value = -12;

  // The amount of time, in seconds, required to reduce the gain by 10 dB.
  inp.compressor.attack.value = 0.1;

  // The amount of time, in seconds, required to increase the gain by 10 dB.
  inp.compressor.release.value = 0.1;

  // The amount of gain reduction currently applied by the compressor to the signal.
  // Intended for metering purposes, it returns a value in dB, or 0 (no gain
  // reduction) if no signal is fed into the DynamicsCompressorNode. The range of
  // this value is between -20 and 0 (in dB).
  // I am not clear if there's any point in writing it here.
  inp.compressor.reduction.value = -20;

  inp.level.connect(inp.compressor);

  // Low-pass filter for resampling.
  inp.lowpass = r.context.createBiquadFilter();
  inp.lowpass.type = 'lowpass';
  inp.lowpass.frequency.value = netSampleRate / 2;
  inp.lowpass.Q.value = 1.0 / Math.sqrt(3);
  inp.compressor.connect(inp.lowpass);


  // The input worker performs the data conversion and transmits the data.
  // At this writing, Chrome won't start the input graph unless this node
  // has an output and the graph is connected to a destination. So, we end
  // up sending lots of silence to PulseAudio. We can use this to implement
  // a sidetone later on. Size the buffer so that it runs about every tenth
  // of a second.
  inp.worker =
    r.context.createScriptProcessor(Math.max(256, nearestPowerOf2(
      r.context.sampleRate / 5.0)), 1, 1);
  inp.worker.onaudioprocess = processAudioInput;
  inp.lowpass.connect(inp.worker);
  inp.worker.connect(r.context.destination);

  // Output Graph.

  // The output worker takes the received audio from a queue and
  // feeds it to the audio graph. Size it so that it runs about every tenth
  // second. It will be processing payloads of size (netSampleRate * 2) per
  // second.
  out.worker =
    r.context.createScriptProcessor(Math.max(256, nearestPowerOf2((netSampleRate * 2) / 5.0)), 0, 1);

  out.worker.onaudioprocess = processAudioOutput;
}

// Build the websocket.
function buildSocket() {
  notice('Opening a connection to the radio.');
  var r = window.radioclient;
  var protocol = window.location.protocol == 'https:' ? 'wss' : 'ws';
  // FIX: Cut this down to 2 seconds once the server sends audio at the proper rate.
  r.receiveQueue = new FIFO(Float32Array, r.context.sampleRate * 30);

  // At this writing, Firefox has a bug in which it emits connection-interrupted
  // messages on the browser console regarding _old_ websockets, not the one you're
  // using! It even emits them after the browser loads another page entirely.
  // Until the browser programmers fix this bug, there is no way to fix it in
  // user code.
  r.soc = new WebSocket(protocol + '://' + window.location.host + '/WebSocket/', 'radio-server-1');

  r.soc.onerror = function(event) {
    websocketError(event);
  };
  r.soc.onclose = function(event) {
    serverDisconnected(event);
  };
  r.soc.onopen = function(event) {
    gotSocket();
  };
  r.soc.onmessage = response;
  r.soc.binaryType = 'arraybuffer';
  notice("Waiting for the WebSocket connection. readyState:" + r.soc.readyState);
}

// Keyboard events don't directly return the character code. This is a cheat and doesn't
// work for every key. A keycode to character table would work better.
function charCodeFromKeyboardEvent(event) {
  return String.fromCharCode(event.keyCode);
}

function copy(inp, out, length)
{
  length = length|0;

  for ( var i = 0|0; i < length; i++ ) {
    out[i] = b[i];
  }
}

function copyWithin(inp, offset, length)
{
  offset = offset|0;
  length = length|0;

  for ( var i = 0|0; i < length; i++ ) {
    inp[i] = inp[i + offset];
  }
}

function errorHandler(errorMessage, url, lineNumber, column, errorObject)
{
  var page =
    '<h1>Oops!</h1>' +
    '<p>There was an unexpected error.' +
    ' This can be a software bug, a browser incompatibility,' +
    ' or sometimes a misconfiguration such as a lack of permission' +
    ' to access a device.</p>' +
    '<dl>' +
    '<dt>Error message</dt><dd>' +
    errorMessage +
    '</dd><dt>Line number</dt><dd>' +
    lineNumber +
    '</dd><dt>Column</dt><dd>' +
    column +
    '</dd><dt>Error object</dt><dd>' +
    errorObject +
    '</dd>';
 
  if ( errorObject && errorObject.message ) {
    page = page + '<dt>Error object message</dt><dd>' +
     errorObject.message +
     '</dd>';
  }
  if ( errorObject && errorObject.toSource ) {
    page = page + '<dt>Error source</dt></dd>' +
    errorObject.toSource() +
    '</dd>';
  }
  if ( errorObject && errorObject.stack ) {
    page = page + '<dt>Call stack</dt><dd><pre>' +
    errorObject.stack +
    '</pre></dd>';
  }
  document.body.innerHTML = page + '</dl>';
}

function receivingNotice() {
  var r = window.radioclient;

  if (r.transmitting) {
    return;
  }

  var s = 'Receiving';

  if (r.delayedReceivePacketsPercentage > 0) {
    s += ', ' + r.delayedReceivePacketsPercentage + '% late arrival of network audio packets.';
  } else {
    s += '.';
  }
  notice(s);
}

// This is invoked if the user denies us use of the microphone.
// Complain to the user, and then go back to the monitor page.
function didNotGetMicrophone(error) {
  if (error.name == 'PermissionDeniedError') {
    notice('No permission to use the microphone.');
  } else {
    notice("Didn't get the microphone, error " + error.name + ': ' + error.message);
  }
}

// Restrict the input to the frequency field.
// Key code 13 is the carriage return, which causes the form to submit.
// Key code 8 is backspace.
// Key code 46 is delete.
function frequencyFieldKeyEvent(event) {
  var c = String.fromCharCode(event.charCode);
  if ((c < '0' || c > '9') && c != '.' && event.keyCode != 13 && event.keyCode != 8 && event.keyCode != 46) {
    event.stopPropagation();
    event.preventDefault();
    return false;
  }
}

// Entry point to this script.
function go(foo) {
  console.log('Copyright (C) 2015 Algoram. Dual-licensed: Affero GPL 3 is the default,' + ' and a commercial license is available for sale.');
  var r = {};
  window.radioclient = r;
  var element;

  r.noticeBox = document.getElementById('Notice');
  notice("Javascript running...");

  window.onerror = errorHandler;
  var hasWebSocket = window['webSocket'];
  var hasWebAudioAPI = navigator['getUserMedia'];
  var hasAudioContext = window['AudioContext'];

  var lacking = '';
  if ( !window.WebSocket ) {
    lacking += " WebSocket";
  }
  if ( !navigator.getUserMedia ) {
    lacking += " getUserMedia";
  }
  if ( !window.AudioContext ) {
    lacking += " AudioContext";
  }
  if ( lacking != '' ) {
    var s = 'Sorry: This browser and system combination lack the necessary APIs:';
    s += lacking;
    s += '. User Agent: ' + navigator.userAgent;
    notice(s);
    return;
  }

  r.disable = false;
  r.inputOffset = 0;
  r.outputOffset = 0;
  r.currentNotice = null;
  r.tooFast = false;

  // Clean up before unloading the page.
  // This may not be necessary, but neither WebSocket nor the Web Audio API
  // are standardized as I write this.
  window.addEventListener('beforeunload', tearDown, false);
  window.addEventListener('keydown', keyDown, true);
  window.addEventListener('keyup', keyUp, true);
  element = document.getElementById('frequency');
  element.addEventListener('keypress', frequencyFieldKeyEvent, true);
  element.focus();
  document.getElementById('mode').addEventListener('change', sendParameters, true);
  document.getElementById('parametersForm').addEventListener('submit', parametersFormSubmit, true);
  element = document.getElementsByClassName('TransmitButton')[0];
  element.addEventListener('mousedown', transmit, false);
  element.addEventListener('keyup', keyUp, true);
  element = document.getElementsByClassName('ReceiveButton')[0];
  element.addEventListener('mousedown', receive, false);
  element.addEventListener('keyup', keyUp, true);

  acquireMicrophone();
}

// This is called when we succeed in acquiring the microphone.
function gotMicrophone(stream) {
  notice('In control of the Microphone.');
  buildAudioGraph(stream);
  notice('Built the audio graph.');
  buildSocket();
}

// This is called when we succeed in opening the socket.
function gotSocket() {
  var r = window.radioclient;

  document.getElementsByClassName('Controls')[0].id = 'ControlsVisible';
  r.transmitting = true; // Just so that receive will start.
  notice('Connected.');
  receive();
}

// Call the passed-in function, ignoring exceptions.
// This is briefer than a try {} catch(error) {} block.
// It would be prettier with ES6 elipsis and arrow functionality.
function ignoreErrors(block /* ...args */)
{
  try {
    return block.apply(this, Array.prototype.slice.call(arguments, 1));
  }
  catch (error) {
    return undefined;
  }
}

// Called on any key down in the window.
function keyDown(event) {
  var r = window.radioclient;

  if (charCodeFromKeyboardEvent(event) == ' ') {
    if (!r.transmitKeyDown) { // Ignore key repeat.
      r.transmitKeyDown = true;
      transmit();
    }
    event.stopPropagation();
    event.preventDefault();
    return false;
  }
}

// Called on any key-up in the window.
function keyUp(event) {
  var r = window.radioclient;

  if (charCodeFromKeyboardEvent(event) == ' ') {
    r.transmitKeyDown = false; // Stop ignoring key repeat.
    receive();
    event.stopPropagation();
    event.preventDefault();
    return false;
  }
}

// Used for sizing worker nodes.
function nearestPowerOf2(n) {
  return Math.pow(2, Math.round(Math.log(n) / Math.log(2)));
}

function notice(message) {
  var r = window.radioclient;

  if (r.currentNotice != message) {
    r.currentNotice = message;
    r.noticeBox.innerHTML = message;
  }
}

function parametersFormSubmit(event) {
  event.stopPropagation();
  event.preventDefault();
  sendParameters();

  return false;
}

// Resample by interpolating between the nearest two samples, no filtering.
// This is required to produce no click between two runs. We can go to a
// higher-order resampling later, although the high-frequency artifacts of
// this aren't large and we can use the Web Audio API to pre- or post-filter.
//
function resample(inp, out, inLength, outLength, context) {
  inLength = inLength|0;
  outLength = outLength|0;

  var inToOutRatio = +context.inRate / context.outRate;
  var scale = +context.scale;
  var last = +context.last || 0.0;

  // Delta is the offset inherited from the last run so that this run concatenates
  // seamlessly with it. It is initialized to 0.5 to offset from the midpoint of the
  // sample to the lower boundary. Setting it here hoists a common "-0.5" operation
  // from the loop.
  var delta = +context.delta != undefined ? context.delta : -0.5;

  // Call the inner loop with type coercion, for the optimizer.
  resampleInnerLoop(inp, out, outLength|0, +last, +inToOutRatio, +delta, +scale);

  var point = +((outLength + 0.5) * inToOutRatio) + delta;
  var index = ~~Math.floor(point)|0;
  context.last = index < 0 ? last : inp[index];
  context.delta = ((Math.abs(point) % 1) - 1) - (0.5 * inToOutRatio);
  return out;
}

// Given the length of the output array, return the length of the input array.
function resamplingInputLength(length, context)
{
  if ( typeof(context.delta) == "undefined" ) {
    context.delta = -0.5;
  }
  var inToOutRatio = context.inRate / context.outRate;
  return Math.floor(((length - 0.5) * inToOutRatio) + context.delta) + 1;
}

// Given the length of the input array, return the length of the ouput array.
function resampledOutputLength(length, context)
{
  if ( typeof(context.delta) == "undefined" ) {
    context.delta = -0.5;
  }
  var inToOutRatio = context.inRate / context.outRate;
  return Math.floor(((length - 1 - context.delta) / inToOutRatio) - 0.5) + 1;
}

// Inner loop of resample, optimizes well on V8, might eventually port to asm.js
// but V8 optimizes well enough that it's hard to see the point of asm.js .
function resampleInnerLoop(inp, out, outLength, last, inToOutRatio, delta, scale) {
  var prefixLength = Math.min((Math.floor(((((0.5) * inToOutRatio) - delta) / inToOutRatio) - 0.5) + 1), outLength);
  var next = inp[0];
  for (var i = 0|0; i < prefixLength|0; i++) {
    var point = +((i + 0.5) * inToOutRatio) + delta;
    var offset = +(1 + point) % 1;
    out[i] = +(((last * (1 - offset))) + (next * offset)) * scale;
  }
  for (; i < outLength|0; i++) {
    var point = +((i + 0.5) * inToOutRatio) + delta;
    var offset = +point % 1;
    var index = ~~Math.floor(point)|0;
    out[i] = +(((inp[index] * (1 - offset))) + (inp[index + 1] * offset)) * scale;
  }
}

// This runs in input.worker.
// Do data conversion and resampling on the audio input and send it to the
// svar ocket.
function processAudioInput(event) {
  var r = window.radioclient;
  var context = r.inputResamplingContext;

  if (!r.transmitting || r.disable) {
    return;
  }

  var inp = event.inputBuffer.getChannelData(0);
  var length = resampledOutputLength(inp.length, context);
  var buffer = new ArrayBuffer((length * 2) + 4);
  var type = new Uint32Array(buffer, 0, 1);
  type[0] = 1;
  var out = new Int16Array(buffer, 4, length);
  resample(inp, out, inp.length, out.length, r.inputResamplingContext);
  r.soc.send(buffer);
}

// At this writing, V8 optimizer doesn't like const. For no reason that makes sense
// to me.
function processAudioOutput(event) {
  var r = window.radioclient;

  if (r.disable) {
    return;
  }

  var out = event.outputBuffer.getChannelData(0);
  var length = r.out.worker.bufferSize|0;

  if (!r.transmitting) {
    if (r.receiveQueue.queuedLength() < length) {
      r.delayedReceivePackets += 1;
    }
    else {
      r.onTimeReceivePackets += 1;
    }
  }

  var offset = 0;
  if (!r.transmitting && r.receiveQueue.queuedLength() > 0 ) {
    var used = +Math.min(r.receiveQueue.queuedLength(), length);
    var inp = r.receiveQueue.get(used); 
    if ( out.set ) {
      out.set(inp);
    }
    else {
      copy(inp, out, used);
    }
    length -= used;
    offset = used;
  }
  if ( length > 0 ) {
    var end = (offset + length)|0;
    if ( out.fill ) {
      out.fill(0, offset, end);
    }
    else {
      for (var i = offset|0; i < end|0; i++) {
        out[i] = 0;
      }
   }
  }

  var oldRatio = r.delayedReceivePacketsPercentage;
  r.delayedReceivePacketsPercentage = Math.round((r.delayedReceivePackets / (r.delayedReceivePackets + r.onTimeReceivePackets)) * 100);

  if (oldRatio != r.delayedReceivePacketsPercentage) {
    receivingNotice();
  }
}

// Finish transmit, start receive.
function receive() {
  var r = window.radioclient;

  if (r.transmitting) {
    r.transmitting = false;

    r.delayedReceivePackets = 0;
    r.onTimeReceivePackets = 0;
    r.delayedReceivePacketsPercentage = 0;

    receivingNotice();

    var text = {
      command: 'receive'
    };
    r.soc.send(JSON.stringify(text));

    ignoreErrors(function() {r.inp.source.disconnect(r.inp.level);});
    r.out.worker.connect(r.context.destination);
    document.getElementsByClassName('TransmitButton')[0].id = 'TransmitButtonReceiving';
  }
}

// Receive JSON from the server.
function response(event) {
  if (event.data instanceof ArrayBuffer) {
    responseArrayBuffer(event.data);
  } else if (typeof(event.data) == 'string') {
    responseString(event.data);
  } else {
    console.log('Unhandled response type: ' + typeof(event.data));
  }
}

function responseArrayBuffer(data) {
  var r = window.radioclient;

  if (r.disable || r.transmitting) {
    return;
  }

  var inTypeView = new Uint32Array(data, 0, 1);

  switch ( inTypeView[0] ) {
  case 0:
    responseString(data);
    break;
  case 1:
    responseReceiveAudio(data);
    break;
  default:
    notice("Received unknown data type " + inTypeView[0]);
  }
}

function responseReceiveAudio(data)
{
  var r = window.radioclient;
  var context = r.outputResamplingContext;

  var inDataView = new Int16Array(data, 4);
  var outputLength = resampledOutputLength(inDataView.length, context);

  // Don't bog down the browser by receiving too fast from broken software.
  if (outputLength > r.receiveQueue.writableLength()) {
    if (!r.tooFast) {
      notice('Server error: incoming audio rate exceeds ' + netSampleRate + ' samples per second. Output length = ' + outputLength + ", writable length = " + r.receiveQueue.writableLength());
    }
    r.tooFast = true;
    return;
  } else {
    r.tooFast = false;
  }

  var outView = r.receiveQueue.put(resampledOutputLength(inDataView.length, context));

  var out = resample(
    inDataView,
    outView,
    inDataView.length,
    outView.length,
    context);
}

function responseString(data) {
  var r = window.radioclient;
  var root = JSON.parse(data);

  if (root.frequency) {
    document.getElementById('frequency').value = root.frequency.toFixed(3);
  }
  if (root.mode) {
    document.getElementById('mode').value = root.mode;
  }
}

function sendParameters() {
  var r = window.radioclient;
  var frequency = parseFloat(document.getElementById('frequency').value);
  var mode = document.getElementById('mode').value;


  var text = {
    command: 'set',
    frequency: frequency,
    mode: mode
  };
  r.soc.send(JSON.stringify(text));
  notice('Settings sent to the radio.');
}

// The remote server has disconnected.
function serverDisconnected(event) {
  var r = window.radioclient;

  if (!r.reentered) {
    tearDown(event);
    document.body.innerHTML =
      'The server has disconnected. ' + '<form>' + "<input type='submit' value='Try Connecting Again'>" + '</form>';
  }
}

// Start transmit, end receive.
function transmit() {
  var r = window.radioclient;

  if (!r.transmitting) {
    r.transmitting = true;
    var text = {
      command: 'transmit'
    };
    r.soc.send(JSON.stringify(text));

    ignoreErrors(function() {r.out.worker.disconnect(r.context.destination);});
    r.receiveQueue.reset();
    r.inp.source.connect(r.inp.level);
    document.getElementsByClassName('TransmitButton')[0].id = 'TransmitButtonTransmitting';

    notice('Transmitting.');
  }
}

// Tear down the socket and audio graph, prior to abandoning this page.
// It may be that none of this is necessary. But neither WebSocket nor the
// web audio API is standardized as I write this.
function tearDown(event) {
  var r = window.radioclient;

  if (r && !r.reentered) {
    r.reentered = true;

    r.disable = true; // Disable further I/O.

    if (r.soc && r.soc.readyState == WebSocket.OPEN) {
      var text = {
        command: 'close'
      };
      r.soc.send(JSON.stringify(text));
    }

    if (r.soc && r.soc.close) {
      var ret = ignoreErrors(function() {return r.soc.close(1000, 'Goodbye.')});
      if (ret) {
        ret.then(webSocketCloseFulfilled, webSocketCloseRejected);
      }
    }


    if (r.context) {
      // AudioContext.suspend is not in FireFox at this writing.
      if (r.context.suspend) {
        r.context.suspend().then(
          audioContextSuspendFulfilled,
          audioContextSuspendRejected);
      }
      else {
        audioContextSuspendFulfilled();
      }
    }
    r.reentered = false;
  }
}

// The close Promise has been fulfilled. Complete the shutdown operation.
function webSocketCloseFulfilled() {
  console.log('WebSocket close promise fulfilled.');
}

// Something kept the web socket from closing.
function webSocketCloseRejected() {
  console.log('WebSocket close promise rejected.');
}

function websocketError(error) {
  notice("Didn't get the WebSocket, error " + error.name + ': ' + error.message);
  serverDisconnected(error);
}
