document.getElementsByTagName("body")[0].onload = function() { // Add event listeners for the javascript functionalities
    RequestUserSettings();

    document.getElementsByTagName("form")[0].addEventListener("submit", SubmitChanges);
    document.getElementById("js-auto-set-option").addEventListener("click", ShowAutoSettings); // Auto option button
    document.getElementById("js-manual-set-option").addEventListener("click", ShowManualSettings); // Manual option button
    document.getElementById("js-password-button-container").addEventListener("click", 
        TogglePasswordVisibility);

    // Increase and decrease buttons
    let decreaseButtons = document.getElementsByClassName("decrease-button");
    let increaseButtons = document.getElementsByClassName("increase-button");
    
    for (let i = 0; i < increaseButtons.length; i++) {
        decreaseButtons[i].addEventListener("click", function() {
            DecreaseTime(i, this);
        });
        increaseButtons[i].addEventListener("click", function() {
            IncreaseTime(i, this);
        });
    }

    // Slider input
    document.getElementById("js-brightness-control-label").addEventListener("click", ToggleSliderInput);
    let sliderInput = document.getElementById("js-slider-input");
    let sliderThumb = document.getElementById("js-slider-thumb");
    
    sliderInput.addEventListener("input", function() {
        UpdateSlider(this, sliderThumb, sliderThumb.scrollWidth);
    });
    sliderInput.addEventListener("change", function() {
        UpdateSlider(this, sliderThumb, sliderThumb.scrollWidth);
    });

    document.getElementById("js-popup-button").addEventListener("click", CloseStatusPopup); // Popup button
};

function CloseStatusPopup() {
    document.getElementById("js-popup-container").classList.remove("show-popup");
    document.getElementById("js-popup-message").innerText = "";
}

function DecreaseTime(i, clickedButton) {
    // Animate the button on click
    clickedButton.style.borderTopColor = "var(--pale-purple)";

    setTimeout(function() {
        clickedButton.style.borderTopColor = "var(--red)";
    }, 150);

    let clockDigits = document.getElementsByClassName("clock-digit");
    let number = parseInt(clockDigits[i].value) - 1; // Decrease the value for the corresponding digit

    if (i == 0) {
        if (number == -1)
            number = 2;
        
        if (number == 2) { // First digit can be 0, 1 and 2. Adjust the second digit according to it
            let secondDigit = clockDigits[1].value;

            if (secondDigit > 3)
                clockDigits[1].value = "0";
        }
    }
    else if (i == 1) { // Second digit can be 0 to 9 or 0 to 3 depending on the first digit
        let firstDigit = parseInt(clockDigits[0].value);

        if (firstDigit < 2 && number === -1)
            number = 9;
        else if (firstDigit === 2 && number === -1)
            number = 3;
    }
    else if (i === 2 && number === -1)
        number = 5; // Third digit can be 0 to 5
    else if (i === 3 && number === -1)
        number = 9; // Fourth digit can be 0 to 9

    clockDigits[i].value = number.toString();
    return false;
}

function IncreaseTime(i, clickedButton) { // Oposite of DecreaseTime
    // Animate the button on click
    clickedButton.style.borderBottomColor = "var(--pale-purple)";

    setTimeout(function() {
        clickedButton.style.borderBottomColor = "var(--red)";
    }, 150);

    let clockDigits = document.getElementsByClassName("clock-digit");
    let number = parseInt(clockDigits[i].value) + 1;

    if (i == 0) { // First digit can be 0, 1 and 2. Adjust the second digit according to it
        if (number == 3)
            number = 0;
        else if (number == 2) {
            let secondDigit = clockDigits[1].value;

            if (secondDigit > 3)
                clockDigits[1].value = "0";
        }
    }
    else if (i == 1) { // Second digit can be 0 to 9 or 0 to 3 depending on the first digit
        let firstDigit = parseInt(clockDigits[0].value);

        if (firstDigit < 2 && number == 10)
            number = 0;
        else if (firstDigit == 2 && number == 4)
            number = 0;
    }
    else if (i == 2 && number == 6)
        number = 0; // Third digit can be 0 to 5
    else if (i == 3 && number == 10)
        number = 0; // Fourth digit can be 0 to 9

    clockDigits[i].value = number.toString();
    return false;
}

function RequestConnectionStatus(requestParams) {
    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        // The request finished and response is ready
        if (xhttp.readyState === 4) {
            let statusText = "";

            // In case of successful response
            if (xhttp.status === 200)
                statusText = xhttp.responseText;
            /*else
                statusText = "Моля проверете връзката с часовника!";*/
            
            ToggleLoader();
            ShowStatusPopup(statusText);
        }
    };

    xhttp.open("POST", "/", true);
    xhttp.setRequestHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
    xhttp.send(requestParams);
}

function RequestUserSettings() {
    let xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (this.readyState === 4 && this.status === 200) { 
            let xmlDoc = this.responseXML;  // Get the settings file and compare each value
                                            // Edit the webpage accordingly

            // Daylight saving checkbox
            let daylightSavingCheckbox = document.getElementById("js-daylight-saving");
            
            if (xmlDoc.getElementsByTagName("daylightSaving")[0].childNodes[0].nodeValue === "Active")
                daylightSavingCheckbox.checked = true;
            
            // Slider (for brightness) input and checkbox
            let sliderContainer = document.getElementById("js-slider-container");
            if (xmlDoc.getElementsByTagName("brightnessControl")[0].childNodes[0].nodeValue !== "Auto") {
                sliderContainer.style.opacity = 1;
                sliderContainer.style.pointerEvents = "all";
                document.getElementById("js-brightness-control").checked = false;
            }

            let sliderInput = document.getElementById("js-slider-input");
            let sliderThumb = document.getElementById("js-slider-thumb");

            sliderInput.value = xmlDoc.getElementsByTagName("manualBrightnessLevel")[0].childNodes[0].nodeValue;
            UpdateSlider(sliderInput, sliderThumb, sliderThumb.scrollWidth);
            
            // Radio buttons
            let timeSyncCheckboxes = document.getElementsByClassName("js-time-sync-type");
            let syncTimeWith = xmlDoc.getElementsByTagName("timeSetup")[0].childNodes[0].nodeValue.toLowerCase();
            let checkString = "js-" + syncTimeWith + "-input";

            for (let i = 0; i < timeSyncCheckboxes.length; i++) {
                if (timeSyncCheckboxes[i].id === checkString)
                    timeSyncCheckboxes[i].checked = true;
            }

        }
    };

    xhttp.open("GET", "/espSettings.xml", true);
    xhttp.setRequestHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
    xhttp.send();
}

function ShowAutoSettings() { // Show the auto fieldsets
    document.getElementById("js-auto-set-option").style.backgroundColor = "#012341";
    document.getElementById("js-manual-set-option").style.backgroundColor = "#384575";
    document.getElementById("js-time-sync-mode").value = "auto";

    let autoSettings = document.getElementById("js-auto-settings");
    let manualSettings = document.getElementById("js-manual-settings");

    manualSettings.style.opacity = "0";

    setTimeout(function() {
        autoSettings.style.display = "block";
        autoSettings.style.opacity = "1";
        manualSettings.style.display = "none";
    }, 300);
}

function ShowManualSettings() { // Show the manual fieldset
    document.getElementById("js-auto-set-option").style.backgroundColor = "#384575";
    document.getElementById("js-manual-set-option").style.backgroundColor = "#012341";
    document.getElementById("js-time-sync-mode").value = "manual";

    let autoSettings = document.getElementById("js-auto-settings");
    let manualSettings = document.getElementById("js-manual-settings");

    autoSettings.style.opacity = "0";

    setTimeout(function() {
        autoSettings.style.display = "none";
        manualSettings.style.display = "block";
        manualSettings.style.opacity = "1";
    }, 300);
}

function ShowStatusPopup(popupText) { // Displays the popup on submit
    document.getElementById("js-popup-container").classList.add("show-popup");
    document.getElementById("js-popup-message").innerText = popupText;
}

function SubmitChanges() { // Write the changes to their hidden inputs so the ESP can read them
    event.preventDefault(); // Prevent page reload
    event.stopPropagation();

    ToggleLoader();

    let autoSettingsInputs = document.getElementsByClassName("auto-settings-input");
    let submitData = "ssid=" + autoSettingsInputs[0].value;
    submitData += "&pass=" + autoSettingsInputs[1].value;
    submitData += "&daylightSaving=";

    // Checkbox for daylight saving
    if (document.getElementById("js-daylight-saving").checked === true)
        submitData += "ON";
    else
        submitData += "OFF";

    submitData += "&brightnessControl=";

    if (document.getElementById("js-brightness-control").checked === true)
        submitData += "auto";
    else
        submitData += "manual";
    
    if (document.getElementById("js-time-sync-mode").value === "manual") {
        submitData += "&manualTimeValue=";

        for (let i = 0; i < 4; i++)
            submitData += document.getElementsByClassName("clock-digit")[i].value;
    }

    submitData += "&brightnessValue=" + document.getElementsByName("brightnessValue")[0].value;
    
    submitData += "&timeSyncMode=" + document.getElementById("js-time-sync-mode").value;
    submitData += "&setTimeWith=";
    let timeSyncType = document.getElementsByClassName("js-time-sync-type");

    if (timeSyncType.length > 1) {
        if (timeSyncType[1].checked === true)
            submitData += "gps";
        else
            submitData += "wifi";
    }
    else
        submitData += "wifi";
    
    RequestConnectionStatus(submitData);
}

function ToggleLoader() { // Show loading screen while waiting for xml http response
    let formContainers = document.getElementsByClassName("form-container");

    for (let i = 0; i < formContainers.length; i++) {
        if (window.getComputedStyle(formContainers[i]).display === "inline-block")
            formContainers[i].style.display = "none";
        else
            formContainers[i].style.display = "inline-block";
    }
}

function TogglePasswordVisibility() { // Show or hide the password
    let passInput = document.getElementById("js-pass-input");
    let showPasswordButton = document.getElementById("showPasswordButton");
    let hidePasswordButton = document.getElementById("hide-password-button");

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

function ToggleSliderInput() { // Show or hide the slider input
    let isChecked = document.getElementById("js-brightness-control").checked;
    let sliderContainer = document.getElementById("js-slider-container");

    if (isChecked === true) {
        sliderContainer.style.opacity = 0;
        sliderContainer.style.pointerEvents = "none";
    }
    else {
        sliderContainer.style.opacity = 1;
        sliderContainer.style.pointerEvents = "all";
    }
}

function UpdateSlider(slider, thumb, thumbWidth) { // Moves the slider thumb
    // Using min and max values of the input, so the thumb movement is responsive
    let min = Number(slider.min);
    let max = Number(slider.max);
    let currentValue = Number(slider.value);

    // 0% to 100% margin from left
    let newX = (currentValue / (max - min)) * 100 > 100 ? 100 : (currentValue / (max - min)) * 100;

    // Set margin from left for the thumb
    thumb.style.left = `calc(${newX}% - ${thumbWidth / 2}px`;
}
