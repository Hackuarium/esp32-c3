// we use a prefix if the parameters do not start at 'A' but at 'BA' for example.
const prefix = "B";

// const server = "http://192.168.1.178/";
const server = "/";
let servers = [server];
const urlParams = new URLSearchParams(window.location.search);
if (urlParams && urlParams.get("servers")) {
  servers = urlParams
    .get("servers")
    .split(",")
    .map((server) => "/" + server + "/");
}

let timerId = undefined;
let lastEvent = Date.now();
let throttle = 200;
async function sendSlowlyCommand(command) {
  clearTimeout(timerId);
  if (Date.now() - lastEvent > throttle) {
    lastEvent = Date.now();
    sendCommand(command);
  } else {
    timerId = setTimeout(() => {
      lastEvent = Date.now();
      sendCommand(command);
    }, throttle - (Date.now() - lastEvent));
  }
}

async function getCurrentSettings() {
  const response = await fetch(servers[0] + "command" + "?value=uc");
  const result = await response.text();
  const parameters = [];
  for (let i = 0; i < Math.min(result.length, 60); i = i + 4) {
    parameters.push(parseInt(result.substr(i, 4), 16));
  }
  document.getElementById("result").value = "A" + parameters.join(",");
  return result;
}

async function sendCommand(command) {
  console.log(command);
  if (command.match(/^[A-Z][0-9]+$/)) {
    let elements = document.querySelectorAll(
      `[data-label=${command.substring(0, 1)}]`
    );
    for (let element of elements) {
      element.value = command.substring(1);
    }
  }
  if (command.match(/^[A-Z][0-9,]+$/)) {
    command = prefix + command;
  }

  const results = [];

  for (let server of servers) {
    const response = await fetch(
      server + "command" + "?value=" + encodeURIComponent(command)
    );
    results.push(await response.text());
  }
  document.getElementById("result").value = results.join("\n");
  return results.join("\n");
}
async function demoFunction(functionStr) {
  document.getElementById("functionStr").value = functionStr;
  sendFunction();
}
async function sendFunction() {
  const functionStr = document.getElementById("functionStr").value;
  const results = [];
  for (let server of servers) {
    const response = await fetch(
      server + "function" + "?value=" + encodeURIComponent(functionStr)
    );
    results.push(await response.text());
  }
  document.getElementById("result").value = results.join("\n");
}
async function reloadSettings() {
  let result = await sendCommand("uc");
  // TODO we could check the checkDigit ...

  if (prefix) {
    const shift = (prefix.charCodeAt(0) - 64) * 26 * 4 + 8;
    result = result.substring(shift, shift + 104);
  } else {
    result = result.substring(8); // first 8 symbols is epoch
  }

  for (let i = 0; i < result.length; i = i + 4) {
    let code = String.fromCharCode(65 + i / 4);
    let value = parseInt(result.substring(i, i + 4), 16);
    let elements = document.querySelectorAll(
      `[data-label="${code}"]:not([type="radio"])`
    );
    for (let element of elements) {
      element.value = value;
    }
    // radio button
    elements = document.querySelectorAll(
      `[data-label="${code}"][value="${value}"]`
    );
    for (let element of elements) {
      element.setAttribute("checked", "checked");
    }
  }
  // color button (in KLM)
  let elements = document.querySelectorAll(`[data-label="KLM"]`);
  for (let element of elements) {
    element.value =
      "#" + result.substr(42, 2) + result.substr(46, 2) + result.substr(50, 2);
  }
}
reloadSettings();
