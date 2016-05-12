var initialised = false;

function appMessageAck(e) {
    console.log("options sent to Pebble successfully");
}

function appMessageNack(e) {
    console.log("options not sent to Pebble: " + e.error.message);
}

Pebble.addEventListener("ready", function() {
    initialised = true;
});

Pebble.addEventListener("showConfiguration", function() {
    var options = JSON.parse(localStorage.getItem('mid_pla_opt'));
    console.log("read options: " + JSON.stringify(options));
    console.log("showing configuration");
	var uri = 'http://panicman.github.io/config_midpla.html?title=Planetarium%20v3.0';
    if (options !== null) {
        uri +=
			'&inv=' + encodeURIComponent(options.inv) + 
			'&anim=' + encodeURIComponent(options.anim) + 
			'&stars=' + encodeURIComponent(options.stars) +
			'&vibr=' + encodeURIComponent(options.vibr) +
			'&astro=' + encodeURIComponent(options.astro) +
			'&infr=' + encodeURIComponent(options.infr) +
			'&date=' + encodeURIComponent(options.date); 
    }
	console.log("Uri: "+uri);
    Pebble.openURL(uri);
});

Pebble.addEventListener("webviewclosed", function(e) {
    console.log("configuration closed");
    if (e.response !== '') {
        var options = JSON.parse(decodeURIComponent(e.response));
        console.log("storing options: " + JSON.stringify(options));
        localStorage.setItem('mid_pla_opt', JSON.stringify(options));
        Pebble.sendAppMessage(options, appMessageAck, appMessageNack);
    } else {
        console.log("no options received");
    }
});
