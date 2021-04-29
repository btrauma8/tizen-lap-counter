var view = ko.observable('pace');
var started = ko.observable(false);
var currPace = ko.observable('--:--');
var dist = ko.observable('00:00');
var startTime = ko.observable(0);
var overallPace = ko.observable('--:--');
var totalTime = ko.observable('00:00');

const kmToMiles = (km) => km * 0.621371;
const mToMiles = (m) => m * 0.000621371;
// const toMeters = (x) => x * 1609.344; // miles to meters
const round3 = number => (Math.round(number * 1000) / 1000);

const secToMinSec = (sec) => {
    const m = Math.floor(sec / 60);
    const s = sec - (m * 60);
    return m + ':' + (s < 10 ? '0' + s : s);
}

const onPedChange = (x) => {

    if (!started()) return;

    tizen.humanactivitymonitor.getHumanActivityData("PEDOMETER", (info) => {

        const timeElapsedSec = (Date.now() - startTime()) / 1000;

        const kmPerHr = info.speed;
        if (kmPerHr > 0) {
            const milesPerHr = kmPerHr * 0.621371;
            const secPerMile = Math.round((60 * 60) / milesPerHr);
            currPace(secToMinSec(secPerMile));
        } else {
            currPace(secToMinSec(0));
        }

        const totalMiles = mToMiles(info.cumulativeDistance);
        overallPace(secToMinSec(totalMiles > 0 ? Math.round(timeElapsedSec / totalMiles) : 0));
        dist(totalMiles.toFixed(2));

    }, (err) => {
        currPace(String(err));
        overallPace('err');
        dist(-1);
    })
}

setInterval(() => {
    if (!started()) return;
    const elapsedSec = Math.round((Date.now() - startTime()) / 1000);
    totalTime(secToMinSec(elapsedSec));
}, 300);

const start = () => {
    currPace('-:--');
    overallPace('-:--');
    dist('00:00');
    totalTime('00:00');
    startTime(Date.now());
    started(true);
    tizen.humanactivitymonitor.start('PEDOMETER', onPedChange);
    view('pace');
}

const doStop = () => {
    started(false);
    tizen.humanactivitymonitor.stop('PEDOMETER');
    view('pace');
}

window.onload = function () {
	
    document.addEventListener('tizenhwkey', function(e) {
        if (e.keyName == "back") {
            // go back to viewing pace!
            view('pace');
            // if (started()) {
            // }
        	try {
        		// tizen.application.getCurrentApplication().exit();
        	} catch (ignore) {
        	}
        }
    })

    ko.applyBindings();
    document.getElementById('main').style.display = 'block';

}
