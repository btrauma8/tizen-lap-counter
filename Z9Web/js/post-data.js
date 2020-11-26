function postData(url, data) {
    // Default options are marked with *
    // const response = await 
    fetch(url, {
        method: 'POST', // *GET, POST, PUT, DELETE, etc.
        // mode: 'cors', // no-cors, *cors, same-origin
        // cache: 'no-cache', // *default, no-cache, reload, force-cache, only-if-cached
        // credentials: 'same-origin', // include, *same-origin, omit
        headers: { 'Content-Type': 'application/json' },
        // redirect: 'follow', // manual, *follow, error
        // referrerPolicy: 'no-referrer', // no-referrer, *no-referrer-when-downgrade, origin, origin-when-cross-origin, same-origin, strict-origin, strict-origin-when-cross-origin, unsafe-url
        body: JSON.stringify(data) // body data type must match "Content-Type" header
    })
    // return response.json(); // parses JSON response into native JavaScript objects
}

function postData2(url, data) {
    var xhr = new XMLHttpRequest();
    xhr.open("post", url, true);
    xhr.setRequestHeader("Accept", "text/json");
    xhr.setRequestHeader('Content-Type', 'application/json');
    // xhr.onreadystatechange = function() {
    //     if (xhr.readyState === 4 && xhr.status === 200) {
    //         document.getElementById("content-text").textContent=xhr.responseText;
    //     }
    //     else {
    //         document.getElementById("content-text").textContent=xhr.readyState +":"+ xhr.status+":"+xhr.responseText;
    //         //document.getElementById("content-text").innerHTML=xhr.responseText;
    //     }
    // };
    xhr.send(JSON.stringify(data));
}