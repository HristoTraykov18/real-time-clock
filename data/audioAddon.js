/* AUDIO MODULE (base version) - appended to mainScript.js by rtc_setup. See docs/audio-module-base.spec, 4 */
const AUDIO_ROWS_PER_PAGE = 10;
const AUDIO_MAX_NOTIFICATIONS = 30;
const AUDIO_MAX_VOLUME = 10;
const AUDIO_ALL_WEEKDAYS = 0x7F; // bit0 = Monday ... bit6 = Sunday
const AUDIO_WEEKDAY_NAMES = ["Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Нд"];
const AUDIO_PLAY_ICON = '<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path d="M8 5.6v12.8a1.2 1.2 0 0 0 1.8 1l10-6.4a1.2 1.2 0 0 0 0-2l-10-6.4A1.2 1.2 0 0 0 8 5.6z"/></svg>';
const AUDIO_STOP_ICON = '<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><rect x="6" y="6" width="12" height="12" rx="2.5"/></svg>';
const AUDIO_EDIT_ICON = '<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path class="audio-icon-line" d="M13 3H6.5A1.5 1.5 0 0 0 5 4.5v15A1.5 1.5 0 0 0 6.5 21H11M13 3v5h5M13 3l5 5v2.5"/><path d="M19.6 12.4l2 2-6.6 6.6H13v-2z"/></svg>';
const AUDIO_DELETE_ICON = '<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path class="audio-icon-line" d="M4 7h16M9 7V4.5h6V7M6.5 7l1 13h9l1-13M10 11v5.5M14 11v5.5"/></svg>';

// Data of the last GET /audio: { volume, playing, tracks: [{ key, name }], notifications: [{ id, track, time, weekdays }] }
let audioData = null;
let audioView = "files";
let audioPages = { files: 0, notifications: 0 };
let audioSortAscending = true;
let audioSelectedIds = new Set();
let audioVolumeRequestPending = false;
let audioPopup = { editId: 0, track: "", hours: 7, minutes: 0, weekdays: 0 };


function initializeAudioAddon() {
    TAB_OPEN_HANDLERS.audio = openAudioTab;

    document.getElementById("js-audio-volume-minus").addEventListener("click", function() { changeAudioVolume(-1); });
    document.getElementById("js-audio-volume-plus").addEventListener("click", function() { changeAudioVolume(1); });

    // One delegated listener per table area
    document.getElementById("js-audio-table-header").addEventListener("click", handleAudioHeaderClick);
    document.getElementById("js-audio-views").addEventListener("click", handleAudioRowsClick);
    document.getElementById("js-audio-table-footer").addEventListener("click", handleAudioFooterClick);

    // Notification popup
    document.getElementById("js-audio-popup-files").addEventListener("click", function(event) {
        const item = event.target.closest(".audio-popup-file");

        if (!item) return;

        audioPopup.track = item.dataset.track;
        renderAudioPopupFiles();
    });
    document.getElementById("js-audio-popup-time").addEventListener("click", function(event) {
        const button = event.target.closest("button[data-unit]");

        if (button) stepAudioPopupTime(button.dataset.unit, parseInt(button.dataset.step, 10));
    });
    document.getElementById("js-audio-popup-time").addEventListener("wheel", function(event) {
        const digit = event.target.closest(".timer-digit[data-unit]");

        if (!digit) return;

        event.preventDefault();
        stepAudioPopupTime(digit.dataset.unit, event.deltaY < 0 ? 1 : -1);
    }, { passive: false });
    document.getElementById("js-audio-popup-weekdays").addEventListener("click", function(event) {
        const button = event.target.closest(".audio-weekday");

        if (!button) return;

        audioPopup.weekdays ^= 1 << parseInt(button.dataset.day, 10);
        renderAudioPopupWeekdays();
    });
    document.getElementById("js-audio-popup-save").addEventListener("click", saveAudioNotification);
    document.getElementById("js-audio-popup-cancel").addEventListener("click", closeAudioPopup);
}

// ---------------- Requests ----------------
// Request without the loader and without a popup on success (play, stop, volume). A rejection shows the clock's reason
async function sendAudioRequest(route, params) {
    try {
        const response = await fetchWithTimeout(route, {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8" },
            body: params
        });

        if (!response.ok) {
            showStatusPopup(response.status === 400 ? await response.text() : "Възникна грешка!");
            return false;
        }

        return true;
    } catch {
        showStatusPopup("Неуспешно свързване с часовника!\nМоля проверете дали сте свързани с мрежата му и опитайте отново");
        return false;
    }
}

function openAudioTab() {
    requestAudioData(audioData === null);
}

function parseAudioData(text) {
    const data = { volume: 0, playing: "0", tracks: [], notifications: [] };

    for (const line of text.split("\n")) {
        const parts = line.replace(/\r$/, "").split("|");

        if (parts[0] === "V")
            data.volume = parseInt(parts[1], 10) || 0;
        else if (parts[0] === "P")
            data.playing = parts[1];
        else if (parts[0] === "T" && parts.length >= 3)
            data.tracks.push({ key: parts[1], name: parts.slice(2).join("|") });
        else if (parts[0] === "N" && parts.length === 5)
            data.notifications.push({ id: parseInt(parts[1], 10), track: parts[2], time: parts[3], weekdays: parseInt(parts[4], 10) });
    }

    return data;
}

// The loader is shown on the first opening; later refreshes keep the table on screen
async function requestAudioData(showLoader) {
    const loader = document.getElementById("js-audio-loader");
    const views = document.getElementById("js-audio-views");
    const disconnected = document.getElementById("js-audio-disconnected");

    if (showLoader) {
        loader.style.display = "block";
        views.style.display = "none";
        disconnected.style.display = "none";
    }

    try {
        const response = await fetchWithTimeout("/audio", {});

        if (!response.ok)
            throw new Error("HTTP " + response.status);

        audioData = parseAudioData(await response.text());
        loader.style.display = "none";
        views.style.display = "";
        renderAudio();

        return true;
    } catch {
        loader.style.display = "none";

        if (audioData === null) {
            disconnected.style.display = "flex";
            document.getElementById("js-audio-settings").querySelector(".message").innerText =
                "Моля свържете се с мрежата на часовника";
        }

        return false;
    }
}

// ---------------- Rendering ----------------
function audioTrackName(key) {
    const track = audioData.tracks.find(function(t) { return t.key === key; });

    return track ? track.name : null;
}

function formatWeekdays(mask) {
    const parts = [];
    let day = 0;

    while (day < 7) {
        if (!(mask & (1 << day))) {
            day++;
            continue;
        }

        let end = day;

        while (end + 1 < 7 && (mask & (1 << (end + 1))))
            end++;

        if (end - day >= 2)
            parts.push(AUDIO_WEEKDAY_NAMES[day] + "-" + AUDIO_WEEKDAY_NAMES[end]);
        else
            for (let d = day; d <= end; d++)
                parts.push(AUDIO_WEEKDAY_NAMES[d]);

        day = end + 1;
    }

    return parts.join(", ");
}

function makeAudioNameCell(text, extraClass) {
    const cell = document.createElement("div");
    cell.className = "audio-name " + extraClass;

    const span = document.createElement("span");
    span.className = "audio-name-text";
    span.textContent = text; // textContent, so a name can never inject markup
    cell.appendChild(span);

    return cell;
}

function pageOf(items, view) {
    const pageCount = Math.max(1, Math.ceil(items.length / AUDIO_ROWS_PER_PAGE));
    audioPages[view] = Math.min(Math.max(audioPages[view], 0), pageCount - 1);

    const start = audioPages[view] * AUDIO_ROWS_PER_PAGE;

    return { rows: items.slice(start, start + AUDIO_ROWS_PER_PAGE), pageCount: pageCount };
}

function renderAudio() {
    document.getElementById("js-audio-volume-value").innerText = audioData.volume;
    document.getElementById("js-audio-table").dataset.view = audioView;

    for (const button of document.querySelectorAll(".audio-view-button"))
        button.classList.toggle("audio-view-button-active", button.dataset.view === audioView);

    // Drop selections of notifications that no longer exist
    const ids = new Set(audioData.notifications.map(function(n) { return n.id; }));
    audioSelectedIds = new Set([...audioSelectedIds].filter(function(id) { return ids.has(id); }));

    const filesPage = renderAudioFiles();
    const notificationsPage = renderAudioNotifications();
    const page = audioView === "files" ? filesPage : notificationsPage;

    document.getElementById("js-audio-page").innerText = "Страница " + (audioPages[audioView] + 1) + " / " + page.pageCount;
    document.getElementById("js-audio-delete-selected").classList.toggle("inactive", audioSelectedIds.size === 0);
    document.getElementById("js-audio-select-all").checked =
        audioData.notifications.length > 0 && audioSelectedIds.size === audioData.notifications.length;
    document.getElementById("js-audio-sort-up").classList.toggle("audio-sort-arrow-active", audioSortAscending);
    document.getElementById("js-audio-sort-down").classList.toggle("audio-sort-arrow-active", !audioSortAscending);

    startAudioNameScrolling();
}

function renderAudioFiles() {
    const container = document.getElementById("js-audio-files-rows");
    const tracks = audioData.tracks.slice().sort(function(a, b) {
        const order = a.name.localeCompare(b.name, "bg");
        return audioSortAscending ? order : -order;
    });
    const page = pageOf(tracks, "files");

    container.innerHTML = "";

    if (tracks.length === 0) {
        container.innerHTML = '<div class="audio-empty">Липсва файл audio.csv или в него няма файлове</div>';
        return page;
    }

    for (const track of page.rows) {
        const isPlaying = track.key === audioData.playing;
        const row = document.createElement("div");
        row.className = "audio-row";
        row.dataset.track = track.key;
        row.appendChild(makeAudioNameCell(track.name, "audio-col-file"));

        const button = document.createElement("button");
        button.type = "button";
        button.className = "audio-icon-button audio-play-button";
        button.dataset.action = isPlaying ? "stop" : "play";
        button.setAttribute("aria-label", isPlaying ? "Спри" : "Пусни");
        button.innerHTML = isPlaying ? AUDIO_STOP_ICON : AUDIO_PLAY_ICON;
        row.appendChild(button);
        container.appendChild(row);
    }

    return page;
}

function renderAudioNotifications() {
    const container = document.getElementById("js-audio-notifications-rows");
    const notifications = audioData.notifications.slice().sort(function(a, b) {
        return a.time === b.time ? a.id - b.id : (a.time < b.time ? -1 : 1);
    });
    const page = pageOf(notifications, "notifications");

    container.innerHTML = "";

    if (notifications.length === 0) {
        container.innerHTML = '<div class="audio-empty">Няма известия</div>';
        return page;
    }

    for (const notification of page.rows) {
        const name = audioTrackName(notification.track);
        const row = document.createElement("div");
        row.className = "audio-row" + (name === null ? " audio-row-missing" : "");
        row.dataset.id = notification.id;
        row.dataset.track = notification.track;

        if (name === null)
            row.title = "Файлът на това известие липсва";

        const check = document.createElement("label");
        check.className = "audio-checkbox audio-col-check";
        check.innerHTML = '<input type="checkbox" class="hidden-element" data-action="select"><span class="audio-checkbox-box"></span>';
        check.querySelector("input").checked = audioSelectedIds.has(notification.id);
        row.appendChild(check);

        row.appendChild(makeAudioNameCell(name === null ? "⚠ Липсващ файл " + notification.track : name, "audio-col-name"));

        const time = document.createElement("div");
        time.className = "audio-col-time";
        time.textContent = notification.time + "  " + formatWeekdays(notification.weekdays);
        row.appendChild(time);

        const buttons = document.createElement("div");
        buttons.className = "audio-col-buttons";
        buttons.innerHTML =
            '<button type="button" class="audio-icon-button" data-action="edit" aria-label="Редактирай">' + AUDIO_EDIT_ICON + "</button>" +
            '<button type="button" class="audio-icon-button" data-action="delete" aria-label="Изтрий">' + AUDIO_DELETE_ICON + "</button>";
        row.appendChild(buttons);
        container.appendChild(row);
    }

    return page;
}

// Names wider than their cell scroll back and forth
function startAudioNameScrolling() {
    for (const text of document.querySelectorAll("#js-audio-views .audio-name-text")) {
        const overflow = text.scrollWidth - text.parentElement.clientWidth;

        text.classList.toggle("audio-name-scroll", overflow > 0);

        if (overflow > 0) {
            text.style.setProperty("--audio-scroll-distance", -overflow + "px");
            text.style.setProperty("--audio-scroll-duration", Math.max(3, overflow / 20) + "s");
        }
    }
}

// ---------------- Table events ----------------
function handleAudioFooterClick(event) {
    const viewButton = event.target.closest(".audio-view-button");
    const pageButton = event.target.closest(".audio-page-button");

    if (viewButton) {
        audioView = viewButton.dataset.view;
    }
    else if (pageButton) {
        const action = pageButton.dataset.page;

        if (action === "first") audioPages[audioView] = 0;
        else if (action === "previous") audioPages[audioView]--;
        else if (action === "next") audioPages[audioView]++;
        else if (action === "last") audioPages[audioView] = Number.MAX_SAFE_INTEGER;
    }
    else
        return;

    if (audioData) renderAudio(); // pageOf() keeps the page inside the range
}

function handleAudioHeaderClick(event) {
    const target = event.target.closest("[data-action]");

    if (!target || !audioData) return;

    const action = target.dataset.action;

    if (action === "sort") {
        audioSortAscending = !audioSortAscending;
        renderAudio();
    }
    else if (action === "select-all" && event.target === target) { // The label's own click is repeated on the input
        audioSelectedIds = target.checked ? new Set(audioData.notifications.map(function(n) { return n.id; })) : new Set();
        renderAudio();
    }
    else if (action === "delete-selected" && audioSelectedIds.size > 0) {
        deleteAudioNotifications([...audioSelectedIds]);
    }
    else if (action === "new") {
        if (audioData.notifications.length >= AUDIO_MAX_NOTIFICATIONS)
            showStatusPopup("Достигнат е максималният брой известия (" + AUDIO_MAX_NOTIFICATIONS + ")!");
        else
            openAudioPopup(null);
    }
}

async function handleAudioRowsClick(event) {
    const target = event.target.closest("[data-action]");
    const row = event.target.closest(".audio-row");

    if (!target || !row || !audioData) return;

    const action = target.dataset.action;

    if (action === "play" || action === "stop") {
        const ok = action === "play"
            ? await sendAudioRequest("/audio-play", "track=" + encodeURIComponent(row.dataset.track))
            : await sendAudioRequest("/audio-stop", "");

        if (ok) {
            audioData.playing = action === "play" ? row.dataset.track : "0";
            renderAudio();
        }
        else if (action === "play")
            requestAudioData(false); // The file list may be outdated
    }
    else if (action === "select" && event.target === target) {
        const id = parseInt(row.dataset.id, 10);

        if (target.checked) audioSelectedIds.add(id);
        else audioSelectedIds.delete(id);

        renderAudio();
    }
    else if (action === "edit") {
        openAudioPopup(audioData.notifications.find(function(n) { return n.id === parseInt(row.dataset.id, 10); }) || null);
    }
    else if (action === "delete") {
        deleteAudioNotifications([parseInt(row.dataset.id, 10)]);
    }
}

async function changeAudioVolume(delta) {
    if (!audioData || audioVolumeRequestPending) return;

    const volume = audioData.volume + delta;

    if (volume < 0 || volume > AUDIO_MAX_VOLUME) return;

    audioVolumeRequestPending = true;

    if (await sendAudioRequest("/audio-volume", "volume=" + volume)) { // The value changes only after the clock confirms
        audioData.volume = volume;
        document.getElementById("js-audio-volume-value").innerText = volume;
    }

    audioVolumeRequestPending = false;
}

async function deleteAudioNotifications(ids) {
    const tracks = ids.map(function(id) {
        return audioData.notifications.find(function(n) { return n.id === id; }).track;
    });
    const params = "ids=" + ids.join(",") + "&tracks=" + tracks.map(encodeURIComponent).join(",");

    if (await sendServerRequest(params, true, "/audio-notification-delete")) {
        for (const id of ids) audioSelectedIds.delete(id);
        audioData.notifications = audioData.notifications.filter(function(n) { return !ids.includes(n.id); });
        renderAudio();
    }

    requestAudioData(false);
}

// ---------------- Notification popup ----------------
function closeAudioPopup() {
    document.getElementById("js-audio-popup-container").classList.remove("show-popup");
}

function openAudioPopup(notification) {
    if (notification) {
        const timeParts = notification.time.split(":");
        audioPopup = {
            editId: notification.id,
            track: audioTrackName(notification.track) === null ? "" : notification.track, // A missing file must be chosen again
            hours: parseInt(timeParts[0], 10),
            minutes: parseInt(timeParts[1], 10),
            weekdays: notification.weekdays
        };
    }
    else
        audioPopup = { editId: 0, track: "", hours: 7, minutes: 0, weekdays: 0 };

    document.getElementById("js-audio-popup-title").innerText = notification ? "Промяна на известие" : "Ново известие";
    renderAudioPopupFiles();
    renderAudioPopupTime();
    renderAudioPopupWeekdays();
    document.getElementById("js-audio-popup-container").classList.add("show-popup");

    const selected = document.querySelector("#js-audio-popup-files .audio-popup-file-selected");

    if (selected && selected.scrollIntoView)
        selected.scrollIntoView({ block: "nearest" });
}

function renderAudioPopupFiles() {
    const list = document.getElementById("js-audio-popup-files");
    const tracks = audioData.tracks.slice().sort(function(a, b) { return a.name.localeCompare(b.name, "bg"); });

    list.innerHTML = "";

    if (tracks.length === 0)
        list.innerHTML = '<div class="audio-empty">Няма файлове</div>';

    for (const track of tracks) {
        const item = document.createElement("div");
        item.className = "audio-popup-file" + (track.key === audioPopup.track ? " audio-popup-file-selected" : "");
        item.dataset.track = track.key;
        item.textContent = track.name;
        list.appendChild(item);
    }
}

function renderAudioPopupTime() {
    document.getElementById("js-audio-popup-hours").textContent = String(audioPopup.hours).padStart(2, "0");
    document.getElementById("js-audio-popup-minutes").textContent = String(audioPopup.minutes).padStart(2, "0");
}

function renderAudioPopupWeekdays() {
    for (const button of document.querySelectorAll("#js-audio-popup-weekdays .audio-weekday"))
        button.classList.toggle("audio-weekday-selected", (audioPopup.weekdays & (1 << parseInt(button.dataset.day, 10))) !== 0);
}

async function saveAudioNotification() {
    const time = String(audioPopup.hours).padStart(2, "0") + ":" + String(audioPopup.minutes).padStart(2, "0");
    const isEdit = audioPopup.editId !== 0;

    // The clock repeats every check
    if (!audioPopup.track || audioTrackName(audioPopup.track) === null) {
        showStatusPopup("Моля изберете файл!");
        return;
    }

    if ((audioPopup.weekdays & AUDIO_ALL_WEEKDAYS) === 0) {
        showStatusPopup("Изберете поне един ден от седмицата!");
        return;
    }

    if (!isEdit && audioData.notifications.length >= AUDIO_MAX_NOTIFICATIONS) {
        showStatusPopup("Достигнат е максималният брой известия (" + AUDIO_MAX_NOTIFICATIONS + ")!");
        return;
    }

    const clash = audioData.notifications.find(function(n) {
        return n.id !== audioPopup.editId && n.time === time && (n.weekdays & audioPopup.weekdays) !== 0;
    });

    if (clash) {
        showStatusPopup("Вече има известие в " + time + " за някой от избраните дни!");
        return;
    }

    const params = "id=" + audioPopup.editId +
                   "&track=" + encodeURIComponent(audioPopup.track) +
                   "&time=" + encodeURIComponent(time) +
                   "&weekdays=" + audioPopup.weekdays +
                   "&edit=" + (isEdit ? 1 : 0);

    if (await sendServerRequest(params, true, "/audio-notification")) {
        closeAudioPopup();
        audioView = "notifications";
        await requestAudioData(false); // The clock assigns the id of a new notification
    }
}

function stepAudioPopupTime(unit, step) {
    if (unit === "hours")
        audioPopup.hours = (audioPopup.hours + step + 24) % 24;
    else
        audioPopup.minutes = (audioPopup.minutes + step + 60) % 60;

    renderAudioPopupTime();
}

initializeAudioAddon();
