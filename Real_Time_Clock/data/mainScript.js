const ANIMATION_TIMEOUT = 400;
const SLIDERS_THUMB_DIAMETER = 25;

const AUDIO_MENU_URL = "/audio"; // New endpoint for fetching audio files
const ADD_NOTIFICATION_URL = "/add_notification"; // New endpoint for adding notifications
const PLAY_AUDIO_URL = "/play_audio"; // New endpoint for playing audio
const HOUR_PICKER_RADIUS = 150;
const MINUTE_PICKER_RADIUS = 150;
const OUTER_HOUR_RADIUS = 120;
const INNER_HOUR_RADIUS = 60;
const MINUTE_NUM_RADIUS = 120;
const hourDisplay = document.getElementById("js-hour-display");
const minuteDisplay = document.getElementById("js-minute-display");
const hourPickerContainer = document.getElementById("js-hour-picker-container");
const minutePickerContainer = document.getElementById("js-minute-picker-container");
const hourSelector = document.getElementById("js-hour-selector");
const minuteSelector = document.getElementById("js-minute-selector");

let istimePickging = false;
let activePicker = 'hour';

window.addEventListener("load", function() { // Add event listeners for the javascript functionalities
    requestConfig();
    submitManualTime();
    document.getElementsByTagName("form")[0].addEventListener("submit", submitNetworkRequest);
    document.getElementById("js-time-sync-mode").addEventListener("click", toggleTimeSyncMode);
    document.getElementById("js-daylight-saving").addEventListener("click", toggleDaylightSaving);
    document.getElementById("js-password-button-container").addEventListener("click", togglePasswordVisibility);
    document.getElementById("js-main-menu-button").addEventListener("click", () => switchMenu("main"));
    document.getElementById("js-audio-menu-button").addEventListener("click", () => {
        switchMenu("audio");
        // requestAudioFiles();
    });
    document.getElementById("js-add-notification-ok").addEventListener("click", addNotification);

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
    let networkInputs = document.getElementsByClassName("clock-settings-input");
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


// Functions for the audio menu
function switchMenu(menuId) {
    const mainMenu = document.getElementById("js-clock-settings");
    const audioMenu = document.getElementById("js-audio-menu-content");
    const mainMenuButton = document.getElementById("js-main-menu-button");
    const audioMenuButton = document.getElementById("js-audio-menu-button");

    if (menuId === "main") {
        mainMenu.style.display = "flex";
        audioMenu.style.display = "none";
        mainMenuButton.style.backgroundColor = "var(--dark-blue)";
        audioMenuButton.style.backgroundColor = "var(--pale-purple)";
    }
    else if (menuId === "audio") {
        mainMenu.style.display = "none";
        audioMenu.style.display = "flex";
        mainMenuButton.style.backgroundColor = "var(--pale-purple)";
        audioMenuButton.style.backgroundColor = "var(--dark-blue)";
    }
}

// Fetch the list of audio files from the ESP
function requestAudioFiles() {
    let xhttp = new XMLHttpRequest();

    xhttp.onreadystatechange = function() {
        if (this.readyState === 4 && this.status === 200) {
            const audioListContainer = document.querySelector(".audio-list");
            audioListContainer.innerHTML = ""; // Clear old list
            const files = JSON.parse(this.responseText);
            files.forEach(file => {
                const item = document.createElement("div");
                item.className = "audio-list-item";
                item.innerHTML = `
                <span class="audio-list-item-name">${file.name}</span>
                <div class="audio-list-item-buttons">
                    <div class="button-red cursor-pointer" onclick="playAudio('${file.name}')">Play</div>
                    <div class="button-red cursor-pointer" onclick="showNotificationPopup('${file.name}')">+</div>
                    <svg class="arrow-icon" viewBox="0 0 24 24" onclick="toggleSublist(this)">
                    <path d="M7,10l5,5l5-5H7z"/>
                    </svg>
                </div>
                <div class="sublist">
                    </div>
                `;
                audioListContainer.appendChild(item);
            });
        }
    };
    xhttp.open("GET", AUDIO_MENU_URL, true);
    xhttp.send();
}

// Send a request to the ESP to play the specific file
function playAudio(filename) {
    let xhttp = new XMLHttpRequest();

    xhttp.open("POST", PLAY_AUDIO_URL, true);
    xhttp.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    xhttp.send(`filename=${filename}`);
}

// Show the popup and set the filename
function showNotificationPopup(filename) {
    document.getElementById("js-notification-popup").classList.add("show-popup");
    document.getElementById("js-notification-filename").innerText = filename;
    document.getElementById("js-notification-popup").dataset.filename = filename;
}

// Send a request to the ESP to add the notification
function addNotification() {
    const filename = document.getElementById("js-notification-popup").dataset.filename;
    const hour = hourDisplay.innerText;
    const minute = minuteDisplay.innerText;
    const time = `${hour}:${minute}`;
    
    // Send a request to the ESP to add the notification
    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (this.readyState === 4 && this.status === 200) {
            closePopup(document.getElementById("js-add-notification-ok"));
        }
    };
    xhttp.open("POST", ADD_NOTIFICATION_URL, true);
    xhttp.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    xhttp.send(`filename=${filename}&time=${time}`);
}

function toggleSublist(iconElement) {
    const listItem = iconElement.closest('.audio-list-item');
    const sublist = listItem.querySelector('.sublist');
    const arrowIcon = listItem.querySelector('.arrow-icon');

    if (sublist.style.display === "block") {
        sublist.style.display = "none";
        arrowIcon.classList.remove('up');
    }
    else {
        // In a real implementation, you would make a request to the ESP to get the notifications for this file
        // For this example, let's just show some dummy data
        sublist.innerHTML = `<p>08:00</p><p>12:30</p>`; 
        sublist.style.display = "block";
        arrowIcon.classList.add('up');
    }
}

function timePick(e) {
    const container = (activePicker === 'hour') ? hourPickerContainer : minutePickerContainer;
    const selector = (activePicker === 'hour') ? hourSelector : minuteSelector;
    const radius = (activePicker === 'hour') ? HOUR_PICKER_RADIUS : MINUTE_PICKER_RADIUS;

    const rect = container.getBoundingClientRect();
    const centerX = rect.left + radius;
    const centerY = rect.top + radius;
    const x = e.clientX - centerX;
    const y = e.clientY - centerY;
    const dist = Math.sqrt(x * x + y * y);
    
    const angle = Math.atan2(y, x) * (180 / Math.PI) + 90;
    const normalizedAngle = (angle + 360) % 360;
    
    let selectedValue;

    if (activePicker === 'hour') {
        const hour = Math.round(normalizedAngle / 30);
        
        if (dist > (INNER_HOUR_RADIUS + 20) && dist < (OUTER_HOUR_RADIUS + 20)) {
            // Outer circle (0-11)
            selectedValue = hour === 12 ? 0 : hour;
        } 
        else if (dist < INNER_HOUR_RADIUS) {
            // Inner circle (12-23)
            selectedValue = hour === 12 ? 12 : hour + 12;
        }
        
        hourDisplay.innerText = selectedValue.toString().padStart(2, '0');
        
        // Update selector position
        const newRadius = (dist > INNER_HOUR_RADIUS) ? OUTER_HOUR_RADIUS : INNER_HOUR_RADIUS;
        const newAngle = (selectedValue % 12) * 30;
        const newRadians = ((newAngle - 90) * Math.PI) / 180;
        const newX = newRadius * Math.cos(newRadians);
        const newY = newRadius * Math.sin(newRadians);
        
        selector.style.transform = `translate(-50%, -50%) translate(${newX}px, ${newY}px)`;
    } 
    else { // Minute picker
        const minute = Math.round(normalizedAngle / 6);
        selectedValue = minute === 60 ? 0 : minute;
        
        minuteDisplay.innerText = (Math.round(selectedValue / 5) * 5).toString().padStart(2, '0');
        
        // Update selector position
        const newAngle = (Math.round(selectedValue / 5) * 5 * 6);
        const newRadians = ((newAngle - 90) * Math.PI) / 180;
        const newX = MINUTE_NUM_RADIUS * Math.cos(newRadians);
        const newY = MINUTE_NUM_RADIUS * Math.sin(newRadians);
        
        selector.style.transform = `translate(-50%, -50%) translate(${newX}px, ${newY}px)`;
    }
}

// Functions to toggle between pickers
function toggleHourPicker() {
    activePicker = 'hour';
    updatePickerState();
}

function toggleMinutePicker() {
    activePicker = 'minute';
    updatePickerState();
}

function updatePickerState() {
    if (activePicker === 'hour') {
        hourPickerContainer.classList.remove('hidden-element');
        minutePickerContainer.classList.add('hidden-element');
        
        hourDisplay.classList.add('active-display');
        minuteDisplay.classList.remove('active-display');
    } 
    else {
        minutePickerContainer.classList.remove('hidden-element');
        hourPickerContainer.classList.add('hidden-element');
        
        minuteDisplay.classList.add('active-display');
        hourDisplay.classList.remove('active-display');
    }
}