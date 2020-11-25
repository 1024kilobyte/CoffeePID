"use strict";

// setup routing
window.addEventListener('load', appInit);
window.addEventListener('hashchange', hashChanged);

function appInit() {
    // check for hash, default to home
    var hash = window.location.hash.substr(1);
    if (hash.length === 0) {
        window.location.hash = '#home';
    } else {
        route(hash);
    }
}

function hashChanged(event) {
    var hash = window.location.hash.substr(1);
    route(hash);
}

var updateHomeTempIntervalId = null;
var updateTimerLabelIntervalId = null;

function route(hash) {
    if (updateHomeTempIntervalId !== null) clearInterval(updateHomeTempIntervalId);
    if (updateTimerLabelIntervalId !== null) clearInterval(updateTimerLabelIntervalId);
    chartVisible = (hash === 'chart');

    // animate old content -> fade out
    var oldContent = $('#contentContainer').children();
            
    if (oldContent.length > 0) {
        oldContent.fadeOut(200);
    }

    var newContent;

    switch (hash) {
        case 'home':
            newContent = $(getHomeContent()).hide();

            setTimeout(function() {
                $('#contentContainer').html(newContent);
                if (currentTemperature) $('#currentTemp').html(currentTemperature.toFixed(1));
                $('#powerLabel').html(currentPowerStr);
                $('#standByLabel').prop('hidden', !espStandBy);
                // add timer to update timer label
                updateTimerLabel();
                updateTimerLabelIntervalId = setInterval(updateTimerLabel, 1000);
                newContent.fadeIn(200);
            }, oldContent.length > 0 ? 200 : 0);
            break;
        case 'settings':
            newContent = $(getSettingsContent()).hide();

            setTimeout(function() {
                $('#contentContainer').html(newContent);
                initSettings();
                newContent.first().fadeIn(200);
            }, oldContent.length > 0 ? 200 : 0);
            break;
        case 'chart':
            newContent = $(getChartContent()).hide();
			if(currentTemperature) $('#currentTemp').html(currentTemperature.toFixed(1));
            setTimeout(function() {
                $('#contentContainer').html(newContent);
                if (currentTemperature) $('#currentTemp').html(currentTemperature.toFixed(1));
                $('#powerLabel').html(currentPowerStr);
                $('#standByLabel').prop('hidden', !espStandBy);
                getChartSettings();
                //updateTimerLabel();
                document.querySelector('#chart1').addEventListener('contextmenu', function (evt) {  //to allow chart scaling with right mouse button
                    evt.preventDefault();
                }, false);
                createChart();
                if (chartMoving) {
                    chartOptions.axisX.low = temperatureChartData.series[TemperatureSeries][0].x;
                    chartOptions.axisX.high = chartOptions.axisX.low + temperatureChartPeriodWindow;
                    updateChart(chartOptions);
                };
                newContent.fadeIn(200);
            }, oldContent.length > 0 ? 200 : 0);
           break;
        default:
            window.location.hash = '#home';
            return;
    }
}

function initSettings() {
    $('[data-toggle="tooltip"]').tooltip();
    $('input[type="number"]').inputSpinner();

    $('#wifiConfirmationModal').on('shown.bs.modal', function() {
        // FIXME: check if PW is needed, working anyway
        $('#inputClientPassword').focus();
    });
    
    $('#tempSettingsForm').submit(function(event){
        event.preventDefault();
        postSettings('temp');
    });

    $('#inputClientPassword').on('input', function (event) {
        var currentValue = $('#inputClientPassword').val();

        if (currentValue.length == 0 || currentValue.length >= 8) {
            $('#postWifiClientSettingsButton').prop('disabled', false);
        } else {
            $('#postWifiClientSettingsButton').prop('disabled', true);
        }
    });

    $('#inputApPassword').on('input', function (event) {
        var currentValue = $('#inputApPassword').val();

        if (currentValue.length == 0 || currentValue.length >= 8) {
            $('#postWifiApSettingsButton').prop('disabled', false);
        } else {
            $('#postWifiApSettingsButton').prop('disabled', true);
        }
    });
	
	getSettings();
}

function getSettings() {
    $.ajax({
        type: 'GET',
        url: '/ajax_get_settings',
        success : function(responseData) {
            if (responseData.wifi_ap_password.length > 0) {
                $('#buttonAP').html(responseData.wifi_ap_ssid + ' / ' + responseData.wifi_ap_password);
            } else {
                $('#buttonAP').html(responseData.wifi_ap_ssid);
            }

            var clientSSID = (responseData.wifi_client_ssid.length == 0) ? "Select network" : responseData.wifi_client_ssid;
            $('#buttonSSID').html(clientSSID);
            
            if (responseData.preferred_wifi_mode == 'client') {
                $('#preferredWifiModeClient').prop('checked', true);
            } else {
                $('#preferredWifiModeAP').prop('checked', true);
            }

            // change event for radio buttons only fired on check
            $('#preferredWifiModeClient, #preferredWifiModeAP').on('change', function() {
                postSettings('change_wifi_mode');
            });

            $('#inputApSSID').val(responseData.wifi_ap_ssid);
            $('#inputApPassword').val(responseData.wifi_ap_password);

            $('#inputTargetTemp').val(Number(responseData.target_temp));

            $('#inputPIDkp').val(Number(responseData.pid_kp));
            $('#inputPIDki').val(Number(responseData.pid_ki));
            $('#inputPIDkd').val(Number(responseData.pid_kd));
            $('#inputPIDdt').val(Number(responseData.pid_dt));
            $('#inputPWMperiod').val(Number(responseData.pwm_period));
        }
    });

    getChartSettings();
}

function getChartSettings() {
    chartShowTemperature = getBooleanCookie('chartShowTemperature', chartShowTemperature);
    chartShowPower = getBooleanCookie('chartShowPower', chartShowPower);
    chartShowMeanPower = getBooleanCookie('chartShowMeanPower', chartShowMeanPower);
    temperatureChartPeriodWindow = getCookie('chartPeriod', temperatureChartPeriodWindow / (60 * 1000)) * 60 * 1000;

    $('#chartShowTemperature').prop('checked', chartShowTemperature);
    $('#chartShowPower').prop('checked', chartShowPower);
    $('#chartShowMeanPower').prop('checked', chartShowMeanPower);
    $('#chartPeriod').val(temperatureChartPeriodWindow / (60 * 1000));
}

function setChartSettingsCookies() {
    const MaxAge = 60 * 60 * 24 * 365 * 10;
    setCookie('chartShowTemperature', chartShowTemperature, { 'max-age': MaxAge });
    setCookie('chartShowPower', chartShowPower, { 'max-age': MaxAge });
    setCookie('chartShowMeanPower', chartShowMeanPower, { 'max-age': MaxAge });
    setCookie('chartPeriod', temperatureChartPeriodWindow / (60 * 1000), { 'max-age': MaxAge });
    //alert(document.cookie);
    //deleteCookie('chartShowTemperature');
    //deleteCookie('chartShowPower');
    //deleteCookie('chartShowMeanPower');
    //deleteCookie('chartPeriod');
}

// *********************
// ***** SETTINGS ******
// *********************
function loadWifis() {
    $('#modalSpinner').show();
    $('#modalTable').hide();

    $('#wifiSelectionModal').modal('show');

    $.ajax({
        type: 'GET',
        url: '/ajax_get_wifis',
        success : function(responseData) {
            var modalBody = '';
            
            for (var counter = 0; counter < responseData.wifis.length; counter++) {
                var signalStrengthClass;
            
                if (responseData.wifis[counter].rssi < -73) {
                    signalStrengthClass = 'signal-poor';
                } else if (responseData.wifis[counter].rssi < -65) {
                    signalStrengthClass = 'signal-ok';
                } else {
                    signalStrengthClass = 'signal-good';
                }

                modalBody += '<tr onclick="wifiSelected(' + String(counter) + ')"><td id="ssidCol' + String(counter) + '">' + responseData.wifis[counter].ssid + '</td><td><div class="mx-auto signal-circle ' + signalStrengthClass + '"></td><td class="text-center" id="encryptionCol' + String(counter) + '">' + ((responseData.wifis[counter].encryption == 'None') ? '' : '<img src="img/lock.svg" width="20" height="20" alt="wifi encryption" />')  + '</td></tr>';
            }

            $('#wifiTableBody').html(modalBody);

            $('#modalSpinner').hide();
            $('#modalTable').show();
        },
        error: function() {
            // hide wifi selection modal
            $('#wifiSelectionModal').modal('hide');
        }
    });
}

function wifiSelected(index) {
    var ssid = $('#ssidCol' + index).html();
    var encryption = $('#encryptionCol' + index).html();

    // hide wifi selection modal
    $('#wifiSelectionModal').modal('hide');

    $('#inputClientSSID').val(ssid);
    $('#inputClientPassword').val('');

    // show / hide password field
    if (encryption.length > 0) {
        $('#passwordRegion').removeClass('d-none');
    } else {
        $('#passwordRegion').addClass('d-none');
    }

    // show pw / confirmation modal
    $('#wifiConfirmationModal').modal('show');
}

function showAPModeDialog() {
    $('#wifiCredentialsModal').modal('show');
}

function postSettings(section) {
    var dataString = '';
	
    switch (section) {
        case 'wifi_client':
            dataString += 'wifi_client_ssid=' + $('#inputClientSSID').val();
            dataString += '&wifi_client_password=' + $('#inputClientPassword').val();

            $('#wifiConfirmationModal').modal('hide');
            break;
        case 'wifi_ap':
            dataString += 'wifi_ap_ssid=' + $('#inputApSSID').val();
            dataString += '&wifi_ap_password=' + $('#inputApPassword').val();

            $('#wifiCredentialsModal').modal('hide');
            break;
        case 'temp':
            dataString += 'target_temp=' + $('#inputTargetTemp').val();
            break;
        case 'reboot':
            dataString += 'reboot=true';
            break;
        case 'change_wifi_mode':
            dataString += 'preferred_wifi_mode=' + ($('#preferredWifiModeClient').prop('checked') ? 'client' : 'ap');
            break;
        case 'pid':
            dataString += 'pid_kp=' + $('#inputPIDkp').val();
            dataString += '&pid_ki=' + $('#inputPIDki').val();
            dataString += '&pid_kd=' + $('#inputPIDkd').val();
            dataString += '&pid_dt=' + $('#inputPIDdt').val();
            dataString += '&pwm_period=' + $('#inputPWMperiod').val();
            break;
        case 'pid_tune':
            dataString += 'start_pid_tune=1';
            break;
        case 'chart':
            chartShowTemperature = $('#chartShowTemperature').prop('checked');
            chartShowPower = $('#chartShowPower').prop('checked');
            chartShowMeanPower = $('#chartShowMeanPower').prop('checked');
            temperatureChartPeriodWindow = $('#chartPeriod').val() * 60 * 1000;
            setChartSettingsCookies();
            break;
   }

    $.ajax({
        type: 'POST',
        url: '/ajax_set_settings',
        data: dataString,
        success : function(text) {
            // as the user may want to reboot additionally, don't redirect automatically
            /*
            if (section != 'change_wifi_mode') {
                window.location.hash = '#home';
            }
            */
        }
    });
}

// *********************
// ******* HOME ********
// *********************
var espBootTime = null;
var espStandBy = false;

function updateTimerLabel() {
    if (espBootTime != null) {
        var elapsedTime = (Date.now() - espBootTime);

        var timerText = msToTime(elapsedTime);

        $('#timerLabel').html(timerText);
    }
}

function msToTime(s) {
    var ms = s % 1000;
    s = (s - ms) / 1000;
    var secs = s % 60;
    s = (s - secs) / 60;
    var mins = s;
  
    return mins + ':' + ('0' + String(secs)).slice(-2);
}



// *********************
// ******* CHART *******
// *********************

var currentTemperature = null;
var currentPowerStr = '';
var targetTemperature = null;
var temperatureChart = null;


var chartShowTemperature = true;
var chartShowPower = false;
var chartShowMeanPower = false;

const PowerSeries = 0
const MeanPowerSeries = 1
const TemperatureSeries = 2
var temperatureChartData = {
    series: []   //temperature, power, meanPower
}
var powerData = new Array;
var temperatureData = new Array;
var chartUpdating = false;
var chartMoving = false;
var chartLive = true;
var chartVisible = false;

let temperatureChartPeriod = 60 * 60 * 1000; //msec
let temperatureChartPeriodWindow = 20 * 60 * 1000; //msec
let temperatureChartStartX = 0;
let temperatureChartRealWidth = 0;
let scaleAnchor = 0; //to keep the same points when scaling

var responsiveOptions = [
  ['screen and (max-width: 640px)', {
    maxLabels: 4,
  }],
  ['screen and (min-width: 641px) and (max-width: 1024px)', {
    maxLabels: 6,
  }],
  ['screen and (min-width: 1025px)', {
    maxLabels: 10,
  }],
];

var chartOptions = {  
	  // Don't draw the line chart points
	  showPoint: false,
	  // line smoothing
      lineSmooth: Chartist.Interpolation.none({
        fillHoles: false
      }),
	  maxLabels: 5,
	  // X-Axis specific configuration
	  axisX: {
	    type: Chartist.FixedScaleAxis,
        low: new Date().getTime(),
		high: new Date().getTime() + temperatureChartPeriodWindow,
		showGrid: true,
		showLabel: true,
		offset: 20,
		labelInterpolationFnc: function (value, index, labels) {
		  if(index === 0) return '';
		  if(index === labels.length-1) return '';
		  let d = new Date(value);
		  if(labels.length > chartOptions.maxLabels) {
              let scale = Math.round(labels.length / chartOptions.maxLabels);
              let minutes = d.getHours()*60 + d.getMinutes();
              if (minutes % scale !== 0) return '';
		  }
		  let label = d.getHours() + ':' + ('0' + String(d.getMinutes())).slice(-2);
          return label;
		},
	  },
	  // Y-Axis specific configuration	
      axisY: {
	    //type: Chartist.FixedScaleAxis,
 		onlyInteger: true,
        low: 20,
		high: 100,
		scaleMinSpace: 30,
		// Lets offset the chart a bit from the labels
		offset: 40,
		labelInterpolationFnc: function(value) {
            if (chartShowTemperature) {
                return value + '°С';
            }
            else {
                return value + '%';
            }
		}
	  },
	  targetLine: {
		value: 0,
		class: 'ct-target-line'
	  }
	};


function updateChartData(temperature, power, meanPower, time) {
    if (espBootTime == null) return;
    if (chartUpdating) return;

    let tp = {};
    tp.x = time + espBootTime;
    tp.y = temperature;

    let pp = {};
    pp.time = tp.x;
    pp.power = power;
    pp.meanPower = meanPower;

    if (temperatureData.length == 0) {
        let time = tp.x - temperatureChartPeriod - espBootTime;
        if (time < 0) time = 0;
        let obj = { get: { history: { from: time, to: 0 } } };
        chartUpdating = true;
        setTimeout(function () { chartUpdating = false; }, 20000);
        doSend(JSON.stringify(obj));
        return;
    }
    else {
        let lastX = temperatureData[temperatureData.length - 1].x;
        if (tp.x - lastX > 1500) {
            let obj = { get: { history: { from: lastX - espBootTime, to: 0 } } };
            chartUpdating = true;
            setTimeout(function () {
                if (chartUpdating === true) {
                    temperatureData.sort((a, b) => a.x - b.x);
                    powerData.sort((a, b) => a.time - b.time);
                    chartUpdating = false;
                }
            }, 10000);
            doSend(JSON.stringify(obj));
            return;
        }
    }

    chartOptions.targetLine.value = targetTemperature;

    temperatureData.push(tp);
    powerData.push(pp);
    while (temperatureData[0].x < tp.x - 2 * temperatureChartPeriod) {
        temperatureData.shift();
        powerData.shift();
        scaleAnchor = scaleAnchor + 1;
    }

    if (!chartMoving && chartVisible) updateChart(chartOptions);
}

function updateChart(chartOptions) {
    let startX = temperatureData[0].x;
    let lastX = temperatureData[temperatureData.length - 1].x;
    let scaleX = Math.round((temperatureChartPeriodWindow / 1000) / temperatureChartRealWidth);
    if (scaleX === 0) scaleX = 1;

    let lowX = chartOptions.axisX.low;
    let highX = chartOptions.axisX.high; 
    if (!chartMoving) {
        lowX = lastX - temperatureChartPeriodWindow;
        if (lowX < startX) lowX = startX;
        highX = lowX + temperatureChartPeriodWindow;
    }

    let lowY = 0;
    let highY = 100;

    temperatureChartData.series[TemperatureSeries] = [];
    temperatureChartData.series[PowerSeries] = [];
    temperatureChartData.series[MeanPowerSeries] = [];

    if (chartShowTemperature) {
        temperatureChartData.series[TemperatureSeries] = temperatureData.filter((tp, i) => tp.x >= lowX && tp.x <= highX && (i + scaleAnchor) % scaleX === 0);

        //let highLow = Chartist.getHighLow(temperatureData, {}, 'y');   //Scale all or just visible window?
        let highLow = Chartist.getHighLow(temperatureChartData.series[TemperatureSeries], {}, 'y');
        highY = Math.ceil(highLow.high) + 1;
        lowY = Math.floor(highLow.low) - 1;
        highY = Math.ceil(highY / 10) * 10;
        lowY = Math.floor(lowY / 10) * 10;
    }

    if (chartShowPower || chartShowMeanPower) {
        let k = (highY - lowY) / 255;
        let data = powerData.filter((pp, i) => pp.time >= lowX && pp.time <= highX && (i + scaleAnchor) % scaleX === 0);

        if (chartShowPower)
            temperatureChartData.series[PowerSeries] = data.map(function (pp) { return { 'x': pp.time, 'y': lowY + pp.power * k }; });

        data = data.filter((pp) => pp.time >= lowX + 30000);
        if (chartShowMeanPower)
            temperatureChartData.series[MeanPowerSeries] = data.map(function (pp) { return { 'x': pp.time - 30000, 'y': lowY + pp.meanPower * k }; });
    }

    chartOptions.axisX.low = lowX;
    chartOptions.axisX.high = highX;
    chartOptions.axisY.high = highY;
    chartOptions.axisY.low = lowY;
    setTicks(chartOptions);
    if (temperatureChart != null) temperatureChart.update(temperatureChartData, chartOptions);
}

function setTicks(chartOptions) {
    chartOptions.axisX.ticks = [];
    chartOptions.axisX.ticks.push(chartOptions.axisX.low);
    let tick = new Date(chartOptions.axisX.low);
    tick.setSeconds(0);
    for (let t = tick.getTime() + 60 * 1000; t <= chartOptions.axisX.high; t += 60 * 1000) {
        chartOptions.axisX.ticks.push(t);
    }
    chartOptions.axisX.ticks.push(chartOptions.axisX.high);
}

function createChart() {
    temperatureChart = new Chartist.Line('.ct-chart-1', temperatureChartData, chartOptions, responsiveOptions);
    var optionsProvider = Chartist.optionsProvider(chartOptions, responsiveOptions);

    chartOptions.maxLabels = optionsProvider.getCurrentOptions().maxLabels;
    temperatureChart.on('optionsChanged', function () {
        chartOptions.maxLabels = optionsProvider.getCurrentOptions().maxLabels;
    });

    function projectY(chartRect, bounds, value) {
        return chartRect.y1 -
            (chartRect.height() * (value - bounds.min) / bounds.range);
    }

    /*var defaultOptions = {
        // onZoom
        resetOnRightMouseBtn: true,
        pointClipOffset: 5,
        zoomActivationThreshold: { x: 5, y: 5 },
    };

    var options = Chartist.extend({}, defaultOptions, chartOptions);
    var threshold = options.zoomActivationThreshold;
    options.zoomActivationThreshold =
        typeof options.zoomActivationThreshold === 'number' ? { x: threshold, y: threshold } : threshold;
    */

    var svg, axisX, /*axisY,*/ chartRect;
    var downPosition;
    var initialChartSize;
    var buttonPressed;
    //var onZoom = options.onZoom;
    var ongoingTouches = [];

    temperatureChart.on('draw', function (data) {
        var type = data.type;
        if (type === 'line' || type === 'bar' || type === 'area') {
            data.element.attr({ 'clip-path': 'url(#line-mask)' });
        }
    });

    temperatureChart.on('created', function (data) {
        temperatureChartRealWidth = data.chartRect.x2 - data.chartRect.x1;

        let targetLineY = projectY(data.chartRect, data.bounds, data.options.targetLine.value);
        data.svg.elem('line', {           //draw target temperature line
            x1: data.chartRect.x1,
            x2: data.chartRect.x2,
            y1: targetLineY,
            y2: targetLineY
        }, data.options.targetLine.class);

        axisX = data.axisX;
        //axisY = data.axisY;
        chartRect = data.chartRect;
        svg = data.svg._node;
 
        function on(event, handler) {
            svg.addEventListener(event, handler);
        }

        on('mousedown', onMouseDown);
        on('mouseup', onMouseUp);
        on('mousemove', onMouseMove);
        on('touchstart', onTouchStart);
        on('touchmove', onTouchMove);
        on('touchend', onTouchEnd);
        on('touchcancel', onTouchCancel);
    });

    function onMouseDown(event) {
        event.preventDefault();
        if (event.button === 0 || event.button === 2) {
            var point = position(event, svg);
            if (isInRect(point, chartRect)) {
                downPosition = point;
                buttonPressed = event.button;
                startMove();
            }
        }
    }

    function onMouseUp(event) {
        if ((event.button === 0 || event.button === 2) && downPosition) {
            endMove();
        }
    }

    function onMouseMove(event) {
        if (downPosition) {
            var point = position(event, svg);
            if (isInRect(point, chartRect)) {
                moveChart(point);
                event.preventDefault();
            }
            else onMouseUp(event);
        }
    }

    function onTouchStart(event) {
        event.preventDefault();
        var touches = event.changedTouches;
        for (var i = 0; i < touches.length; i++) {
            ongoingTouches.push(copyTouch(touches[i]));
        }
        if (!downPosition) downPosition = ongoingTouches[0];
        startMove();  //to start on second touch too, if not started 
    }

    function onTouchMove(event) {
        event.preventDefault();
        var touches = event.changedTouches;
        for (var i = 0; i < touches.length; i++) {
            var idx = ongoingTouchIndexById(touches[i].identifier);
            ongoingTouches.splice(idx, 1, copyTouch(touches[i]));
            if (idx === 0) {
                if (event.scale != 1) {
                    moveChart(downPosition, event.scale);
                }
                else moveChart(ongoingTouches[0]);
            }
        }
    }

    function onTouchEnd(event) {
        removeTouches(event.changedTouches);
        if (ongoingTouches.length === 0) {
            endMove();
        }
    }

    function onTouchCancel(event) {
        onTouchEnd(event);
    }

    function startMove() {
        initialChartSize = temperatureChartPeriodWindow;
        let startX = temperatureData[0].x;
        let lastX = temperatureData[temperatureData.length - 1].x;
        if (!chartMoving && (buttonPressed !== 0 || ongoingTouches.length > 1 || lastX - startX > temperatureChartPeriodWindow))
            chartMoving = true;
    }

    function endMove() {
        downPosition = null;
        if (chartMoving && chartLive)
            chartMoving = false;
        setChartSettingsCookies();
    }

    function moveChart(point, scale) {
        let distance = point.x - downPosition.x;
        if (Math.abs(distance) > 5 || scale) {
            let startX = temperatureData[0].x;
            let lastX = temperatureData[temperatureData.length - 1].x;
            if (chartMoving) {
                let distanceX = project(point.x, axisX) - project(downPosition.x, axisX);
                temperatureChart.options.axisX.low -= distanceX;
                if (scale) {
                    let currentSize = temperatureChartPeriodWindow;
                    temperatureChartPeriodWindow = initialChartSize / scale;
                    temperatureChart.options.axisX.low -= temperatureChartPeriodWindow - currentSize;
                }
                else if (buttonPressed === 0 || ongoingTouches.length === 1)
                    temperatureChart.options.axisX.high -= distanceX;  //left button - move, right - scale
                else temperatureChartPeriodWindow += distanceX;
                let offsetStartX = temperatureChart.options.axisX.low - startX;  //distances from end points to chart bounds
                let offsetLastX = temperatureChart.options.axisX.high - lastX;
                chartLive = false;
                if (lastX - startX < temperatureChartPeriodWindow) {
                    temperatureChart.options.axisX.low = startX;
                    temperatureChart.options.axisX.high = startX + temperatureChartPeriodWindow;
                    chartLive = true;
                }
                else if (offsetStartX < 0) { //do not move beyond data
                    temperatureChart.options.axisX.low = startX;
                    temperatureChart.options.axisX.high -= offsetStartX;
                }
                else if (offsetLastX >= -1500) { //do not move beyond data and stay live
                    temperatureChart.options.axisX.low -= offsetLastX;
                    temperatureChart.options.axisX.high = lastX;
                    chartLive = true;
                }
                updateChart(temperatureChart.options);
            }
            downPosition = point;
        }
    }

    function copyTouch(touch) {
        var p = position(touch, svg);
        p.id = touch.identifier;
        return p;
    }

    function ongoingTouchIndexById(idToFind) {
        for (var i = 0; i < ongoingTouches.length; i++) {
            var id = ongoingTouches[i].id;
            if (id === idToFind) {
                return i;
            }
        }
        return -1;
    }

    function removeTouches(touches) {
        for (var i = 0; i < touches.length; i++) {
            var idx = ongoingTouchIndexById(touches[i].identifier);
            if (idx >= 0) {
                ongoingTouches.splice(idx, 1);
            }
        }
    }

    function isInRect(point, rect) {
        return point.x >= rect.x1 && point.x <= rect.x2 && point.y >= rect.y2 && point.y <= rect.y1;
    }

    function position(event, svg) {
        return transform(event.clientX, event.clientY, svg);
    }

    function transform(x, y, svgElement) {
        var svg = svgElement.tagName === 'svg' ? svgElement : svgElement.ownerSVGElement;
        var matrix = svg.getScreenCTM();
        var point = svg.createSVGPoint();
        point.x = x;
        point.y = y;
        point = point.matrixTransform(matrix.inverse());
        return point || { x: 0, y: 0 };
    }

    function project(value, axis) {
        var bounds = axis.bounds || axis.range;
        var max = bounds.max;
        var min = bounds.min;
        if (axis.scale && axis.scale.type === 'log') {
            var base = axis.scale.base;
            return Math.pow(base,
                value * baseLog(max / min, base) / axis.axisLength) * min;
        }
        var range = bounds.range || (max - min);
        return (value * range / axis.axisLength) + min;
    }

    function baseLog(val, base) {
        return Math.log(val) / Math.log(base);
    }
}


// *********************
// ******* COOKIES *****
// *********************

function getCookie(name, defaultValue) {
    let matches = document.cookie.match(new RegExp(
        "(?:^|; )" + name.replace(/([\.$?*|{}\(\)\[\]\\\/\+^])/g, '\\$1') + "=([^;]*)"
    ));
    return matches ? decodeURIComponent(matches[1]) : defaultValue;
}

function getBooleanCookie(name, defautValue) {
    let cookie = stringToBoolean(getCookie(name, undefined));
    return (cookie !== undefined) ? cookie : defautValue;
}

function stringToBoolean(string) {
    if (string === undefined) return undefined;
    switch (string.toLowerCase()) {
        case "false": case "no": case "0": case "": return false;
        default: return true;
    }
}

function setCookie(name, value, options = {}) {
    options = {
        path: '/',
        ...options
    };

    if (options.expires instanceof Date) {
        options.expires = options.expires.toUTCString();
    }

    let updatedCookie = encodeURIComponent(name) + "=" + encodeURIComponent(value);

    for (let optionKey in options) {
        updatedCookie += "; " + optionKey;
        let optionValue = options[optionKey];
        if (optionValue !== true) {
            updatedCookie += "=" + optionValue;
        }
    }

    document.cookie = updatedCookie;
}

function deleteCookie(name) {
    setCookie(name, "11", {
        'max-age': -1
    })
}


// *********************
// **** VISIBILITY *****
// *********************

// Set the name of the hidden property and the change event for visibility
var hidden, visibilityChange;
if (typeof document.hidden !== "undefined") { // Opera 12.10 and Firefox 18 and later support 
    hidden = "hidden";
    visibilityChange = "visibilitychange";
} else if (typeof document.msHidden !== "undefined") {
    hidden = "msHidden";
    visibilityChange = "msvisibilitychange";
} else if (typeof document.webkitHidden !== "undefined") {
    hidden = "webkitHidden";
    visibilityChange = "webkitvisibilitychange";
}

function handleVisibilityChange() {
    chartVisible = (!document[hidden] && window.location.hash === '#chart');
}

// Warn if the browser doesn't support addEventListener or the Page Visibility API
if (typeof document.addEventListener === "undefined" || hidden === undefined) {
    console.log("Visibility requires a browser, such as Google Chrome or Firefox, that supports the Page Visibility API.");
} else {
    // Handle page visibility change   
    document.addEventListener(visibilityChange, handleVisibilityChange, false);
}