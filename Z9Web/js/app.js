const SERVICE_APP_ID = "DaCMXXuEWw.z9service";

// state observables
var lat = ko.observable(0);
var lng = ko.observable(0);
var inBox = ko.observable(false);
var runStartEpoch = ko.observable(0);
var lastPausedEpoch = ko.observable(0);
var doNotRegisterLapUntilEpoch = ko.observable(0);
var totalPausedSec = ko.observable(0);
var minTimeBetweenLaps = ko.observable(0);
var lapCount = ko.observable(0);
var active = ko.observable(false);
// UI specific
var showConfirmResetModal = ko.observable(false);
var totalRunTime = ko.observable(0);

const bundleToObject = (arr) => {
    // We get this stupid arrays like this:
    // [ { key:"lat", value: "35.2342342" }, { key:"lng", value: "-88.324234" } ]
    let obj = {};
    arr.forEach(item => {
        obj[item.key] = item.value;
    })
    return obj;
}

const populateFromState = (state) => {
    // Remember: state brings everything over in STRINGS
    lat(parseInt(state.lat));
    lng(parseInt(state.lng));
    inBox(state.inBox === 'true');
    // NOTE: these are all SECONDS!!! NOT MS!
    runStartEpoch(parseInt(state.runStartEpoch));
    lastPausedEpoch(parseInt(state.lastPausedEpoch));
    doNotRegisterLapUntilEpoch(parseInt(state.doNotRegisterLapUntilEpoch));
    totalPausedSec(parseInt(state.totalPausedSec));
    minTimeBetweenLaps(parseInt(state.minTimeBetweenLaps));
    lapCount(parseInt(state.lapCount));
    active(state.active === 'true');
    // Now...to trigger everything:
    totalRunTime(getTotalRunTime());
}

const currTime = () => {
    return Math.round(Date.now() / 1000);
}
const secToMinSec = (sec) => {
    const m = Math.floor(sec / 60);
    const s = sec - (m * 60);
    return m + ':' + (s < 10 ? '0' + s : s);
}

// const msToMinSec = (ms) => secToMinSec(Math.round(t / 1000));


const getTotalRunTime = () => {
    if (active()) {
        return (currTime() - runStartEpoch()) - totalPausedSec();
    } else {
        // paused
        if (runStartEpoch() === 0) {
            // never started
            return 0;
        } else {
            return (lastPausedEpoch() - runStartEpoch()) - totalPausedSec();
        }
    }
}

var totalRunTimeStr = ko.computed(function() {
    return secToMinSec(totalRunTime());
})

var totalPausedStr = ko.computed(function() {
    return secToMinSec(totalPausedSec());
})

var paceStr = ko.computed(function() {
    const laps = lapCount();
    const total = totalRunTime();
    if (laps === 0) return '0:00';
    return secToMinSec(Math.round(total / laps));
})

var startBtnText = ko.computed(function() {
    if (runStartEpoch() === 0) return 'Start';
    return 'Resume';
})

const sendCommand = (x) => {
    const remotePort = tizen.messageport.requestRemoteMessagePort(SERVICE_APP_ID, 'GUG');
    const bundle = [{ key: "command", value: x }];
    remotePort.sendMessage(bundle);
}

const resetRun = () => {
    sendCommand("reset"); // this auto-starts after resetting.
    showConfirmResetModal(false);
}
const pauseRun = () => {
	sendCommand("pause");
}
const startOrResumeRun = () => {
	sendCommand("resume");	// this will start if never started.
}
const refreshState = () => {
	sendCommand("refresh");
}

const showStartOverModal = () => {
    showConfirmResetModal(true);
}
function nevermind() {
    showConfirmResetModal(false);
}


document.addEventListener("tizenhwkey", (event) => {
    if (event.keyName === "back") {
        try {
            // If the back key is pressed, exit the application.
            // sendCommand("pause");
            tizen.application.getCurrentApplication().exit();
        } catch (ignore) {}
    }
})

function pig(s) {
    const t = document.getElementById('shit');
    if (!t) return;
    t.value = t.value + '\n' + s;
}


window.onload = () => {
    // alert('onload');
    const localPort = tizen.messageport.requestLocalMessagePort('PIG');
    localPort.addMessagePortListener(function(data, replyPort) {
        const obj = bundleToObject(data);
        // try {
            // postData2('http://jeffrad.com:5000', obj);
        // } catch(err) {
            // alert('error posting data');
        // }
        if (obj.err) {
            // error object
        } else if (obj.isState) {
            populateFromState(obj);
        }
    })

    try {
        tizen.application.launch(SERVICE_APP_ID, () => {
            // alert('zservice launched');
            refreshState();
        }, (err) => {
            alert('zerservice ERROR');
        })
    } catch (exc) {
        // showAlert('Exception while launching HybridServiceApp:<br>' +exc.message);
        alert('ERROR');
    }
    ko.applyBindings();
    document.getElementById('main').style.display = 'block';
}

window.onfocus = () => {
    // alert('onfocus');
}