/*
 * Rekindled Server
 * Copyright (C) 2021 Tim Leonard
 * Copyright (C) 2026 Jake Morgeson
 *
 * This program is free software; licensed under the MIT license.
 * You should have received a copy of the license along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

let gRefreshStatisticsInterval;
let gRefreshPlayersInterval;
let gRefreshBansInterval;
let gRefreshDebugInterval;

let gActivePlayersChart;

// store received game type so we can rebuild title after language switches
let gCurrentGameType = null;

function updateTitleWithGameType(type) {
    let base = document.querySelector('[data-i18n="html_title"]');
    let prefix = base ? base.textContent : "Rekindled Server";
    let dynamic = `${prefix} - ${type}`;
    document.title = dynamic;
    let hdr = document.querySelector(".mdl-layout-title");
    if (hdr) hdr.textContent = dynamic;
}

// when language changes, refresh title if we know gameType
globalThis.addEventListener("lang-updated", () => {
    if (gCurrentGameType) updateTitleWithGameType(gCurrentGameType);
});

globalThis.onload = function () {
    onDocumentLoaded();
};

// Runs on page, checks authentication and begins refreshing the relevant data.
function onDocumentLoaded() {
    let logoutButton = document.querySelector("#logout-button");
    logoutButton.addEventListener("click", function () {
        storeAuthToken("");
        reauthenticate();
    });

    let playersChart = document.querySelector("#players-chart");
    gActivePlayersChart = new Chart(playersChart, {
        type: "line",
        data: {
            labels: ["00:00"],
            datasets: [
                {
                    label: "Active Players",
                    data: [0],
                    backgroundColor: ["rgba(255, 99, 132, 0.2)"],
                    borderColor: ["rgba(255, 99, 132, 1)"],
                    borderWidth: 1,
                },
            ],
        },
        options: {
            scales: {
                y: {
                    beginAtZero: true,
                },
            },
        },
    });

    checkAuthState();
}

// Checks with the server if we are authenticated.
function checkAuthState() {
    stopDataRefresh();

    fetch("/auth", {
        method: "get",
        headers: {
            "Content-Type": "application/json",
            "Auth-Token": getAuthToken(),
        },
    })
        .then(function (data) {
            if (data.status == 200) {
                startDataRefresh();
            } else {
                reauthenticate();
            }
        })
        .catch(function () {
            reauthenticate();
        });
}

function authenticate(username, password) {
    let dialog = document.querySelector("#auth-dialog");

    fetch("/auth", {
        method: "post",
        headers: {
            "Content-Type": "application/json",
            "Auth-Token": getAuthToken(),
        },
        body: JSON.stringify({ username: username, password: password }),
    })
        .then((response) => {
            return response.json();
        })
        .then(function (data) {
            dialog.close();
            storeAuthToken(data["token"]);

            // record and display game type
            gCurrentGameType = data["gameType"];
            updateTitleWithGameType(gCurrentGameType);
            startDataRefresh();
        })
        .catch(function () {
            dialog.close();
            reauthenticate();
        });
}

// Shows a dialog to the user asking them to authenticate with the server.
function reauthenticate() {
    storeAuthToken("");

    let dialog = document.querySelector("#auth-dialog");
    let button = document.querySelector("#auth-login-button");
    let usernameBox = document.querySelector("#auth-username");

    button.disabled = false;
    if (!dialog.showModal) {
        dialogPolyfill.registerDialog(dialog);
    }
    dialog.showModal();

    // Focus the username field for accessibility
    usernameBox.focus();

    let handler = function () {
        button.removeEventListener("click", handler);
        button.disabled = true;
        authenticate(usernameBox.value, document.querySelector("#auth-password").value);
    };
    button.addEventListener("click", handler);
}

// Starts async loading data to populate the page data.
function startDataRefresh() {
    gRefreshStatisticsInterval = setInterval(refreshStatisticsTab, 5000);
    gRefreshPlayersInterval = setInterval(refreshPlayersTab, 5000);
    gRefreshBansInterval = setInterval(refreshBansTab, 5000);
    gRefreshDebugInterval = setInterval(refreshDebugTab, 5000);

    refreshStatisticsTab();
    refreshPlayersTab();
    refreshBansTab();
    refreshSettingsTab();
    refreshDebugTab();
}

// Stops async loading data for the page data.
function stopDataRefresh() {
    clearInterval(gRefreshStatisticsInterval);
    clearInterval(gRefreshPlayersInterval);
    clearInterval(gRefreshBansInterval);
    clearInterval(gRefreshDebugInterval);
}

// Helper: safely create a table row with text cells
function createTableRow(cells) {
    let tr = document.createElement("tr");
    for (let i = 0; i < cells.length; i++) {
        let td = document.createElement("td");
        let cell = cells[i];
        if (typeof cell === "object" && cell !== null) {
            // HTML element
            td.appendChild(cell);
        } else {
            td.textContent = cell;
        }
        tr.appendChild(td);
    }
    return tr;
}

// Disconnects the given player-id.
function disconnectUser(playerId) {
    apiDelete("/players", { playerId: playerId, ban: false });
}

// Bans the given player-id.
function banUser(playerId) {
    apiDelete("/players", { playerId: playerId, ban: true });
}

// Bans the given steam-id
function removeBan(steamId) {
    apiDelete("/bans", { steamId: steamId });
}

// Sends a message to a given user.
function sendUserMessageInternal(playerId, message) {
    apiPost("/message", { playerId: playerId, message: message });
}

function sendUserMessage(playerId) {
    let dialog = document.querySelector("#send-message-dialog");
    let sendButton = document.querySelector("#send-message-button");
    let cancelButton = document.querySelector("#cancel-send-message-button");
    let messageBox = document.querySelector("#send-message-text");

    if (!dialog.showModal) {
        dialogPolyfill.registerDialog(dialog);
    }
    dialog.showModal();
    messageBox.focus();

    let sendHandler = function () {
        sendButton.removeEventListener("click", sendHandler);
        sendUserMessageInternal(playerId, messageBox.value);
        dialog.close();
    };
    let cancelHandler = function () {
        cancelButton.removeEventListener("click", cancelHandler);
        dialog.close();
    };

    sendButton.addEventListener("click", sendHandler);
    cancelButton.addEventListener("click", cancelHandler);
}

function sendMessageToAllUsers() {
    let dialog = document.querySelector("#send-message-dialog");
    let sendButton = document.querySelector("#send-message-button");
    let cancelButton = document.querySelector("#cancel-send-message-button");
    let messageBox = document.querySelector("#send-message-text");

    if (!dialog.showModal) {
        dialogPolyfill.registerDialog(dialog);
    }
    dialog.showModal();
    messageBox.focus();

    let sendHandler = function () {
        sendButton.removeEventListener("click", sendHandler);
        sendUserMessageInternal(0, messageBox.value);
        dialog.close();
    };
    let cancelHandler = function () {
        cancelButton.removeEventListener("click", cancelHandler);
        dialog.close();
    };

    sendButton.addEventListener("click", sendHandler);
    cancelButton.addEventListener("click", cancelHandler);
}

// Retrieves data from the server to update the statistics tab.
function refreshStatisticsTab() {
    apiGet("/statistics")
        .then(function (data) {
            // Update active players chart.
            let chartLabels = [];
            let chartData = [];

            for (const sample of data.activePlayerSamples) {
                chartLabels.push(sample.time);
                chartData.push(sample.players);
            }

            gActivePlayersChart.data.labels = chartLabels;
            gActivePlayersChart.data.datasets[0].data = chartData;
            gActivePlayersChart.update();

            // Update the statistics list.
            let statisticsTable = document.querySelector("#statistic-table-body");
            statisticsTable.textContent = "";
            for (const stat of data.statistics) {
                statisticsTable.appendChild(createTableRow([stat.name, stat.value]));
            }

            // Update populated areas list.
            let populatedAreasTable = document.querySelector("#populated-areas-table-body");
            populatedAreasTable.textContent = "";
            for (const stat of data.populatedAreas) {
                populatedAreasTable.appendChild(createTableRow([stat.areaName, stat.playerCount]));
            }
        })
        .catch(function () {});
}

// Retrieves data from the server to update the players tab.
function refreshPlayersTab() {
    apiGet("/players")
        .then(function (data) {
            let table = document.querySelector("#players-table-body");
            table.textContent = "";

            for (const player of data.players) {
                let actionsTd = document.createElement("td");
                actionsTd.innerHTML = ""; // safe: we build children ourselves

                let disconnectBtn = document.createElement("button");
                disconnectBtn.textContent = "Disconnect";
                disconnectBtn.className = "mdl-button mdl-js-button mdl-button--raised mdl-button--colored";
                disconnectBtn.addEventListener("click", () => disconnectUser(player.playerId));

                let banBtn = document.createElement("button");
                banBtn.textContent = "Ban";
                banBtn.className = "mdl-button mdl-js-button mdl-button--raised mdl-button--colored";
                banBtn.addEventListener("click", () => banUser(player.playerId));

                let msgBtn = document.createElement("button");
                msgBtn.textContent = "Message";
                msgBtn.className = "mdl-button mdl-js-button mdl-button--raised mdl-button--colored";
                msgBtn.addEventListener("click", () => sendUserMessage(player.playerId));

                actionsTd.appendChild(disconnectBtn);
                actionsTd.appendChild(banBtn);
                actionsTd.appendChild(msgBtn);

                let row = createTableRow([
                    player.characterName,
                    player.soulLevel,
                    player.soulMemory,
                    player.covenant,
                    player.status,
                    player.location,
                    player.playTime,
                    player.connectionTime,
                    player.antiCheatScore,
                    actionsTd,
                ]);

                table.appendChild(row);
            }
        })
        .catch(function () {});
}

// Retrieves data from the server to update the bans tab.
function refreshBansTab() {
    apiGet("/bans")
        .then(function (data) {
            let table = document.querySelector("#bans-table-body");
            table.textContent = "";

            for (const ban of data.bans) {
                let actionsTd = document.createElement("td");
                let removeBtn = document.createElement("button");
                removeBtn.textContent = "Remove";
                removeBtn.className = "mdl-button mdl-js-button mdl-button--raised mdl-button--colored";
                removeBtn.addEventListener("click", () => removeBan(ban.steamId));
                actionsTd.appendChild(removeBtn);

                let row = createTableRow([
                    ban.steamId64,
                    ban.reason,
                    actionsTd,
                ]);

                table.appendChild(row);
            }
        })
        .catch(function () {});
}

// Retrieves data from the server to update the settings tab.
function refreshSettingsTab() {
    apiGet("/settings")
        .then(function (data) {
            setMaterialTextField(document.querySelector("#server-name"), data.serverName);
            setMaterialTextField(document.querySelector("#server-description"), data.serverDescription);
            setMaterialTextField(document.querySelector("#server-password"), data.password);
            setMaterialTextField(document.querySelector("#server-private-hostname"), data.privateHostname);
            setMaterialTextField(document.querySelector("#server-public-hostname"), data.publicHostname);

            setMaterialCheckState(document.querySelector("#advertise"), data.advertise);
            setMaterialCheckState(document.querySelector("#disable-coop"), data.disableCoop);
            setMaterialCheckState(document.querySelector("#disable-blood-messages"), data.disableBloodMessages);
            setMaterialCheckState(document.querySelector("#disable-blood-stains"), data.disableBloodStains);
            setMaterialCheckState(document.querySelector("#disable-ghosts"), data.disableGhosts);
            setMaterialCheckState(document.querySelector("#disable-invasions"), data.disableInvasions);
            setMaterialCheckState(document.querySelector("#disable-auto-summon-coop"), data.disableAutoSummonCoop);
            setMaterialCheckState(document.querySelector("#disable-auto-summon-invasions"), data.disableAutoSummonInvasions);
            setMaterialCheckState(document.querySelector("#disable-weapon-level-matching"), data.disableWeaponLevelMatching);
            setMaterialCheckState(document.querySelector("#disable-soul-level-matching"), data.disableSoulLevelMatching);
            setMaterialCheckState(document.querySelector("#disable-soul-memory-matching"), data.disableSoulMemoryMatching);
            setMaterialCheckState(document.querySelector("#ignore-invasion-area-filter"), data.ignoreInvasionAreaFilter);
            setMaterialCheckState(document.querySelector("#anti-cheat-enabled"), data.antiCheatEnabled);
            setAnnouncements(document.querySelector("#announcements"), data.announcements);
        })
        .catch(function () {});
}

// Posts all the settings to the server.
function saveSettings() {
    apiPost("/settings", {
        serverName: document.querySelector("#server-name").value,
        serverDescription: document.querySelector("#server-description").value,
        password: document.querySelector("#server-password").value,
        privateHostname: document.querySelector("#server-private-hostname").value,
        publicHostname: document.querySelector("#server-public-hostname").value,
        advertise: document.querySelector("#advertise").checked,
        disableCoop: document.querySelector("#disable-coop").checked,
        disableBloodMessages: document.querySelector("#disable-blood-messages").checked,
        disableBloodStains: document.querySelector("#disable-blood-stains").checked,
        disableGhosts: document.querySelector("#disable-ghosts").checked,
        disableInvasions: document.querySelector("#disable-invasions").checked,
        disableAutoSummonCoop: document.querySelector("#disable-auto-summon-coop").checked,
        disableAutoSummonInvasions: document.querySelector("#disable-auto-summon-invasions").checked,
        disableWeaponLevelMatching: document.querySelector("#disable-weapon-level-matching").checked,
        disableSoulLevelMatching: document.querySelector("#disable-soul-level-matching").checked,
        disableSoulMemoryMatching: document.querySelector("#disable-soul-memory-matching").checked,
        ignoreInvasionAreaFilter: document.querySelector("#ignore-invasion-area-filter").checked,
        antiCheatEnabled: document.querySelector("#anti-cheat-enabled").checked,
        announcements: getAnnouncements(document.querySelector("#announcements")),
    });
}

function deleteThisAnnouncementBlock(button) {
    let parentElement = button.parentNode;
    parentElement.remove();
}

function createNewAnnouncementBlock(where) {
    let element = document.querySelector("#announcements");
    let index = element.children.length;

    const classes = [
        "mdl-textfield",
        "mdl-js-textfield",
        "mdl-textfield--floating-label",
        "fullWidth",
    ];

    let nab = document.createElement("div");
    nab.classList.add("fullWidth");
    nab.id = `announcement-${index}`;

    let addBefore = document.createElement("button");
    addBefore.textContent = String.fromCodePoint(0x2795);
    addBefore.onclick = function () {
        createNewAnnouncementBlock(nab);
    };
    addBefore.title = "Add an announcement block before this one";
    nab.appendChild(addBefore);

    let announcementHeader = document.createElement("div");
    announcementHeader.classList.add(...classes);
    let headerInput = document.createElement("input");
    headerInput.className = "mdl-textfield__input";
    headerInput.type = "text";
    headerInput.id = `announcement-${index}-header`;
    let headerLabel = document.createElement("label");
    headerLabel.className = "mdl-textfield__label";
    headerLabel.htmlFor = `announcement-${index}-header`;
    headerLabel.textContent = "Announcement Header";
    announcementHeader.appendChild(headerInput);
    announcementHeader.appendChild(headerLabel);
    nab.appendChild(announcementHeader);

    let announcementBody = document.createElement("div");
    announcementBody.classList.add(...classes);
    let bodyTextarea = document.createElement("textarea");
    bodyTextarea.className = "mdl-textfield__input";
    bodyTextarea.rows = "5";
    bodyTextarea.cols = "80";
    bodyTextarea.id = `announcement-${index}-body`;
    let bodyLabel = document.createElement("label");
    bodyLabel.className = "mdl-textfield__label";
    bodyLabel.htmlFor = `announcement-${index}-body`;
    bodyLabel.textContent = "Announcement Body";
    announcementBody.appendChild(bodyTextarea);
    announcementBody.appendChild(bodyLabel);
    nab.appendChild(announcementBody);

    let deleteButton = document.createElement("button");
    deleteButton.textContent = String.fromCodePoint(0x1f5d1);
    deleteButton.onclick = function () {
        nab.remove();
    };
    deleteButton.title = "Delete this announcement block";
    nab.appendChild(deleteButton);

    nab.appendChild(document.createElement("hr"));

    if (where == null) {
        element.appendChild(nab);
    } else {
        where.before(nab);
    }

    componentHandler.upgradeElement(announcementHeader);
    componentHandler.upgradeElement(announcementBody);
}

function setAnnouncements(element, announcements) {
    element.textContent = "";
    let i = 0;
    for (const announcement of announcements) {
        createNewAnnouncementBlock(null);
        const announcementHeader = document.querySelector(`#announcement-${i}-header`);
        announcementHeader.value = announcement.header;
        announcementHeader.parentElement.classList.add("is-dirty");
        const announcementBody = document.querySelector(`#announcement-${i}-body`);
        announcementBody.value = announcement.body;
        announcementBody.parentElement.classList.add("is-dirty");
        i += 1;
    }
}

function getAnnouncements(element) {
    let announcementList = [];
    for (const child of element.children) {
        const announcementHeader = document.querySelector(`#${child.id}-header`);
        const announcementBody = document.querySelector(`#${child.id}-body`);
        announcementList.push({
            header: announcementHeader.value,
            body: announcementBody.value,
        });
    }
    return announcementList;
}

function setMaterialCheckState(element, state) {
    if (element.checked != state) {
        if (state) {
            element.parentNode.MaterialSwitch.on();
        } else {
            element.parentNode.MaterialSwitch.off();
        }
    }
}

function setMaterialTextField(element, text) {
    element.parentNode.MaterialTextfield.change(text);
}

// Retrieves data from the server to update the debug statistics tab.
function refreshDebugTab() {
    apiGet("/debug_statistics")
        .then(function (data) {
            let timerTable = document.querySelector("#debug-timer-table-body");
            let counterTable = document.querySelector("#debug-counter-table-body");
            let logTable = document.querySelector("#debug-log-table-body");

            // Update the timer list.
            timerTable.textContent = "";
            for (const stat of data.timers) {
                timerTable.appendChild(createTableRow([stat.name, stat.current, stat.average, stat.peak]));
            }

            // Update the counter list.
            counterTable.textContent = "";
            for (const stat of data.counters) {
                counterTable.appendChild(createTableRow([stat.name, stat.average_rate, stat.total_lifetime]));
            }

            // Update the debug log list.
            logTable.textContent = "";
            for (const stat of data.logs) {
                logTable.appendChild(createTableRow([stat.level, stat.source, stat.message]));
            }
        })
        .catch(function () {});
}
