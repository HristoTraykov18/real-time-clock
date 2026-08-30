const SLIDERS_THUMB_DIAMETER = 25;
const INACTIVITY_TIMEOUT_MS = 600000;
const INACTIVITY_WARNING_MS = 30000;
const DEFAULT_REQUEST_TIMEOUT_MS = 10000;
const NETWORK_REQUEST_TIMEOUT_MS = 20000;

// SSID of the row the user tapped, carried into the password popup
let selectedNetworkSSID = "";


async function initializeApp () { // Add event listeners for the javascript functionalities
    document.getElementById("js-main-form").addEventListener("submit", submitNetworkRequest);
    document.getElementById("js-time-sync-mode").addEventListener("click", toggleTimeSyncMode);
    document.getElementById("js-daylight-saving").addEventListener("click", toggleDaylightSaving);
    document.getElementById("js-password-button-container").addEventListener("click", togglePasswordVisibility);

    // Tab switching
    document.getElementById("js-rtc-menu-button").addEventListener("click", function() { switchTab("rtc"); submitManualTime(); });
    document.getElementById("js-timer-menu-button").addEventListener("click", function() { switchTab("timer"); });

    // Networks list and password popup
    document.getElementById("js-network-list-rows").addEventListener("click", function(event) {
        const row = event.target.closest(".network-row");
        if (row) showNetworkPopup(row.dataset.ssid);
    });
    document.getElementById("js-network-refresh").addEventListener("click", function(event) {
        event.stopPropagation();
        requestNetworks();
    });

    document.getElementsByName("hiddenNetwork")[0].addEventListener("change", toggleHiddenNetworkMode);
    document.getElementById("js-network-popup-form").addEventListener("submit", function(event) {
        event.preventDefault();
        connectToSelectedNetwork();
    });
    document.getElementById("js-network-cancel-btn").addEventListener("click", closeNetworkPopup);
    document.getElementById("js-popup-password-btn-container").addEventListener("click", togglePopupPasswordVisibility);

    // Info panel
    document.getElementById("js-info-button").addEventListener("click", openInfoPanel);

    // Additional settings panel
    document.getElementById("js-additional-settings-button").addEventListener("click", openAdditionalSettings);
    document.getElementById("js-timezone-minus").addEventListener("click", function(e) { e.stopPropagation(); adjustTimezone(-1); });
    document.getElementById("js-timezone-plus").addEventListener("click",  function(e) { e.stopPropagation(); adjustTimezone(1); });
    document.getElementById("js-delete-creds-btn").addEventListener("click", deleteCredentials);
    document.getElementById("js-software-update-btn").addEventListener("click", activateSoftwareUpdate);

    // Timer controls
    document.getElementById("js-hours-up").addEventListener("click", function() { adjustTimerUnit("hours", 1); });
    document.getElementById("js-hours-down").addEventListener("click", function() { adjustTimerUnit("hours", -1); });
    document.getElementById("js-minutes-up").addEventListener("click", function() { adjustTimerUnit("minutes", 1); });
    document.getElementById("js-minutes-down").addEventListener("click", function() { adjustTimerUnit("minutes", -1); });
    document.getElementById("js-seconds-up").addEventListener("click", function() { adjustTimerUnit("seconds", 1); });
    document.getElementById("js-seconds-down").addEventListener("click", function() { adjustTimerUnit("seconds", -1); });
    document.getElementById("js-timer-start").addEventListener("click", sendTimerStart);
    document.getElementById("js-timer-pause-control").addEventListener("click", sendTimerPauseControl);

    // Slider input
    document.getElementById("js-brightness-control-label").addEventListener("mouseup", toggleBrightnessSliderInput);

    let slidersInputs = document.getElementsByClassName("js-slider-input");
    let slidersThumbs = document.getElementsByClassName("js-slider-thumb");

    for (let i = 0, arrLen = slidersInputs.length; i < arrLen; i++) {
        slidersInputs[i].addEventListener("input", function() {
            updateSlider(this, slidersThumbs[i]);
        });
        slidersInputs[i].addEventListener("change", function() {
            updateSlider(this, slidersThumbs[i]);
            let submitData = "autoBrightnessControl=false&manualBrightnessLevel=";
            submitData += slidersInputs[i].value;

            sendServerRequest(submitData);
        });
    }

    // Inactivity session timeout for softAP clients
    if (window.location.hostname === "192.168.4.1") {
        document.getElementById("js-continue-session-button").addEventListener("click", function() {
            resetInactivityTimer(true);
        });
        resetInactivityTimer();
        inactivityTicker = setInterval(inactivityTick, 1000);
    }

    let closePopupButtons = document.getElementsByClassName("js-close-popup-button"); // Close buttons in popups

    for (let i = 0, arrLen = closePopupButtons.length; i < arrLen; i++) {
        closePopupButtons[i].addEventListener("click", function() {
            closePopup(this);
        });
    }

    toggleLoader();
    let shouldSyncTime = false;
    [shouldSyncTime] = await Promise.all([requestConfig(), requestNetworks()]);

    if (shouldSyncTime)
        await submitManualTime(false);

    toggleLoader();
};

// Inactivity timeout functionality
let inactivityDeadline = 0;
let inactivityTicker = null;
let sessionDisconnected = false;

function inactivityTick() {
    if (sessionDisconnected) return;
    let remainingMs = Math.ceil(inactivityDeadline - Date.now());

    if (remainingMs <= 0) {
        timeoutForInactivity();
        return;
    }

    if (remainingMs <= INACTIVITY_WARNING_MS) {
        document.getElementById("js-timeout-popup-message").innerText =
            "Устройството Ви ще бъде разкачено от мрежата на часовника, поради неактивност след: " + Math.trunc(remainingMs / 1000) + " секунди";
        document.getElementById("js-timeout-popup-container").classList.add("show-popup");
    }
}

function resetInactivityTimer(sendExtendRequest = false) {
    if (sessionDisconnected) return;

    if (sendExtendRequest)
        sendServerRequest("", false, "/extend");

    inactivityDeadline = Date.now() + INACTIVITY_TIMEOUT_MS;
}

function timeoutForInactivity() {
    sessionDisconnected = true;
    clearInterval(inactivityTicker);
    sendServerRequest("", false, "/timeout");
    document.getElementById("js-timeout-popup-container").classList.remove("show-popup");
}
// --------------------------------

// Networks list and popup functionality
function connectToSelectedNetwork() {
    let submitData = "ssid=" + selectedNetworkSSID;
    submitData += "&pass=" + document.getElementById("js-popup-pass-input").value;
    submitData += "&timeSyncMode=wifi";
    submitData += "&isHiddenNetwork=false";
    submitData += "&workMode=" + getActiveWorkMode();

    document.getElementById("js-network-popup-container").classList.remove("show-popup");
    resetNetworkPopupPassword();
    sendServerRequest(submitData);
}

function closeNetworkPopup() {
    document.getElementById("js-network-popup-container").classList.remove("show-popup");
    resetNetworkPopupPassword();
}

function renderNetworkRows(responseText) {
    let listElement = document.getElementById("js-network-list-rows");
    listElement.innerHTML = "";

    let lines = responseText.trim().split("\n");

    if (lines.length === 0 || (lines.length === 1 && lines[0] === "")) {
        listElement.innerHTML = '<div class="network-list-empty">Няма видими мрежи в обхват</div>';

        return;
    }

    for (const line of lines) {
        // The SSID itself may contain "|", so the signal strength is taken from the last one
        let separatorIndex = line.lastIndexOf("|");

        if (separatorIndex === -1)
            continue;

        let ssid = line.substring(0, separatorIndex).trim();

        if (!ssid)
            continue;

        let rssi = parseInt(line.substring(separatorIndex + 1).trim(), 10);
        let row = document.createElement("div");
        row.className = "network-row";
        row.dataset.ssid = ssid;

        let ssidElement = document.createElement("span");
        ssidElement.className = "network-ssid";
        ssidElement.textContent = ssid; // textContent, so an SSID can never inject markup

        let iconWrapper = document.createElement("span");
        iconWrapper.innerHTML = wifiSignalSVG(rssi);

        row.appendChild(ssidElement);
        row.appendChild(iconWrapper);
        listElement.appendChild(row);
    }
}

async function requestNetworks() {
    let listElement = document.getElementById("js-network-list-rows");
    let loaderElement = document.getElementById("js-network-list-loader");
    let headerElement = document.querySelector("#js-network-list-wrapper .network-list-header > span");

    // Back to the normal state before every request
    listElement.style.display = "none";
    document.getElementById("js-network-list-disconnected").style.display = "none";
    headerElement.innerText = "Мрежи в обхват";
    loaderElement.style.display = "block";

    try {
        const response = await fetchWithTimeout("/networks", {});
        loaderElement.style.display = "none";

        if (!response.ok) {
            showNetworkListDisconnected();
            return;
        }

        listElement.style.display = "";
        renderNetworkRows(await response.text());
    } catch {
        loaderElement.style.display = "none";
        showNetworkListDisconnected();
    }
}

function resetNetworkPopupPassword() {
    document.getElementById("js-popup-pass-input").value = "";
    document.getElementById("js-popup-pass-input").type = "password";
    document.getElementById("js-popup-show-password-button").style.opacity = "1";
    document.getElementById("js-popup-hide-password-button").style.opacity = "0";
}

function showNetworkListDisconnected() {
    document.getElementById("js-network-list-rows").style.display = "none";
    document.getElementById("js-network-list-disconnected").style.display = "flex";
    document.querySelector("#js-network-list-wrapper .network-list-header > span").innerText =
        "Моля свържете се с мрежата на часовника";
}

function showNetworkPopup(ssid) {
    selectedNetworkSSID = ssid;
    document.getElementById("js-network-popup-title").innerText = "Въведете парола за " + ssid;
    resetNetworkPopupPassword();
    document.getElementById("js-network-popup-container").classList.add("show-popup");
}

function toggleHiddenNetworkMode() {
    let isHidden = document.getElementsByName("hiddenNetwork")[0].checked;

    document.getElementById("js-network-list-wrapper").classList.toggle("hidden", isHidden);
    document.getElementById("js-manual-input-wrapper").classList.toggle("hidden", !isHidden);
    document.getElementsByClassName("message")[0].innerText =
        isHidden ? "Моля въведете име и парола" : "Моля изберете мрежа от списъка";

    if (!isHidden) requestNetworks();
}

function togglePopupPasswordVisibility() {
    let passInput = document.getElementById("js-popup-pass-input");
    let showPasswordButton = document.getElementById("js-popup-show-password-button");
    let hidePasswordButton = document.getElementById("js-popup-hide-password-button");

    if (passInput.type === "password") {
        passInput.type = "text";
        showPasswordButton.style.opacity = "0";
        hidePasswordButton.style.opacity = "1";
    } else {
        passInput.type = "password";
        showPasswordButton.style.opacity = "1";
        hidePasswordButton.style.opacity = "0";
    }

    passInput.focus();
}

// Inline SVG for the signal strength indicator of a row
function wifiSignalSVG(rssi) {
    let activeArcs;

    if      (rssi > -55) activeArcs = 3;
    else if (rssi > -65) activeArcs = 2;
    else if (rssi > -75) activeArcs = 1;
    else                 activeArcs = 0;

    const active = "white";
    const inactive = "rgba(255,255,255,0.25)";
    const c1 = activeArcs >= 1 ? active : inactive;
    const c2 = activeArcs >= 2 ? active : inactive;
    const c3 = activeArcs >= 3 ? active : inactive;

    return `<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg" class="wifi-signal-icon">
        <circle cx="12" cy="20.5" r="1.8" fill="white"/>
        <path d="M 9.2 17 A 3.6 3.6 0 0 1 14.8 17" stroke="${c1}" stroke-width="2.2" fill="none" stroke-linecap="round"/>
        <path d="M 6.2 14 A 6.6 6.6 0 0 1 17.8 14" stroke="${c2}" stroke-width="2.2" fill="none" stroke-linecap="round"/>
        <path d="M 3.2 11 A 9.6 9.6 0 0 1 20.8 11" stroke="${c3}" stroke-width="2.2" fill="none" stroke-linecap="round"/>
    </svg>`;
}
// -------------------------------------

function closePopup(clickedButton) {
    clickedButton.parentNode.parentNode.parentNode.classList.remove("show-popup");
}

async function fetchWithTimeout(url, options = {}, timeoutMs = DEFAULT_REQUEST_TIMEOUT_MS) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), timeoutMs);

    try {
        const response = await fetch(url, { ...options, signal: controller.signal });
        clearTimeout(timer);

        if (window.location.hostname === "192.168.4.1")
            resetInactivityTimer();

        return response;
    } catch (err) {
        clearTimeout(timer);
        throw err;
    }
}

async function requestConfig() {
    try {
        const response = await fetchWithTimeout("/settings");
        if (!response.ok) return false;
        const xmlDoc = new DOMParser().parseFromString(await response.text(), "text/xml");

        // Daylight saving checkbox
        let daylightSavingCheckbox = document.getElementById("js-daylight-saving");

        if (xmlDoc.getElementsByTagName("daylightSavingEnabled")[0].childNodes[0].nodeValue == "true")
            daylightSavingCheckbox.checked = true;

        // Slider (for brightness) input and checkbox
        let brightnessSliderContainer = document.getElementById("js-brightness-slider-container");

        if (xmlDoc.getElementsByTagName("autoBrightnessControl")[0].childNodes[0].nodeValue  != "true") {
            brightnessSliderContainer.style.opacity = 1;
            brightnessSliderContainer.style.pointerEvents = "all";
            document.getElementById("js-brightness-control").checked = false;
        }

        let brightnessSliderInput = document.getElementById("js-brightness-slider-input");
        let brightnessSliderThumb = document.getElementById("js-brightness-slider-thumb");
        brightnessSliderInput.value = xmlDoc.getElementsByTagName("manualBrightnessLevel")[0].childNodes[0].nodeValue;

        updateSlider(brightnessSliderInput, brightnessSliderThumb);

        // Time synchronization mode slider
        let timeSyncSlider = document.getElementById("js-time-sync-mode");
        let timeSyncMode = xmlDoc.getElementsByTagName("timeSyncMode")[0].childNodes[0].nodeValue.toLowerCase();

        if (timeSyncMode === "gps")
            timeSyncSlider.checked = false;

        // Work mode tab — switch to timer panel if saved mode is "timer"
        let workModeNode = xmlDoc.getElementsByTagName("workMode")[0];

        if (workModeNode && workModeNode.childNodes[0].nodeValue.toLowerCase() === "timer")
            switchTab("timer");

        // Timer duration — populate HH:MM picker from saved seconds value
        let timerDurationNode = xmlDoc.getElementsByTagName("timerDuration")[0];

        if (timerDurationNode) {
            let totalSeconds = parseInt(timerDurationNode.childNodes[0].nodeValue, 10) || 3600;
            let h = Math.floor(totalSeconds / 3600);
            let m = Math.floor((totalSeconds / 60) % 60);
            let s = Math.floor(totalSeconds %  60);
            document.getElementById("js-timer-hours").textContent = String(h).padStart(2, "0");
            document.getElementById("js-timer-minutes").textContent = String(m).padStart(2, "0");
            document.getElementById("js-timer-seconds").textContent = String(s).padStart(2, "0");
        }

        return getActiveWorkMode() === "rtc";
    } catch {
        showStatusPopup("Неуспешно зареждане на конфигурацията на часовника.\nМоля проверете дали сте свързани с мрежата му и опитайте отново!");

        return false;
    };
}

async function sendServerRequest(params, loader = true, route = '/') {
    if (loader) toggleLoader();
    try {
        const response = await fetchWithTimeout(route, {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8" },
            body: params
        }, NETWORK_REQUEST_TIMEOUT_MS);

        if (loader) toggleLoader();

        showStatusPopup(response.ok ? await response.text() : "Възникна грешка!");
    } catch {

        if (loader) toggleLoader();

        showStatusPopup("Неуспешно свързване с часовника!\nМоля проверете дали сте свързани с мрежата му и опитайте отново");
    }
}

function showStatusPopup(popupText) {
    document.getElementById("js-popup-container").classList.add("show-popup");
    document.getElementById("js-popup-message").innerText = popupText;
}

async function submitManualTime(loader = true) {
    let submitData = "workMode=rtc&timeSyncMode=js&currentTime=";
    let currentDate = new Date();
    submitData += Array(currentDate.getFullYear(), currentDate.getMonth(), currentDate.getDate(),
                        currentDate.getHours(), currentDate.getMinutes(), currentDate.getSeconds());

    await sendServerRequest(submitData, loader);
}

function submitNetworkRequest(event) {
    event.preventDefault();
    let timeSyncMode = document.getElementById("js-time-sync-mode");
    let submitData = "workMode=" + getActiveWorkMode();

    if (!timeSyncMode.checked) {
        sendServerRequest("timeSyncMode=gps");

        return;
    }

    submitData += "&timeSyncMode=wifi";

    let networkInputs = document.getElementsByClassName("main-settings-input");
    submitData += "&ssid=" + networkInputs[0].value;
    submitData += "&pass=" + networkInputs[1].value;

    submitData += "&isHiddenNetwork=";
    submitData += document.getElementsByName("hiddenNetwork")[0].checked;

    sendServerRequest(submitData);
}

function toggleBrightnessSliderInput() {
    let isChecked = document.getElementById("js-brightness-control").checked;
    let sliderContainer = document.getElementById("js-brightness-slider-container");

    if (isChecked) { // Show
        sliderContainer.style.opacity = 1;
        sliderContainer.style.pointerEvents = "all";
    } else { // Hide
        sliderContainer.style.opacity = 0;
        sliderContainer.style.pointerEvents = "none";
    }

    let submitData = `autoBrightnessControl=${!isChecked}&manualBrightnessLevel=`;
    submitData += document.getElementById("js-brightness-slider-input").value;
    sendServerRequest(submitData);
}

function toggleDaylightSaving() {
    let submitData = "daylightSavingEnabled=";
    let daylightSavingCheckbox = document.getElementById("js-daylight-saving");
    submitData += daylightSavingCheckbox.checked;

    sendServerRequest(submitData);
}

function toggleLoader() {
    let formContainers = document.getElementsByClassName("form-content");

    for (let i = 0, arrLen = formContainers.length; i < arrLen; i++) {
        if (window.getComputedStyle(formContainers[i]).display === "flex")
            formContainers[i].style.display = "none";
        else
            formContainers[i].style.display = "flex";
    }
}

function togglePasswordVisibility() {
    let passInput = document.getElementById("js-pass-input");
    let showPasswordButton = document.getElementById("js-show-password-button");
    let hidePasswordButton = document.getElementById("js-hide-password-button");

    if (passInput.type === "password") {
        passInput.type = "text";
        showPasswordButton.style.opacity = "0";
        hidePasswordButton.style.opacity = "1";
    } else {
        passInput.type = "password";
        showPasswordButton.style.opacity = "1";
        hidePasswordButton.style.opacity = "0";
    }

    passInput.focus();
}

function toggleTimeSyncMode() {
    let timeSyncMode = document.getElementById("js-time-sync-mode");
    let submitData = "timeSyncMode=";
    submitData += timeSyncMode.checked ? "wifi" : "gps";
    sendServerRequest(submitData);
}

function updateSlider(slider, thumb) {
    // Using min and max values of the input, so the thumb movement is responsive
    let min = Number(slider.min);
    let max = Number(slider.max);
    let currentValue = Number(slider.value);

    // 0% to 100% margin from left
    let newX = (currentValue / (max - min)) * 100 > 100 ? 100 : (currentValue / (max - min)) * 100;

    // Set margin from left for the thumb
    thumb.style.left = `calc(${newX}% - ${SLIDERS_THUMB_DIAMETER / 2}px)`;
}

function switchTab(tabName) {
    let rtcPanel = document.getElementById("js-main-settings");
    let timerPanel = document.getElementById("js-timer-settings");
    let rtcBtn = document.getElementById("js-rtc-menu-button");
    let timerBtn = document.getElementById("js-timer-menu-button");

    let isTimer = (tabName === "timer");

    rtcPanel.classList.toggle("active-content", !isTimer);
    timerPanel.classList.toggle("active-content", isTimer);

    rtcBtn.classList.toggle("active-tab-button", !isTimer);
    rtcBtn.classList.toggle("inactive-tab-button", isTimer);
    timerBtn.classList.toggle("active-tab-button", isTimer);
    timerBtn.classList.toggle("inactive-tab-button", !isTimer);
}

// Side panels common functionality
let activePanelCloseFn = null;

function closeSidePanel(panelId) {
    document.getElementById(panelId).classList.remove("side-panel-open");
    document.getElementById(panelId + "-loader").style.display = "none";
    document.getElementById(panelId + "-info").style.display = "none";
    document.getElementById(panelId + "-disconnected").style.display = "none";
    activePanelCloseFn = null;
}

function displayDisconnectedState(panelId) {
    document.getElementById(panelId + "-info").style.display = "none";
    document.getElementById(panelId + "-loader").style.display = "none";
    document.getElementById(panelId + "-disconnected").style.display = "flex";
    document.getElementById(panelId + "-title").innerText = "Моля свържете се с мрежата на часовника";
}

async function openSidePanel(panelId, title, fetchUrl, closeFn, onSuccess) {
    const panel = document.getElementById(panelId);

    if (panel.classList.contains("side-panel-open"))
        return;

    if (activePanelCloseFn)
        activePanelCloseFn();

    activePanelCloseFn = closeFn;
    panel.classList.add("side-panel-open");
    document.addEventListener("click", closeFn);

    document.getElementById(panelId + "-loader").style.display = "block";
    document.getElementById(panelId + "-info").style.display = "none";
    document.getElementById(panelId + "-disconnected").style.display = "none";
    document.getElementById(panelId + "-title").innerText = title;

    try {
        const response = await fetchWithTimeout(fetchUrl, {});
        document.getElementById(panelId + "-loader").style.display = "none";
        if (response.ok) {
            onSuccess(await response.text());
            document.getElementById(panelId + "-info").style.display = "block";
        } else {
            displayDisconnectedState(panelId);
        }
    } catch {
        document.getElementById(panelId + "-loader").style.display = "none";
        displayDisconnectedState(panelId);
    }
}

// Info panel specific functionality
let infoPanelTimeInterval = null;
let infoPanelSeconds = 0;

function closeInfoPanel() {
    clearInterval(infoPanelTimeInterval);
    infoPanelTimeInterval = null;
    document.removeEventListener("click", closeInfoPanel);
    closeSidePanel("js-info-panel");
}

function openInfoPanel(event) {
    event.stopPropagation();
    openSidePanel("js-info-panel", "Информация за часовника", "/info", closeInfoPanel,
        function(responseText) {
            let parts = responseText.split("|");
            document.getElementById("js-info-ssid").innerText = "Свързана мрежа: " + parts[0];
            document.getElementById("js-info-rssi").innerText = "Сила на сигнала: " +
                (parts[1] === "-0" ? "-" : ("-" + parts[1] + " dBm"));
            document.getElementById("js-info-ip").innerText = "IP адрес: " + parts[2];
            document.getElementById("js-info-mac").innerText = "MAC адрес: " + parts[3];
            document.getElementById("js-info-time").innerText = "Час: " + parts[4];

            let timeParts = parts[4].split(":");
            infoPanelSeconds = parseInt(timeParts[0], 10) * 3600
                             + parseInt(timeParts[1], 10) * 60
                             + parseInt(timeParts[2], 10);
            infoPanelTimeInterval = setInterval(updateInfoPanelTime, 1000);
        }
    );
}

function updateInfoPanelTime() {
    infoPanelSeconds = (infoPanelSeconds + 1) % 86400;

    let h = Math.floor(infoPanelSeconds / 3600);
    let m = Math.floor((infoPanelSeconds % 3600) / 60);
    let s = infoPanelSeconds % 60;

    document.getElementById("js-info-time").innerText = "Час: "
        + String(h).padStart(2, "0") + ":"
        + String(m).padStart(2, "0") + ":"
        + String(s).padStart(2, "0");
}

// Additional settings panel specific functionality
let additionalSettingsTzDebounce = null;
let currentTimezone = 0;

async function activateSoftwareUpdate(event) {
    event.stopPropagation();

    document.getElementById("js-additional-settings-loader").style.display = "block";
    document.getElementById("js-additional-settings-info").style.display = "none";

    try {
        const response = await fetchWithTimeout("/activate-update", {});
        document.getElementById("js-additional-settings-loader").style.display = "none";

        if (response.ok) {
            window.location.href = `http://${window.location.hostname}:1394/sourceControl`;
        } else {
            displayDisconnectedState("js-additional-settings");
        }
    } catch {
        document.getElementById("js-additional-settings-loader").style.display = "none";
        displayDisconnectedState("js-additional-settings");
    }
}

function adjustTimezone(delta) {
    currentTimezone += delta;

    if (currentTimezone > 12)
        currentTimezone = -12;

    if (currentTimezone < -12)
        currentTimezone = 12;

    document.getElementById("js-timezone-value").innerText = currentTimezone > 0 ? "+" + currentTimezone : currentTimezone;

    // Show loader and reset after previous debounce if still pending
    clearTimeout(additionalSettingsTzDebounce);

    additionalSettingsTzDebounce = setTimeout(async function() {
        document.getElementById("js-additional-settings-loader").style.display = "block";
        document.getElementById("js-additional-settings-info").style.display = "none";

        try {
            await fetchWithTimeout("/", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8" },
                body: "timezoneHoursOffset=" + currentTimezone
            });
            document.getElementById("js-additional-settings-loader").style.display = "none";
            document.getElementById("js-additional-settings-info").style.display = "block";
        } catch {
            displayDisconnectedState("js-additional-settings");
        }
        additionalSettingsTzDebounce = null;
    }, 1000);
}

function closeAdditionalSettings() {
    clearTimeout(additionalSettingsTzDebounce);
    additionalSettingsTzDebounce = null;
    document.removeEventListener("click", closeAdditionalSettings);
    closeSidePanel("js-additional-settings");
}

async function deleteCredentials(event) {
    event.stopPropagation();
    const deleteBtn = document.getElementById("js-delete-creds-btn");

    if (deleteBtn.classList.contains("inactive")) return;

    document.getElementById("js-additional-settings-loader").style.display = "block";
    document.getElementById("js-additional-settings-info").style.display = "none";

    try {
        const response = await fetchWithTimeout("/delete-creds", {});
        document.getElementById("js-additional-settings-loader").style.display = "none";

        if (response.ok) {
            document.getElementById("js-additional-settings-info").style.display = "block";
            deleteBtn.textContent = "Няма свързана мрежа";
            deleteBtn.classList.replace("active", "inactive");
        } else {
            displayDisconnectedState("js-additional-settings");
        }
    } catch {
        document.getElementById("js-additional-settings-loader").style.display = "none";
        displayDisconnectedState("js-additional-settings");
    }
}

function openAdditionalSettings(event) {
    event.stopPropagation();
    openSidePanel("js-additional-settings", "Допълнителни настройки", "/additional-settings", closeAdditionalSettings,
        function(responseText) {
            let parts = responseText.split("|");
            currentTimezone = parseInt(parts[0], 10);
            document.getElementById("js-timezone-value").innerText =
                currentTimezone > 0 ? "+" + currentTimezone : currentTimezone;

            let deleteBtn = document.getElementById("js-delete-creds-btn");

            if (parts[1] === "true") {
                deleteBtn.textContent = "Прекъсване на връзката с мрежата";
                deleteBtn.classList.replace("inactive", "active");
            } else {
                deleteBtn.textContent = "Няма свързана мрежа";
                deleteBtn.classList.replace("active", "inactive");
            }
        }
    );
}

// Timer
let timerIsRunning = false;

function adjustTimerUnit(unit, delta) {
    let hoursEl   = document.getElementById("js-timer-hours");
    let minutesEl = document.getElementById("js-timer-minutes");
    let secondsEl = document.getElementById("js-timer-seconds");

    let h = parseInt(hoursEl.textContent, 10);
    let m = parseInt(minutesEl.textContent, 10);
    let s = parseInt(secondsEl.textContent, 10);

    if (unit === "hours") {
        h = (h + delta + 100) % 100; // 0–99 hours
    } else if (unit === "minutes") {
        m += delta;
        if (m < 0)  { m = 59; h = Math.max(0, h - 1); }
        if (m > 59) { m = 0;  h = Math.min(99, h + 1); }
    } else {
        s += delta;
        if (s < 0)  { s = 59; m = Math.max(0, m - 1); }
        if (s > 59) { s = 0; m = Math.min(59, m + 1); }
    }

    hoursEl.textContent   = String(h).padStart(2, "0");
    minutesEl.textContent = String(m).padStart(2, "0");
    secondsEl.textContent = String(s).padStart(2, "0");
}

function getActiveWorkMode() {
    return document.getElementById("js-timer-settings").classList.contains("active-content") ? "timer" : "rtc";
}

function getTimerDurationSeconds() {
    let h = parseInt(document.getElementById("js-timer-hours").textContent, 10);
    let m = parseInt(document.getElementById("js-timer-minutes").textContent, 10);
    let s = parseInt(document.getElementById("js-timer-seconds").textContent, 10);

    return h * 3600 + m * 60 + s;
}

function sendTimerPauseControl() {
    let pauseControlBtn = document.getElementById("js-timer-pause-control");

    if (!timerIsRunning) {
        sendServerRequest("workMode=timer&status=pause", false);
        pauseControlBtn.innerHTML = "&#8635; Продължи";
    } else {
        sendServerRequest("workMode=timer&status=resume", false);
        pauseControlBtn.innerHTML = "&#9646;&#9646; Пауза";
    }

    timerIsRunning = !timerIsRunning;
}

function sendTimerStart() {
    let seconds = getTimerDurationSeconds();

    if (seconds === 0)
        return;

    document.getElementById("js-timer-pause-control").innerHTML = "&#9646;&#9646; Пауза";
    sendServerRequest("workMode=timer&status=start&duration=" + seconds, false);
}

if (document.readyState === "complete")
    initializeApp();
else
    window.addEventListener("load", initializeApp);

/* END OF MAIN mainScript.js FILE */
