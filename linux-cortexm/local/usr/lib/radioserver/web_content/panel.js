/*
 * Copyright (C) 2015 Algoram. Dual-licensed: Affero GPL 3 is the defualt,
 * and a commercial license is available for sale.
 */

const net_sample_rate = 8192;

const constraint = {
  audio: true,
  video: false
};

// Web Audio API and WebSocket aren't final standards at this writing.
// So we have to convert from the "prefixed" versions of operators.
navigator.getUserMedia
 =  navigator.getUserMedia
 || navigator.webkitGetUserMedia
 || navigator.mozGetUserMedia;

window.WebSocket
 =  window.WebSocket
 || window.webkitWebSocket
 || window.mozWebSocket;

window.AudioContext
 =  window.AudioContext
 || window.webkitAudioContext
 || window.mozAudioContext;

// Acquire the microphone.
function acquireMicrophone() {
  notice("Acquiring the microphone." + window.WebSocket + navigator.getUserMedia);
  navigator.getUserMedia(constraint, gotMicrophone, didNotGetMicrophone);
}

// Build the audio graph.
function buildAudioGraph(stream)
{
  const r = window.radioclient;
  const inp = {};
  r.in = inp;
  const out = {};
  r.out = out;

  // Build the input and output graphs.

  // Input Graph.

  r.context = new AudioContext();

  r.inputInterpolationContext = {
    offset: 0,
    inRate: r.context.sampleRate,
    outRate: net_sample_rate,
  };

  r.outputInterpolationContext = {
    offset: 0,
    inRate: net_sample_rate,
    outRate: r.context.sampleRate,
  };

  // Source is the microphone input.
  // It won't be connected until it's time to start.
  //inp.source = r.context.createMediaStreamSource(stream);

  // For testing without a working microphone.
  inp.source = r.context.createOscillator();
  inp.source.frequency = 1000;
  inp.source.type = 'square';
  inp.source.start();

  // Level controls the microphone level.
  inp.level = r.context.createGain();
  inp.level.gain.value = 30;

  // Compressor/limiter on the microphone audio.
  inp.compressor = r.context.createDynamicsCompressor();
  inp.compressor.ratio.value = 20;
  inp.compressor.threshold.value = -50;
  inp.compressor.knee.value = -3;
  inp.compressor.attack.value = 0;
  inp.compressor.release.value = 0.1;
  inp.compressor.reduction.value = 1;
  inp.level.connect(inp.compressor);

  // Low-pass filter for resampling.
  inp.lowpass = r.context.createBiquadFilter();
  inp.lowpass.type = "lowpass";
  inp.lowpass.frequency.value = net_sample_rate / 2;
  inp.lowpass.Q = 1.0/Math.sqrt(3);
  // inp.compressor.connect(inp.lowpass);

  // Multiplies the Float32 data to the value we want for converting
  // it to a fixed-point Int16, but leaves the value in a Float32. The storage
  // type conversion will happen when an Int16Array is constructed in the
  // worker node.
  inp.multiply = r.context.createGain();
  inp.multiply.gain.value = 32768.0;
  // inp.lowpass.connect(inp.multiply);
  inp.compressor.connect(inp.multiply);

  // The input worker performs the data conversion and transmits the data.
  // At this writing, Chrome won't start the input graph unless this node
  // has an output and the graph is connected to a destination. So, we end
  // up sending lots of silence to PulseAudio. We can use this to implement
  // a sidetone later on. Size the buffer so that it runs about every tenth
  // of a second.
  inp.worker =
   r.context.createScriptProcessor(Math.max(256, nearestPowerOf2(
    r.context.sampleRate / 10.0)), 1, 1);
  inp.worker.onaudioprocess = processAudioInput;
  inp.multiply.connect(inp.worker);

  // Output Graph.

  // The output worker takes the received audio from a queue and
  // feeds it to the audio graph. Size it so that it runs about every tenth
  // second. It will be processing payloads of size (net_sample_rate * 2) per
  // second.
  out.worker =
   r.context.createScriptProcessor(Math.max(256, nearestPowerOf2((net_sample_rate * 2) / 10.0)), 0, 1);
  out.worker.onaudioprocess = processAudioOutput;
}

// Build the websocket.
function buildSocket() {
  notice("Opening a connection to the radio.");
  const r = window.radioclient;
  const protocol = window.location.protocol == "https:" ? "wss" : "ws";
  r.receiveQueue = [];

  r.soc = new WebSocket(protocol + "://" + window.location.host + "/foo", "radio-server-1") 
  r.soc.binaryType = 'arraybuffer';
  r.soc.onopen = function(event) { gotSocket(); };
  r.soc.onclose = function(event) { window.radioclient.soc = 0; tearDown(); serverDisconnected(); };
  r.soc.onmessage = response;
  r.soc.onerror = function(event) { websocketError(event); };
}

// Keyboard events don't directly return the character code. This is a cheat and doesn't
// work for every key. A keycode to character table would work better.
function charCodeFromKeyboardEvent(event)
{
  return String.fromCharCode(event.keyCode);
}

// The close Promise has been fulfilled. Complete the shutdown operation.
function closeFulfilled()
{
  const r = window.radioclient;

  r.context = null;
}

// Something kept the audio context from closing.
function closeRejected()
{
  console.log("Close Promise rejected.");
}

function receivingNotice()
{
  const r = window.radioclient;
  var s = "Receiving"

  if ( r.delayed_receive_packets_ratio > 0 )
    s += ", " + r.delayed_receive_packets_ratio + "% late arrival of network audio packets.";
  else
    s += ".";
  notice(s);
}

// This is invoked if the user denies us use of the microphone.
// Complain to the user, and then go back to the monitor page.
function didNotGetMicrophone(error)
{
  if ( error.name == "PermissionDeniedError" ) {
    notice("No permission to use the microphone.");
  }
  else {
    notice("Didn't get the microphone, error " + error.name + ": " + error.message);
  }
}

// Restrict the input to the frequency field.
// Key code 13 is the carriage return, which causes the form to submit.
// Key code 8 is backspace.
// Key code 46 is delete.
function frequencyFieldKeyEvent(event)
{
  const c = String.fromCharCode(event.charCode);
  if ( (c < '0' || c > '9') && c != '.' && event.keyCode != 13 && event.keyCode != 8 && event.keyCode != 46 ) {
    event.stopPropagation();
    event.preventDefault();
    return false;
  }
}

// Entry point to this script.
function go(foo)
{
  console.log("Copyright (C) 2015 Algoram. Dual-licensed: Affero GPL 3 is the default,"
   + " and a commercial license is available for sale.");
  const r = new Object;
  window.radioclient = r;
  var element;

  r.disable = false;
  r.notice_box = document.getElementById('Notice');
  r.inputOffset = 0;
  r.outputOffset = 0;

  if ( !window.WebSocket || !navigator.getUserMedia || !window.AudioContext ) {
    s = "Sorry: This browser doesn't support:"
    if ( !window.WebSocket )
      s += " WebSocket";
    if ( !window.getUserMedia )
      s += " getUserMedia";
    if ( !window.AudioContext )
      s += " AudioContext";
    s += ". Recommended Browsers: Chrome, Mozilla, Opera, Bowser.";
    notice(s);
  }
  else {
    // Clean up before unloading the page.
    // This may not be necessary, but neither WebSocket nor the Web Audio API
    // are standardized as I write this.
    window.addEventListener("beforeunload", tearDown, false);
    window.addEventListener("keydown", keyDown, true);
    window.addEventListener("keyup", keyUp, true);
    element = document.getElementById('frequency');
    element.addEventListener('keypress', frequencyFieldKeyEvent, true);
    element.focus();
    document.getElementById('mode').addEventListener('change', sendParameters, true);
    document.getElementById('lna').addEventListener('change', sendParameters, true);
    document.getElementById('noise').addEventListener('change', sendParameters, true);
    document.getElementById('mute_lo').addEventListener('change', sendParameters, true);
    document.getElementById('vga').addEventListener('change', sendParameters, true);
    document.getElementById('if_bw').addEventListener('change', sendParameters, true);
    document.getElementById('bpf').addEventListener('change', sendParameters, true);
    document.getElementById('pa').addEventListener('change', sendParameters, true);
    document.getElementById('led').addEventListener('change', sendParameters, true);
    document.getElementById('parameters_form').addEventListener('submit', parametersFormSubmit, true);
    element = document.getElementsByClassName('TransmitButton')[0];
    element.addEventListener('mousedown', transmit, false);
    element.addEventListener('keyup', keyUp, true);
    element = document.getElementsByClassName('ReceiveButton')[0];
    element.addEventListener('mousedown', receive, false);
    element.addEventListener('keyup', keyUp, true);
  
    acquireMicrophone();
  }
}

// This is called when we succeed in acquiring the microphone.
function gotMicrophone(stream)
{
  buildAudioGraph(stream);
  buildSocket();
}

// This is called when we succeed in opening the socket.
function gotSocket()
{
  const r = window.radioclient;

  r.transmitting = true; // Just so that receive will start.
  receive();
  notice("Connected.");
  document.getElementsByClassName('Controls')[0].id = 'ControlsVisible';
}

// Interpolate the loudspeaker audio data.
// Pull the required samples from the receiveQueue, write <size> samples to
// <out>.
// FIX: This is currently point sampling, make it linear interpolate.
function interpolateOutput(out, size, inRate, outRate)
{
  const r = window.radioclient;

  pointSample(r.receiveQueue, out, r.receiveQueue.length, out.length, r.outputInterpolationContext);
}

// Called on any key down in the window.
function keyDown(event)
{
  const r = window.radioclient;

  if ( charCodeFromKeyboardEvent(event) == " " ) {
    if ( !r.transmitKeyDown ) { // Ignore key repeat.
      r.transmitKeyDown = true;
      transmit();
    }
    event.stopPropagation();
    event.preventDefault();
    return false;
  }
}

// Called on any key-up in the window.
function keyUp(event)
{
  if ( charCodeFromKeyboardEvent(event) == " " ) {
    r.transmitKeyDown = false; // Stop ignoring key repeat.
    receive();
    event.stopPropagation();
    event.preventDefault();
    return false;
  }
}

// Used for sizing worker nodes.
function nearestPowerOf2(n)
{
  x = Math.pow(2, Math.round(Math.log(n) / Math.log( 2 )));
  return x;
}

function notice(message)
{
  const r = window.radioclient

  if ( r.notice_box.innerHTML != message )
    r.notice_box.innerHTML = message;
}

function parametersFormSubmit(event)
{
  event.stopPropagation();
  event.preventDefault();
  sendParameters();

  return false;
}

// Point sampling without interpolation. In the case of the 
// microphone audio, filtering has been done in the web audio graph which
// makes up for the lack of interpolation.
function pointSample(inp, out, inLength, outLength, context)
{
  const ratio = context.inRate / context.outRate;
  const offset = context.offset;
  const scale = context.scale;

  if ( !outLength )
    outLength = Math.round((inLength - offset) / ratio);

  // If the out argument is a constructor, construct a new object to use
  // for the output, and return it after processing.
  if ( typeof(out) == "function" ) {
    out = new out(outLength);
  }

  for ( var i = 0; i < outLength; i++ ) {
    const j = Math.round((i * ratio) + offset);
    if ( j > inLength )
      console.log("pointSample input buffer overrun.");
    out[i] = inp[j] * scale;
  }
  const used = (outLength * ratio) + offset;
  context.offset = ((outLength * ratio) + offset) % ratio; 
  return out;
}

// This runs in input.worker.
// Do data conversion and resampling on the audio input and send it to the
// socket.
function processAudioInput(event)
{
  const r = window.radioclient

  if ( !r.transmitting || r.disable )
    return;

  const data = event.inputBuffer.getChannelData(0);
  r.soc.send(pointSample(
   data, Int16Array, data.length, null, r.inputInterpolationContext));
}

// This runs in output.worker.
function processAudioOutput(event)
{
  const r = window.radioclient

  const length = r.out.worker.bufferSize;
  const out = event.outputBuffer.getChannelData(0);

  if ( !r.transmitting && r.receiveQueue.length > 0 ) {
    r.on_time_receive_packets += 1;
    pointSample(
     r.receiveQueue,
     out,
     r.receiveQueue.length,
     out.length,
     r.outputInterpolationContext
    );
  }
  else {
    // No data available, just fill the array with silence.
    // At this writing, Chrome doesn't have fill() on the output array,
    // Firefox does.
    r.delayed_receive_packets += 1;
    if ( typeof(out.fill) == "function" )
      out.fill(0, 0, length);
    else {
      for ( var i = 0; i < length; i++ ) {
        out[i] = 0;
      }
    }
  }
  old_ratio = r.delayed_receive_packets_ratio;
  r.delayed_receive_packets_ratio = Math.round((r.delayed_receive_packets
    / (r.delayed_receive_packets +  r.on_time_receive_packets))
    * 100);
    
  if ( old_ratio != r.delayed_receive_packets_ratio )
    receivingNotice();
}

// Finish transmit, start receive.
function receive()
{
  const r = window.radioclient;

  if ( r.transmitting ) {
    r.transmitting = false;

    r.delayed_receive_packets = 0;
    r.on_time_receive_packets = 0;
    r.delayed_receive_packet_ratio = 0;

    receivingNotice();

    const text = { command: "receive" };
    r.soc.send(JSON.stringify(text));

    r.in.source.disconnect(r.in.level);
    r.in.worker.disconnect(r.context.destination);
    r.out.worker.connect(r.context.destination);
    document.getElementsByClassName('TransmitButton')[0].id = 'TransmitButtonReceiving';
  }
}

// Receive JSON from the server.
function response(event)
{
  if ( event.data instanceof ArrayBuffer ) {
    responseArrayBuffer(event.data);
  }
  else if ( typeof(event.data) == 'string' ) {
    responseString(event.data);
  }
  else {
    console.log("Unhandled response type: " + typeof(event.data));
  }
}

function responseArrayBuffer(data)
{
  const r = window.radioclient;
  
  // Converts the input data to a native (floating point) array, but does not
  // convert the range, so it's still -32767..32767.
  // Appends the receive queue in what we hope is the fastest way.
  // The argument list limit is 65K, we must keep messages shorter than
  // that.
  Array.prototype.push.apply(r.receiveQueue, new Int16Array(data));
}

function responseString(data)
{
  const r = window.radioclient;

  root = JSON.parse(data);

  if ( root.frequency )
    document.getElementById('frequency').value = root.frequency.toFixed(3);
  if ( root.mode )
    document.getElementById('mode').value = root.mode;
  if ( root.lna )
    document.getElementById('lna').checked = root.lna;
  if ( root.noise )
    document.getElementById('noise').checked = root.noise;
  if ( root.mute_lo )
    document.getElementById('mute_lo').checked = root.mute_lo;
  if ( root.vga )
    document.getElementById('vga').value = root.vga;
  if ( root.if_bw )
    document.getElementById('if_bw').value = root.if_bw;
  if ( root.bpf )
    document.getElementById('bpf').value = root.bpf;
  if ( root.rssi )
    document.getElementById('rssi').innerHTML = root.rssi;
  if ( root.temp )
    document.getElementById('temp').innerHTML = root.temp;
  if ( root.voltage )
    document.getElementById('voltage').innerHTML = root.voltage;
  if ( root.locked )
    document.getElementById('locked').innerHTML = root.locked ? 'YES' : 'NO';
  if ( root.pa )
    document.getElementById('pa').checked = root.pa;
  if ( root.led )
    document.getElementById('led').checked = root.led;
}

function sendParameters()
{
  const r = window.radioclient;
  const frequency = parseFloat(document.getElementById('frequency').value);
  const mode = document.getElementById('mode').value;
  const lna = document.getElementById('lna').checked;
  const noise = document.getElementById('noise').checked;
  const mute_lo = document.getElementById('mute_lo').checked;
  const vga = document.getElementById('vga').value;
  const if_bw = document.getElementById('if_bw').value;
  const bpf = Number(document.getElementById('bpf').value);
  const pa = document.getElementById('pa').checked;
  const led = document.getElementById('led').checked;



  const text = { command: "set", frequency: frequency, mode: mode,
    lna: lna, noise: noise, mute_lo: mute_lo, vga: vga,
    if_bw: if_bw, bpf: bpf, pa: pa, led: led };
  r.soc.send(JSON.stringify(text));
  notice("Settings sent to the radio.");
}

// The remote server has disconnected.
function serverDisconnected() {
  const r = window.radioclient;

  if ( !r.reentered ) {
    window.stop();
    document.body.innerHTML =
     "The server has disconnected. "
     + "<form>"
     + "<input type='submit' value='Try Connecting Again'>"
     + "</form>";
  }
}

// Start transmit, end receive.
function transmit()
{
  const r = window.radioclient;

  notice("Transmitting.");

  if ( !r.transmitting ) {
    r.transmitting = true;
    const text = { command: "transmit" };
    r.soc.send(JSON.stringify(text));

    r.receiveQueue.length = 0;
    r.out.worker.disconnect(r.context.destination);
    r.in.source.connect(r.in.level);
    // Input processing won't start unless I do this.
    r.in.worker.connect(r.context.destination);
    document.getElementsByClassName('TransmitButton')[0].id = 'TransmitButtonTransmitting';
  }
}

// The suspend Promise has been fulfilled. Continue the shut-down operation.
function suspendFulfilled()
{
  const r = window.radioclient;

  if ( r.in.source ) {
    // This is probably unnecessary, but the web audio API isn't standardized
    // so I'm being careful.
    r.in.source.disconnect();
    r.in.source = null;
    r.in.compressor.disconnect();
    r.in.compressor = null;
    r.in.multiply.disconnect();
    r.in.multiply = null;
    r.in.worker.disconnect();
    r.in.worker = null
    r.out.worker.disconnect();
    r.out.worker = null
  }

  if ( r.context.close )
    r.context.close().then(closeFulfilled, closeRejected);
}

// Something kept the audio context from suspending.
function suspendRejected()
{
  console.log("Suspend Promise rejected.");
}

// Tear down the socket and audio graph, prior to abandoning this page.
// It may be that none of this is necessary. But neither WebSocket nor the
// web audio API is standardized as I write this.
function tearDown(event)
{
  const r = window.radioclient

  if ( r && !r.reentered ) {
    r.reentered = true;

    if ( r.soc && r.soc.readyState == WebSocket.OPEN ) {
      const text = { command: "close" };
      r.soc.send(JSON.stringify(text));
      console.log("Sent close command.");
    }

    r.disable = true	// Disable further I/O.

    if ( r.soc && (r.soc.readyState == WebSocket.OPEN || r.soc.readyState == WebSocket.CONNECTING) ) {
      console.log("Close websocket.");
      r.soc.close(1000, "Goodbye.");
    }

    if ( r.context ) {
      // AudioContext.suspend is not in IceWeasel at this writing.
      if ( r.context.suspend )
        r.context.suspend().then(suspendFulfilled, suspendRejected);
      else
        suspendFulfilled();

    }
    r.reentered = false;
  }
}

function websocketError(error) {
  notice("Didn't get the WebSocket, error " + error.name + ": " + error.message);
  serverDisconnected();
};
