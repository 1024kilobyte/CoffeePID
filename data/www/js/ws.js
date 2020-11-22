window.addEventListener('load', init);

var url;
var lastEventTime;
//var currentTemperature;
const IS_ALIVE_TIMEOUT = 10000;

// This is called when the page finishes loading
function init() {    
    // Connect to WebSocket server
    url = 'ws://' + window.location.hostname + ':81/';
    //url = 'ws://CoffeePid.local:81/';
    wsConnect(url);
    
	lastEventTime = Date.now();
	setInterval(chkConnection, IS_ALIVE_TIMEOUT/2);
}

// Call this to connect to the WebSocket server
function wsConnect(url) {
    
    console.log("Connecting to " + url);
    // Connect to WebSocket server
    websocket = new WebSocket(url);
    websocket.binaryType = 'arraybuffer';

    // Assign callbacks
    websocket.onopen = function(evt) { onOpen(evt) };
    websocket.onclose = function(evt) { onClose(evt) };
    websocket.onmessage = function(evt) { onMessage(evt) };
    websocket.onerror = function(evt) { onError(evt) };
}

function chkConnection() {
   //console.log("Check connection");
   if (Date.now() - lastEventTime > IS_ALIVE_TIMEOUT) {
       console.log("not alive, reconnecting");
       $('#state-circle').removeClass('heating-on');
       $('#state-circle').removeClass('cool-off');
       $('#currentTemp').html('--&nbsp;');

		//if (websocket.readyState === WebSocket.OPEN || websocket.readyState === WebSocket.CONNECTING) {
		if (websocket.readyState === WebSocket.OPEN) {
		  websocket.close();
		}
   }
}

// Called when a WebSocket connection is established with the server
function onOpen(evt) {

    // Log connection state
    console.log("Connected");
    	
 // Enable button
    //button.disabled = false;
    
    // Get the current state of the LED
    //doSend("getLEDState");
}

// Called when the WebSocket connection is closed
function onClose(evt) {

    // Log disconnection state
    console.log("Disconnected");
    
    $('#state-circle').removeClass('heating-on');
    $('#state-circle').removeClass('cool-off');
    $('#currentTemp').html('--&nbsp;');
	//currentTemperature = $('#currentTemp').val();

    espBootTime = null;
    espStandBy = false;

    $('#standByLabel').prop('hidden', true);
    $('#timerLabel').html('');
    currentPowerStr = '';
    $('#powerLabel').html(currentPowerStr);
	
    // Disable button
    //button.disabled = true;
    
    // Try to reconnect after a few seconds
    setTimeout(function() { wsConnect(url) }, 2000);
}

let binaryLength = 0;
//let binaryHead = 0;
let binaryFrontTime = 0;
let binaryFrontTemperature = 0.0;
let binaryLastPart = false;
let binaryTime = 0;
let binaryTemperature = 0.0;

// Called when a message is received from the server
function onMessage(evt) {
    lastEventTime = Date.now();

    if (evt.data instanceof ArrayBuffer) {
        var buffer = new Int8Array(evt.data);
        if (buffer.length === binaryLength * 5) {
            let time = binaryFrontTime;
            let temperature = binaryFrontTemperature;
            let power = 0;
            let meanPower = 0;
            let newData = temperatureData.filter(v => v.x < binaryFrontTime + espBootTime);  //left the old data
            let newPowerData = powerData.filter(v => v.time < binaryFrontTime + espBootTime);

            for (let i = 0; i < buffer.length; i += 5) {
                time += (buffer[i] & 0xFF) + ((buffer[i + 1] & 0xFF) << 8);  //unsigned int
                temperature += buffer[i + 2];
                power = buffer[i + 3] & 0xFF;
                meanPower = buffer[i + 4] & 0xFF;
                newData.push({ 'x': time + espBootTime, 'y': temperature / 100.0 });
                newPowerData.push({ 'time': time + espBootTime, 'power': power, 'meanPower': meanPower });
            }
            temperatureData = newData;
            powerData = newPowerData;
            if (binaryLastPart) chartUpdating = false;
            else {
                binaryFrontTime = time;  //for next part
                binaryFrontTemperature = temperature;
            }
        }
        return;
    }

    var obj = JSON.parse(evt.data);
    // Print out our received message
    if (obj.millis)
        console.log("Received: " + evt.data);

    if (obj.sendBinary !== undefined) {
        binaryLength = obj.sendBinary.length;
        //binaryHead = obj.sendBinary.head;
        if (obj.sendBinary.frontTime !== undefined) binaryFrontTime = obj.sendBinary.frontTime;
        if (obj.sendBinary.frontTemperature !== undefined) binaryFrontTemperature = obj.sendBinary.frontTemperature;
        binaryLastPart = obj.sendBinary.lastPart ? true : false;
    }

    if (obj.heater !== undefined) {
        $('#state-circle').removeClass('heating-on');
        //$('#state-circle').removeClass('cool-off');

        if (obj.heater) {
            $('#state-circle').addClass('heating-on');
        }
        //else {
        //$('#state-circle').addClass('cool-off');
        //}
        if (obj.power !== undefined) {
            currentPowerStr = (obj.power).toFixed(1) + '%';
            $('#powerLabel').html(currentPowerStr);
            //$('#powerLabel').prop('hidden', false);
        }
    }

    if (obj.temperature !== undefined) {
        currentTemperature = obj.temperature;
        $('#currentTemp').html(currentTemperature.toFixed(1));
        updateChartData(obj.temperature, obj.power, obj.meanPower, obj.time);
    }

    if (obj.millis !== undefined) {
        let espBootTimeNew = Date.now() - obj.millis;
        if (espBootTime === null                            //no previous time
            || espBootTimeNew < espBootTime                 //more precise time (lower latency)
            || (espBootTimeNew - espBootTime) > 10000) {    //new time (was reloaded)
            espBootTime = espBootTimeNew;
        }
    }

    if (obj.standby !== undefined) {
        espStandBy = obj.standby;
        $('#standByLabel').prop('hidden', !espStandBy);
    }

    if (obj.fault !== undefined) {
        let faultText = obj.fault.toString(16).toUpperCase();
        $('#standByLabel').html("FAULT: 0x" + faultText);
        $('#standByLabel').prop('hidden', false);
    }

    if (obj.tuning !== undefined) {
        $('#tuningLabel').prop('hidden', obj.tuning == "");
        if (obj.tuning != 0) {
            $('#tuningLabel').html("TUNING:" + obj.tuning);
        }
    }

    if (obj.target !== undefined) {
        targetTemperature = obj.target;
    }

    if (obj.new_config !== undefined) {
        getSettings();
    }
}

// Called when a WebSocket error occurs
function onError(evt) {
    console.log("ERROR: " + evt.data);
}

// Sends a message to the server (and prints it to the console)
function doSend(message) {
    console.log("Sending: " + message);
    websocket.send(message);
}


