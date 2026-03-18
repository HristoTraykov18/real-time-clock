const ANIMATION_TIMEOUT = 500;
const SLIDERS_THUMB_DIAMETER = 25;


window.addEventListener("load", function() { // Add event listeners for the javascript functionalities
    requestConfig();

    if (getActiveWorkMode() === "rtc")
        submitManualTime();

    document.getElementsByTagName("form")[0].addEventListener("submit", submitNetworkRequest);
    document.getElementById("js-time-sync-mode").addEventListener("click", toggleTimeSyncMode);
    document.getElementById("js-daylight-saving").addEventListener("click", toggleDaylightSaving);
    document.getElementById("js-password-button-container").addEventListener("click", togglePasswordVisibility);

    // Tab switching
    document.getElementById("js-rtc-menu-button").addEventListener("click", function() { switchTab("rtc"); submitManualTime(); });
    document.getElementById("js-timer-menu-button").addEventListener("click", function() { switchTab("timer"); });

    // Timer controls
    document.getElementById("js-hours-up").addEventListener("click", function() { adjustTimerUnit("hours", 1); });
    document.getElementById("js-hours-down").addEventListener("click", function() { adjustTimerUnit("hours", -1); });
    document.getElementById("js-minutes-up").addEventListener("click", function() { adjustTimerUnit("minutes", 1); });
    document.getElementById("js-minutes-down").addEventListener("click", function() { adjustTimerUnit("minutes", -1); });
    document.getElementById("js-seconds-up").addEventListener("click", function() { adjustTimerUnit("seconds", 1); });
    document.getElementById("js-seconds-down").addEventListener("click", function() { adjustTimerUnit("seconds", -1); });
    document.getElementById("js-timer-start").addEventListener("click", sendTimerStart);
    document.getElementById("js-timer-pause").addEventListener("click", sendTimerPause);
    document.getElementById("js-timer-restart").addEventListener("click", sendTimerRestart);

    // Slider input
    document.getElementById("js-brightness-control-label").addEventListener("mouseup", toggleBrightnessSliderInput);

    let slidersInputs = document.getElementsByClassName("js-slider-input");
    let slidersThumbs = document.getElementsByClassName("js-slider-thumb");

    for (let i = 0, arrLen = slidersInputs.length; i < arrLen; i++) {
        // Check if the slider has tooltip
        let hasTooltip = slidersInputs[i].parentNode.parentNode.classList.contains("js-sc-big");

        slidersInputs[i].addEventListener("input", function() {
            updateSlider(this, slidersThumbs[i], hasTooltip);
        });
        slidersInputs[i].addEventListener("change", function() {
            updateSlider(this, slidersThumbs[i], hasTooltip);
            let submitData = "autoBrightnessControl=false&manualBrightnessLevel=";
            submitData += slidersInputs[i].value;

            sendServerRequest(submitData);
        });
    }

    let closePopupButtons = document.getElementsByClassName("js-cancel-popup-button"); // Close buttons in popups

    for (let i = 0, arrLen = closePopupButtons.length; i < arrLen; i++) {
        closePopupButtons[i].addEventListener("click", function() {
            closePopup(this);
        });
    }
});

// Close the currently opened popup
function closePopup(clickedButton) {
    clickedButton.parentNode.parentNode.parentNode.classList.remove("show-popup");
}

function requestConfig() {
    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (this.readyState === 4 && this.status === 200) {
            let xmlDoc = this.responseXML;  // Get the settings file and compare each value
                                            // Edit the webpage accordingly

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

            updateSlider(brightnessSliderInput, brightnessSliderThumb, false);

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
        }
    };

    xhttp.open("GET", "/settings", false);
    xhttp.setRequestHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
    xhttp.send();
}

function sendServerRequest(requestParams) {
    toggleLoader(); // Show the loading screen

    let retries = 2;
    let xhttp = new XMLHttpRequest();
    xhttp.timeout = 20000;
    xhttp.onreadystatechange = function() {
        if (this.readyState === 4) {
            let response = "Възникна грешка\nМоля проверете дали сте свързани с мрежата на часовника и опитайте отново";

            if (this.status === 200) {
                toggleLoader();
                response = this.responseText;
            }
            else if (retries > 0) {
                this.abort();
                xhttp.open("POST", "/", true);
                xhttp.send(requestParams);

                if (retries === 3)
                    response = "Заявката до часовника беше неуспешна. Опитвам отново...";

                retries -= 1;
            }
            else {
                toggleLoader();
                response = "Връзката с мрежата, към която се опитвате да се свържете не е стабилна\nМоля свържете часовника с друга мрежа";
            }

            showStatusPopup(response);
        }
    };
    xhttp.ontimeout = function() {
        this.abort();
        showStatusPopup("Времето за свързване с часовника изтече.\nМоля проверете дали сте свързани с мрежата на часовника и опитайте отново");
    };
    xhttp.open("POST", "/", true);
    xhttp.setRequestHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
    xhttp.send(requestParams);
}

// Display the popup on submit
function showStatusPopup(popupText) {
    document.getElementById("js-popup-container").classList.add("show-popup");
    document.getElementById("js-popup-message").innerText = popupText;
}

function submitManualTime() {
    let submitData = "timeSyncMode=js&currentTime=";
    let currentDate = new Date();
    submitData += Array(currentDate.getFullYear(), currentDate.getMonth(), currentDate.getDate(), 
                        currentDate.getHours(), currentDate.getMinutes(), currentDate.getSeconds());
    submitData += "&timezoneHoursOffset=" + (currentDate.getTimezoneOffset() / -60);
    submitData += "&workMode=" + getActiveWorkMode();

    sendServerRequest(submitData);
}

// Write the changes to their hidden inputs so the server can read them
function submitNetworkRequest(event) {
    event.preventDefault();
    let networkInputs = document.getElementsByClassName("main-settings-input");
    let submitData = "ssid=" + networkInputs[0].value;

    submitData += "&pass=" + networkInputs[1].value;
    submitData += "&timeSyncMode=wifi";

    submitData += "&isHiddenNetwork=";
    submitData += document.getElementsByName("hiddenNetwork")[0].checked;

    let currentDate = new Date();
    submitData += "&timezoneHoursOffset=" + (currentDate.getTimezoneOffset() / -60);
    submitData += "&workMode=" + getActiveWorkMode();

    sendServerRequest(submitData);
}

// Show or hide the slider input
function toggleBrightnessSliderInput() {
    let isChecked = document.getElementById("js-brightness-control").checked;
    let sliderContainer = document.getElementById("js-brightness-slider-container");

    if (isChecked) { // Show
        sliderContainer.style.opacity = 1;
        sliderContainer.style.pointerEvents = "all";
    }
    else { // Hide
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

// Loading screen during server xml http response
function toggleLoader() {
    let formContainers = document.getElementsByClassName("form-content");

    for (let i = 0, arrLen = formContainers.length; i < arrLen; i++) {
        if (window.getComputedStyle(formContainers[i]).display === "flex")
            formContainers[i].style.display = "none";
        else
            formContainers[i].style.display = "flex";
    }
}

// Show or hide the password
function togglePasswordVisibility() {
    let passInput = document.getElementById("js-pass-input");
    let showPasswordButton = document.getElementById("js-show-password-button");
    let hidePasswordButton = document.getElementById("js-hide-password-button");

    if (passInput.type === "password") {
        passInput.type = "text";
        showPasswordButton.style.opacity = "0";
        hidePasswordButton.style.opacity = "1";
    }
    else {
        passInput.type = "password";
        showPasswordButton.style.opacity = "1";
        hidePasswordButton.style.opacity = "0";
    }

    passInput.focus();
}

function toggleTimeSyncMode() {
    let timeSyncMode = document.getElementById("js-time-sync-mode");
    let submitData = "timeSyncMode=";

    if (timeSyncMode.checked)
        submitData += "wifi";
    else
        submitData += "gps";

    sendServerRequest(submitData);
}

// Move the slider thumb
function updateSlider(slider, thumb, hasTooltip) {
    // Using min and max values of the input, so the thumb movement is responsive
    let min = Number(slider.min);
    let max = Number(slider.max);
    let currentValue = Number(slider.value);

    // 0% to 100% margin from left
    let newX = (currentValue / (max - min)) * 100 > 100 ? 100 : (currentValue / (max - min)) * 100;

    // Set margin from left for the thumb
    thumb.style.left = `calc(${newX}% - ${SLIDERS_THUMB_DIAMETER / 2}px)`;

    if (hasTooltip) { // Set tooltip value and position it if the slider has tooltip
        let tooltipDiv = thumb.parentNode.children[0];
        tooltipDiv.style.left = `calc(${newX}% - ${SLIDERS_THUMB_DIAMETER / 2}px)`;
        tooltipDiv.textContent = currentValue;
    }
}

// Tab switching
function switchTab(tabName) {
    let rtcPanel   = document.getElementById("js-main-settings");
    let timerPanel = document.getElementById("js-timer-settings");
    let rtcBtn     = document.getElementById("js-rtc-menu-button");
    let timerBtn   = document.getElementById("js-timer-menu-button");

    let isTimer = (tabName === "timer");

    rtcPanel.classList.toggle("active-content", !isTimer);
    timerPanel.classList.toggle("active-content", isTimer);

    rtcBtn.classList.toggle("active-tab-button", !isTimer);
    rtcBtn.classList.toggle("inactive-tab-button", isTimer);
    timerBtn.classList.toggle("active-tab-button", isTimer);
    timerBtn.classList.toggle("inactive-tab-button", !isTimer);
}

// Timer duration picker
function getActiveWorkMode() {
    return document.getElementById("js-timer-settings").classList.contains("active-content") ? "timer" : "rtc";
}

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

function getTimerDurationSeconds() {
    let h = parseInt(document.getElementById("js-timer-hours").textContent, 10);
    let m = parseInt(document.getElementById("js-timer-minutes").textContent, 10);
    let s = parseInt(document.getElementById("js-timer-seconds").textContent, 10);

    return h * 3600 + m * 60 + s;
}

function sendTimerStart() {
    let seconds = getTimerDurationSeconds();
    if (seconds === 0) return;
    sendServerRequest("status=start&duration=" + seconds + "&workMode=timer");
}

function sendTimerPause() {
    sendServerRequest("status=pause&workMode=timer");
}

function sendTimerRestart() {
    sendServerRequest("status=restart&workMode=timer");
}

/* END OF MAIN mainScript.js FILE */
