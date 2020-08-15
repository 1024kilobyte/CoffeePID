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

function route(hash) {
    if (updateHomeTempIntervalId !== null) clearInterval(updateHomeTempIntervalId);

    // animate old content -> fade out
    var oldContent = $('#contentContainer').children(':first');
            
    if (oldContent.length > 0) {
        oldContent.fadeOut(200);
    }

    var newContent;

    switch (hash) {
        case 'home':
            newContent = $(getHomeContent()).hide();

            setTimeout(function() {
                $('#contentContainer').html(newContent);
                updateHomeTemp();
                // and repeat every 750 ms
                updateHomeTempIntervalId = setInterval(updateHomeTemp, 750);
                newContent.first().fadeIn(200);
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

    $.ajax({
        type: "GET",
        url: "/ajax_get_settings",
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
        }
    });
}

// *********************
// ***** SETTINGS ******
// *********************
function loadWifis() {
    $('#modalSpinner').show();
    $('#modalTable').hide();

    $('#wifiSelectionModal').modal('show');

    $.ajax({
        type: "GET",
        url: "/ajax_get_wifis",
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
function updateHomeTemp() {
    $.ajax({
        type: "GET",
        url: "/ajax_get_temp",
        success : function(tempString){
            var resultComponents = tempString.split('|');

            var currentTemperature = Number(resultComponents[1]);
            $('#currentTemp').html(currentTemperature.toFixed(1));

            // not needed for now
            // var targetTemperature = Number(resultComponents[0]);

            $('#state-circle').removeClass('heating-on');
            $('#state-circle').removeClass('cool-off');

            // be aware: boolean(string) is always true
            var isHeating = Boolean(Number(resultComponents[2]));

            if (isHeating) {
                $('#state-circle').addClass('heating-on');
            } else {
                $('#state-circle').addClass('cool-off');
            }
        },
        error: function() {
            // TODO
        }
    });
}