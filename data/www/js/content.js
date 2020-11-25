function getHomeContent() {
  return `
  <div class="rounded my-auto" style="background: rgba(46, 51, 56, 0.7)!important;">
    <h1 class="row" style="margin: 8px;"><img src="img/water-temp.svg" alt="Hot water" width="50" height="50" />&nbsp;&nbsp;<span id="currentTemp">--&nbsp;</span>&deg;C&nbsp;<div id="state-circle" class="circle"></div></h1>
  </div>
  <div style="bottom: 8px; left: 8px; background: rgba(46, 51, 56, 0.7)!important;" class="position-absolute rounded my-auto">
    <span hidden id="tuningLabel" style="margin: 8px; font-size: x-large; font-weight: bold; color: blue;">TUNING</span>
  </div>
  <div style="bottom: 8px; right: 8px; background: rgba(46, 51, 56, 0.7)!important;" class="position-absolute rounded my-auto">
    <span hidden id="standByLabel" style="margin: 8px; font-size: x-large; font-weight: bold; color: crimson;">STANDBY</span>
    <span id="timerLabel" style="margin: 8px; font-family: monospace; font-size: xx-large;"></span>
    <!--
    <span id="powerLabel" style="margin: 8px; font-family: monospace; font-size: xx-large;"></span>
 	-->    
  </div>
  `;
}

function getSettingsContent() {
    return `
        <div class="card text-white my-auto col-lg-6 col-md-6 col-sm-7 col-11 px-0" style="background: rgba(46, 51, 56, 0.7)!important;">
        <div class="card-header pb-1"><h3>Settings</h3></div>
        <div class="card-body p-2 pt-1">
          <!-- Nav tabs -->
          <ul class="nav nav-tabs nav-justified">
            <li class="nav-item">
              <a class="nav-link active" data-toggle="tab" href="#temp"><img src="img/thermo.svg" width="34" height="20" alt="target water temperature" /></a>
            </li>
            <li class="nav-item">
              <a class="nav-link" data-toggle="tab" href="#pid">PID</a>
            </li>
             <li class="nav-item">
              <a class="nav-link p-0" data-toggle="tab" href="#chart"><img src="img/chart.svg" alt="Chart" width="34" height="40" /></a>
            </li>
            <li class="nav-item">
              <a class="nav-link" data-toggle="tab" href="#wifi">WiFi</a>
            </li>
          </ul>
          <!-- Tab panes -->
          <div class="tab-content">
            <div class="tab-pane active mt-3" id="temp">
              <form id="tempSettingsForm">
                <div class="flex-row d-flex mx-1 mb-2">
                  <h5 class="flex-grow-1 mb-0">Brewing</h5>
                  <div><span class="badge badge-pill badge-light font-weight-bold my-auto" data-toggle="tooltip" data-placement="left" title="Set the target brewing temperature for your coffee">i</span></div>
                </div>
                <div class="form-group form-row">
                  <div class="col-2 my-auto text-center">
                    <img src="img/thermo.svg" width="34" height="34" alt="target water temperature" />
                  </div>
                  <div class="col-10">
                    <input type="number" data-suffix="°C" min="0" max="160" value="0" step="1" class="form-control-lg" id="inputTargetTemp" aria-label="coffee target temperature">
                  </div>
                </div>
                <button type="submit" class="btn btn-success btn-block btn-lg">Save changes</button>
              </form>
            </div>
            <div class="tab-pane mt-3 fade" id="wifi">
              <form id="wifiSettingsForm">
                <div class="flex-row d-flex mx-1 mb-2">
                  <div class="form-check flex-grow-1 mb-0">
                    <label class="form-check-label">
                      <input type="radio" class="form-check-input radio-inline mt-1" id="preferredWifiModeClient" name="wifimode">
                      <h5 class="ml-1">Connect to WiFi</h5>
                    </label>
                  </div>
                  <div><span class="badge badge-pill badge-light font-weight-bold my-auto" data-toggle="tooltip" data-placement="left" title="Set the wifi the CoffeePID connects to">i</span></div>
                </div>
                <div class="form-group form-row">
                  <div class="col-2 my-auto text-center">
                    <img src="img/wifi.svg" width="32" height="32" alt="wifi name" />
                  </div>
                  <div class="col-10">
                    <button type="button" class="btn btn-secondary btn-block btn-lg" id="buttonSSID" onclick="loadWifis()" aria-label="name of the wifi">SSID</button>
                  </div>
                </div>
                <hr class="bg-light">
                <div class="flex-row d-flex mx-1 mb-2">
                  <div class="form-check flex-grow-1 mb-0">
                    <label class="form-check-label">
                      <input type="radio" class="form-check-input radio-inline mt-1" id="preferredWifiModeAP" name="wifimode">
                      <h5 class="ml-1">Accesspoint mode</h5>
                    </label>
                  </div>
                  <div><span class="badge badge-pill badge-light font-weight-bold my-auto" data-toggle="tooltip" data-placement="left" title="Set the wifi name and password for accesspoint mode">i</span></div>
                </div>
                <div class="form-group form-row">
                  <div class="col-2 my-auto text-center">
                    <img src="img/router.svg" width="32" height="32" alt="accesspoint mode" />
                  </div>
                  <div class="col-10">
                    <button type="button" class="btn btn-secondary btn-block btn-lg" id="buttonAP" onclick="showAPModeDialog()" aria-label="name of the wifi">SSID</button>
                  </div>
                </div>
                <hr class="bg-light">
                <div class="flex-row d-flex mx-1">
                  <div class="flex-grow-1">
                    <button type="button" class="btn btn-danger btn-block btn-lg" id="buttonReboot" onclick="postSettings('reboot')" aria-label="reboot">reboot</button>
                  </div>
                </div>
              </form>
            </div>
            <div class="tab-pane mt-3 fade" id="pid">
              <form id="pidSettingsForm">
                <div class="flex-row d-flex mx-1 mb-2">
                   <h5 class="flex-grow-1 mb-0">PID parameters</h5>
                 <div><span class="badge badge-pill badge-light font-weight-bold my-auto" data-toggle="tooltip" data-placement="left" title="Set the PID parameters">i</span></div>
                </div>
                <div class="form-group form-row">
                  <div class="col-2">
                    <h5 class="modal-title">Kp</h5>
                  </div>
                  <div class="col-10">
                    <input type="number" data-decimals="2" min="0" max="1000" value="11" step="0.1" class="form-control-lg" id="inputPIDkp" aria-label="PID Kp">
                  </div>
                  <div class="col-2">
                    <h5 class="modal-title">Ki</h5>
                  </div>
                  <div class="col-10">
                    <input type="number" data-decimals="2" min="0" max="1000" value="1" step="0.1" class="form-control-lg" id="inputPIDki" aria-label="PID Ki">
                  </div>
                  <div class="col-2">
                    <h5 class="modal-title">Kd</h5>
                  </div>
                  <div class="col-10">
                    <input type="number" data-decimals="2" min="0" max="1000" value="0" step="0.1" class="form-control-lg" id="inputPIDkd" aria-label="PID Kd">
                  </div>
                  <div class="col-2">
                    <h5 class="modal-title">dt</h5>
                  </div>
                  <div class="col-10">
                    <input type="number" min="100" max="100000" value="100" step="1" class="form-control-lg" id="inputPIDdt" aria-label="PID dt">
                  </div>
                </div>
                <div class="flex-row d-flex mx-1 mb-2">
                   <h5 class="flex-grow-1 mb-0">PWM period</h5>
                 <div><span class="badge badge-pill badge-light font-weight-bold my-auto" data-toggle="tooltip" data-placement="left" title="Set the PWM period">i</span></div>
                </div>
                <div class="form-group form-row">
                   <div class="col-10">
                    <input type="number" min="1000" max="100000" value="1000" step="1" class="form-control-lg" id="inputPWMperiod" aria-label="PWM period">
                  </div>
                </div>
                <div class="flex-row d-flex">
                  <button type="button" class="btn btn-success btn-lg" onclick="postSettings('pid')">Save</button>
				  <button type="button" class="btn btn-danger btn-lg flex-grow-1" onclick="postSettings('pid_tune')">Autotune</button>
                </div>
              </form>
            </div>
            <div class="tab-pane mt-3 fade" id="chart">
              <form id="chartSettingsForm">
                <div class="flex-row d-flex mx-1 mb-2">
                   <h5 class="flex-grow-1 mb-0">Chart</h5>
                </div>
                <div class="flex-row d-flex mx-1 mb-2">
                  <div class="form-check flex-grow-1 mb-0">
                    <label class="form-check-label">
                      <input type="checkbox" class="form-check-input radio-inline mt-1" id="chartShowTemperature" onchange="postSettings('chart')">
                      <h5 class="ml-1">Temperature</h5>
                    </label>
                  </div>
                  <div><span class="badge badge-pill badge-light font-weight-bold my-auto" data-toggle="tooltip" data-placement="left" title="Show temperature plot">i</span></div>
                </div>
                <div class="flex-row d-flex mx-1 mb-2">
                  <div class="form-check flex-grow-1 mb-0">
                    <label class="form-check-label">
                      <input type="checkbox" class="form-check-input radio-inline mt-1" id="chartShowPower" onchange="postSettings('chart')">
                      <h5 class="ml-1">Power</h5>
                    </label>
                  </div>
                  <div><span class="badge badge-pill badge-light font-weight-bold my-auto" data-toggle="tooltip" data-placement="left" title="Show percentage of output power">i</span></div>
                </div>
                <div class="flex-row d-flex mx-1 mb-2">
                  <div class="form-check flex-grow-1 mb-0">
                    <label class="form-check-label">
                      <input type="checkbox" class="form-check-input radio-inline mt-1" id="chartShowMeanPower" onchange="postSettings('chart')">
                      <h5 class="ml-1">Average power</h5>
                    </label>
                  </div>
                  <div><span class="badge badge-pill badge-light font-weight-bold my-auto" data-toggle="tooltip" data-placement="left" title="Show one minute moving average of output power">i</span></div>
                </div>
                <hr class="bg-light">
                <div class="flex-row d-flex mx-1 mb-2">
                   <h5 class="flex-grow-1 mb-0">Number of minutes to show</h5>
                </div>
                <div class="form-group form-row">
                   <div class="col-12">
                    <input type="number" min="1" max="180" value="20" step="1" class="form-control-lg" id="chartPeriod" onchange="postSettings('chart')">
                  </div>
                </div>
              </form>
            </div>
          </div>
        </div>
      
        <!-- Select WiFi Modal -->
        <div class="modal fade" id="wifiSelectionModal" tabindex="-1" role="dialog" aria-hidden="true" aria-labelledby="selectionModalTitle">
          <div class="modal-dialog" role="document">
            <div class="modal-content bg-dark">
              <div class="modal-header">
                <h5 class="modal-title" id="selectionModalTitle">Select Wifi</h5>
                <button type="button" class="close" data-dismiss="modal" aria-label="Close">
                  <span style="color: white;" aria-hidden="true">&times;</span>
                </button>
              </div>
              <div class="modal-body p-0">
                <div id="modalSpinner" class="text-center p-5">
                  <div style="width: 3rem; height: 3rem;" class="spinner-grow text-info" role="status" aria-hidden="true"><span class="sr-only">Loading...</span>
                  </div>
                </div>
                <table id="modalTable" class="table table-striped table-condensed mb-0 text-light">
                  <thead>
                    <tr>
                      <th scope="col-8">Name</th>
                      <th class="text-center" scope="col-2">Signal</th>
                      <th class="text-center" scope="col-2">Encr.</th>
                    </tr>
                  </thead>
                  <tbody id="wifiTableBody">
                  </tbody>
                </table>
              </div>
            </div>
          </div>
        </div>
        <!-- Confirm WiFi Modal -->
        <div class="modal fade" id="wifiConfirmationModal" tabindex="-1" role="dialog" aria-hidden="true" aria-labelledby="confirmationModalTitle">
          <div class="modal-dialog" role="document">
            <div class="modal-content bg-dark">
              <div class="modal-header">
                <h5 class="modal-title" id="confirmationModalTitle">Enter Password</h5>
                <button type="button" class="close" data-dismiss="modal" aria-label="Close">
                  <span style="color: white;" aria-hidden="true">&times;</span>
                </button>
              </div>
              <div class="modal-body">
                <form>
                  <div class="form-group form-row">
                    <div class="col-2 my-auto text-center">
                      <img src="img/wifi.svg" width="32" height="32" alt="wifi name" />
                    </div>
                    <div class="col-10">
                      <input disabled type="text" autocomplete="username" class="form-control" id="inputClientSSID" aria-label="wifi name" >
                    </div>
                  </div>
                  <div id="passwordRegion" class="form-group form-row d-none">
                    <div class="col-2 my-auto text-center">
                      <img src="img/lock.svg" width="30" height="30" alt="wifi password" />
                    </div>
                    <div class="col-10">
                      <input type="password" class="form-control" autocomplete="current-password" id="inputClientPassword" aria-label="wifi password" placeholder="Enter Password">
                    </div>
                  </div>
                </form>
              </div>
              <div class="modal-footer">
                <button type="button" id="postWifiClientSettingsButton" class="btn btn-success btn-block btn-lg" onclick="postSettings('wifi_client')">Save changes</button>
              </div>
            </div>
          </div>
        </div>
        <!-- Set AP Credentials Modal -->
        <div class="modal fade" id="wifiCredentialsModal" tabindex="-1" role="dialog" aria-hidden="true" aria-labelledby="confirmationModalTitle">
          <div class="modal-dialog" role="document">
            <div class="modal-content bg-dark">
              <div class="modal-header">
                <h5 class="modal-title" id="confirmationModalTitle">Accesspoint mode</h5>
                <button type="button" class="close" data-dismiss="modal" aria-label="Close">
                  <span style="color: white;" aria-hidden="true">&times;</span>
                </button>
              </div>
              <div class="modal-body">
                <form>
                  <div class="form-group form-row">
                    <div class="col-2 my-auto text-center">
                      <img src="img/wifi.svg" width="32" height="32" alt="wifi name" />
                    </div>
                    <div class="col-10">
                      <input type="text" autocomplete="username" class="form-control" id="inputApSSID" aria-label="wifi name" >
                    </div>
                  </div>
                  <div class="form-group form-row mb-2">
                    <div class="col-2 my-auto text-center">
                      <img src="img/lock.svg" width="30" height="30" alt="wifi password" />
                    </div>
                    <div class="col-10">
                      <input type="password" class="form-control" autocomplete="current-password" id="inputApPassword" aria-label="wifi password" placeholder="Enter Password">
                    </div>
                  </div>
                  <div class="form-group form-row mb-0">
                    <div class="col-2">
                      &nbsp;
                    </div>
                    <div class="col-10 my-auto">
                      A valid wifi password has at least 8 characters. To create an unencrypted wifi you can leave the password field empty. This is not recommended! 
                    </div>
                  </div>
                </form>
              </div>
              <div class="modal-footer">
                <button type="button" id="postWifiApSettingsButton" class="btn btn-success btn-block btn-lg" onclick="postSettings('wifi_ap')">Save changes</button>
              </div>
            </div>
          </div>
        </div>
      </div>
   `;
}

function getChartContent() {
  return `
  <!--
  <div class="inner"><h3>Temperature</h3></div> 
  -->    
  <div class="rounded my-auto" style="background: rgba(46, 51, 56, 0.7)!important;">
    <h1 class="row" style="margin: 8px;"><img src="img/water-temp.svg" alt="Hot water" width="50" height="50" />&nbsp;&nbsp;<span id="currentTemp">--&nbsp;</span>&deg;C&nbsp;</h1>
  </div>
  <div id="chart1" class="ct-chart-1 ct-perfect-fourth"></div>
  <div style="bottom: 8px; left: 8px; background: rgba(46, 51, 56, 0.7)!important;" class="position-absolute rounded my-auto">
    <span hidden id="tuningLabel" style="margin: 8px; font-size: x-large; font-weight: bold; color: blue;">TUNING</span>
  </div>
  <div style="bottom: 8px; right: 8px; background: rgba(46, 51, 56, 0.7)!important;" class="position-absolute rounded my-auto">
    <span hidden id="standByLabel" style="margin: 8px; font-size: x-large; font-weight: bold; color: crimson;">STANDBY</span>
    <span id="powerLabel" style="margin: 8px; font-family: monospace; font-size: xx-large;"></span>
 </div>
 `;
}