// we use a prefix if the parameters do not start at 'A' but at 'BA' for example.

//const server = "http://192.168.4.1/";
//const server = "http://192.168.1.222/";
const server = "http://192.168.1.197/";
//const server = "http://192.168.1.107/";
//const server = "";
let mqttServer = "";
//const mqttServer = "https://mqtt.patiny.com/publish.cgi";
let topics = ["line"];
let servers = [server];
const urlParams = new URLSearchParams(window.location.search);
if (urlParams) {
  if (urlParams.get("servers")) {
    servers = urlParams
      .get("servers")
      .split(",")
      .map((server) => "/" + server + "/");
  }
  if (urlParams.get("mqttServer")) {
    mqttServer = urlParams.get("mqttServer");
  }
  if (urlParams.get("topics")) {
    topics = urlParams.get("topics").split(",");
  }
}
console.log(mqttServer, topics, servers);
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

function download(data) {
  const blob = new Blob([data], { type: 'text/csv' });
  const elem = window.document.createElement('a');
  elem.href = window.URL.createObjectURL(blob);
  elem.download = Date.now() + '.csv';
  document.body.appendChild(elem);
  elem.click();
  document.body.removeChild(elem);
}

async function updateGIFList() {
  const results = await sendCommand('fd');
  const lines = results.split('\n').filter(line => line.includes('.gif') && !line.includes('weather'))
  const gifs = lines.map(line => {
    const fields = line.trim().split(' ');
    return {
      size: fields[0],
      filename: fields[1],
      name: fields[1].replace(/.*\//, ''),
    }
  })
  const buttons = gifs.map(gif => `<button value="${gif.filename}">${gif.name}</button>`).join('')
  document.getElementById('gifButtons').innerHTML = buttons
  setGIF(gifs[0].filename)
}

function setGIF(gif) {
  sendCommand('pg' + gif)
  console.log({ gif })
}


async function setServo1(status) {
  const settings = await getSettings();
  const off = settings[19]
  const on = settings[20]
  sendCommand('I' + (status ? on : off))
}

async function getSettings() {
  const response = await fetch(servers[0] + "command" + "?value=uc");
  let result = await response.text();
  if (prefix) {
    const shift = (prefix.charCodeAt(0) - 64) * 26 * 4 + 8;
    result = result.substring(shift, shift + 104);
  } else {
    result = result.substring(8); // first 8 symbols is epoch
  }
  const parameters = [];
  for (let i = 0; i < Math.min(result.length, 104); i = i + 4) {
    parameters.push(parseInt(result.substr(i, 4), 16));
  }
  return parameters;
}

async function getCurrentSettings() {
  let parameters = await getSettings();
  parameters = parameters.slice(0, 15);
  document.getElementById("result").value = "A" + parameters.join(",");
  return result;
}

async function getCurrentLineColor() {
  let parameters = await getSettings();
  const colors =
    "[" +
    parameters
      .slice(15, 19)
      .concat(parameters.slice(21, 25))
      .map((value) => "0x" + ("000" + value.toString(16)).slice(-3))
      .join(", ") +
    "],";
  document.getElementById("result").value = colors;
  // [0xf00, 0x0f0, 0x00f, 0x000 ,0xf00, 0x0f0, 0x00f, 0x000 ],

  return colors;
}

async function sendCommand(command, value, options = {}) {
  const { logResult = true } = options;
  if (command.match(/^[A-Z][0-9-]+$/)) {
    let elements = document.querySelectorAll(
      `[data-label=${command.substring(0, 1)}]`
    );
    for (let element of elements) {
      element.value = value || command.substring(1);
    }
  }
  if (command.match(/^[A-Z][0-9]+/)) {
    // split before uppercase without capture
    const parts = command.split(/(?=[A-Z])/).map(part => prefix + part);
    command = parts.join('');
  }
  console.log(command)

  const results = [];

  if (mqttServer) {
    // command must start with uppercase
    if (command.match(/^[A-Z]/)) {
      for (let topic of topics) {
        try {
          let response = await fetch(
            mqttServer + "?topic=" + encodeURIComponent(topic) + "&message=" + encodeURIComponent(command)
          );
          results.push(await response.text());
        } catch (e) {
          console.log("Can not access: " + server);
        }
      }
    }
  } else {
    for (let server of servers) {
      try {
        let response = await fetch(
          server + "command" + "?value=" + encodeURIComponent(command)
        );
        results.push(await response.text());
      } catch (e) {
        console.log("Can not access: " + server);
      }
    }
  }
  if (logResult) document.getElementById("result").value = results.join("\n");
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
  let result = await sendCommand("uc", undefined, { logResult: false });
  // TODO we could check the checkDigit ...
  if (prefix) {
    const shift = (prefix.charCodeAt(0) - 64) * 26 * 4 + 8;
    result = result.substring(shift, shift + 104);
  } else {
    result = result.substring(8); // first 8 symbols is epoch
  }

  for (let i = 0; i < result.length; i = i + 4) {
    if (i / 4 > 25) continue;
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
        if (value > 32767) {
          value = value - 65536;
        }
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
    ((rgb[0] >> 4) << 8) + ((rgb[1] >> 4) << 4) + ((rgb[2] >> 4) << 0);
  return color;
}

function color12ToHex(value) {
  const result =
    "#" +
    ("00" + Number((value & 0xf00) >> 4).toString(16)).slice(-2) +
    ("00" + Number((value & 0x0f0) >> 0).toString(16)).slice(-2) +
    ("00" + Number((value & 0x00f) << 4).toString(16)).slice(-2);
  return result;
}

async function delay(millis) {
  return new Promise((resolve) => {
    setTimeout(resolve, millis);
  });
}

function addColorModelsButtons(models) {
  const modelsElement = document.getElementById("models");
  if (!models || !modelsElement) return;
  for (const model of models) {
    const colors = model.map((color) => color12ToHex(color)).join(",");
    const button = document.createElement("button");
    button.setAttribute("class", "text-xl w-1/4 h-8 text-white");
    button.style.setProperty(
      "background-image",
      "linear-gradient(to right, " + colors + ")"
    );
    button.onmousedown = async () => {
      await sendCommand("D11,P" + model.slice(0, 4).join(",") + ',V' + model.slice(4, 8).join(","));
      await reloadSettings();
    };
    modelsElement.append(button);
  }
}
reloadSettings();
