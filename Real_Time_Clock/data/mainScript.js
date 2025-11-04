const ANIMATION_TIMEOUT = 500;
const SLIDERS_THUMB_DIAMETER = 25;

let activePicker = 'hour';

window.addEventListener("load", function() { // Add event listeners for the javascript functionalities
    requestConfig();
    submitManualTime();
    document.getElementsByTagName("form")[0].addEventListener("submit", submitNetworkRequest);
    document.getElementById("js-time-sync-mode").addEventListener("click", toggleTimeSyncMode);
    document.getElementById("js-daylight-saving").addEventListener("click", toggleDaylightSaving);
    document.getElementById("js-password-button-container").addEventListener("click", togglePasswordVisibility);

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

    updatePickerState();

    // Attach event listeners for both pickers
    hourPickerContainer.addEventListener('mousedown', (e) => { timePick(e); toggleMinutePicker(); });
    minutePickerContainer.addEventListener('mousedown', timePick);

    // Add event listeners for toggling pickers
    hourDisplay.addEventListener('click', toggleHourPicker);
    minuteDisplay.addEventListener('click', toggleMinutePicker);
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
        }
    };

    xhttp.open("GET", "/settings", true);
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
/* END OF MAIN mainScript.js FILE */
