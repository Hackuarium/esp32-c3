// we use a prefix if the parameters do not start at 'A' but at 'BA' for example.
const prefix = "B";

// const server = "http://192.168.1.178/";
const server = "";
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
async function sendSlowlyCommand(command, value) {
  clearTimeout(timerId);
  if (Date.now() - lastEvent > throttle) {
    lastEvent = Date.now();
    sendCommand(command, value);
  } else {
    timerId = setTimeout(() => {
      lastEvent = Date.now();
      sendCommand(command, value);
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

async function sendCommand(command, value) {
  console.log(command);
  if (command.match(/^[A-Z][0-9]+$/)) {
    let elements = document.querySelectorAll(
      `[data-label=${command.substring(0, 1)}]`
    );
    for (let element of elements) {
      element.value = value || command.substring(1);
    }
  }
  if (command.match(/^[A-Z][0-9,]+$/)) {
    command = prefix + command;
  }

  const results = [];

  for (let server of servers) {
    try {
      const response = await fetch(
        server + "command" + "?value=" + encodeURIComponent(command)
      );
      results.push(await response.text());
    } catch (e) {
      console.log("Can not access: " + server);
    }
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
    for (const element of elements) {
      if (element.getAttribute("type") === "color") {
        // color on 15 bits
        element.value = color12ToHex(value);
      } else {
        element.value = value;
      }
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

function colorHexTo12(value) {
  const rgb = value.match(/[A-Za-z0-9]{2}/g).map((v) => parseInt(v, 16));
  const color =
    ((rgb[0] >> 4) << 8) + ((rgb[1] >> l) << 4) + ((rgb[2] >> 4) << 0);
  return color;
}

function color12ToHex(value) {
  const result =
    "#" +
    ("00" + Number((value & 0xf00) >> 4).toString(16)).slice(-2) +
    ("00" + Number((value & 0x0f0) >> 0).toString(16)).slice(-2) +
    ("00" + Number((value & 0x00f) << 4).toString(16)).slice(-2);
  console.log(value, result);
  return result;
}

function addColorModelsButtons(models) {
  const modelsElement = document.getElementById("models");
  if (!models || !modelsElement) return;
  for (const model of models) {
    const colors = model.map((color) => color12ToHex(color)).join(",");
    const button = document.createElement("button");
    button.setAttribute("class", "text-xl w-1/5 text-white");
    button.style.setProperty(
      "background-image",
      "linear-gradient(to right, " + colors + ")"
    );
    const command =
      "P" + model.slice(0, 4).join(",") + "V" + model.slice(4, 8).join(",");
    button.onmousedown = () => {
      sendCommand(command);
    };
    modelsElement.appendChild(button);
  }
}
reloadSettings();
