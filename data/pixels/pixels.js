import { reapplySettings, color12ToHex } from "./reapplySettings.js";

export class Pixels {
  constructor() {
    this.mqttServers = [];
    this.mqttTopics = [];
    this.servers = [];
    this.parameters = [];
    this.init();

    this.state = this.initState().then(() => {
      reapplySettings(this.parameters);
    });
    this.addListeners();

    // if there is some mqttServers we need to add in the class name 'onlyHTTP' with the property display: none
    // to hide the mqtt part
    const style = document.createElement("style");
    if (this.mqttServers.length > 0) {
      style.innerHTML = "[data-only-http] {display: none}";
    } else {
      style.innerHTML = "[data-only-mqtt] {display: none}";
    }
    document.head.appendChild(style);
  }

  async getParameters() {
    await this.state;
    return this.parameters;
  }

  init() {
    const urlParams = new URLSearchParams(window.location.search);
    if (urlParams) {
      if (urlParams.get("servers")) {
        this.servers = urlParams.get("servers").split(",");
      }
      if (urlParams.get("mqttTopics")) {
        this.mqttTopics = urlParams.get("mqttTopics").split(",");
      }
      if (urlParams.get("mqttServers")) {
        this.mqttServers = urlParams.get("mqttServers").split(",");
      }
    }
    if (this.servers.length === 0) {
      this.servers.push("");
    }
  }

  // we will always keep in memory the current state
  async initState() {
    return this.fetchAllParameters();
  }

  async fetchAllParameters() {
    if (!this.servers?.[0]) return;
    const response = await fetch(this.servers[0] + "command" + "?value=uc");
    let string = (await response.text()).substring(8); // first 8 symbols is epoch
    for (let i = 0; i < string.length / 4; i++) {
      let code = String.fromCharCode(65 + (i % 26));
      if (i > 25) {
        code = String.fromCharCode(64 + Math.floor(i / 26)) + code;
      }
      // need to deal with uint16 and convert hex to positive or netaive number
      let value = parseInt(string.slice(i * 4, (i + 1) * 4), 16);
      if (value > 32767) value -= 65536;

      this.parameters[i] = {
        i,
        code,
        value,
      };
    }
  }

  addListeners() {
    let elements = document.querySelectorAll('input[type="range"][data-label]');
    for (let element of elements) {
      element.oninput = (event) => {
        const value = event.target.value;
        const label = event.target.getAttribute("data-label");
        this.sendSlowlyCommand(label + value);
      };
    }
    elements = document.querySelectorAll("select[data-label]");
    for (let element of elements) {
      element.onchange = (event) => {
        const value = event.target.value;
        const label = event.target.getAttribute("data-label");
        this.sendCommand(label + value);
      };
    }
    // deal with the color pickers
    elements = document.querySelectorAll('input[type="color"][data-label]');
    for (let element of elements) {
      element.oninput = (event) => {
        const value = event.target.value;
        const isRGB = event.target.getAttribute("data-rgb");
        const labels = event.target.getAttribute("data-label").split(",");
        if (isRGB) {
          this.sendSlowlyCommand(
            labels[0] +
              value
                .match(/[A-Za-z0-9]{2}/g)
                .map((v) => parseInt(v, 16))
                .join(",")
          );
        } else {
          const color = colorHexTo12(value);
          this.sendSlowlyCommand(
            labels.map((label) => label + color).join(",")
          );
        }
      };
    }
    // listener for number
    elements = document.querySelectorAll('input[type="number"][data-label]');
    for (let element of elements) {
      element.oninput = (event) => {
        const value = event.target.value;
        const label = event.target.getAttribute("data-label");
        this.sendCommand(label + value);
      };
    }
  }

  reapplySettings() {
    reapplySettings(this.parameters);
  }

  async sendCommand(command, value, options = {}) {
    const { logResult = true } = options;
    const results = [];

    if (this.mqttServers.length > 0) {
      // command must start with uppercase
      if (command.match(/^[A-Z]/)) {
        for (const mqttServer of this.mqttServers) {
          for (let topic of this.mqttTopics) {
            try {
              let response = await fetch(
                mqttServer +
                  "?topic=" +
                  encodeURIComponent(topic) +
                  "&message=" +
                  encodeURIComponent(command)
              );
              results.push(await response.text());
            } catch (e) {
              console.log("Can not access: " + mqttServer);
            }
          }
        }
      }
    }
    if (
      this.servers.length > 0 &&
      (this.mqttServers.length === 0 || !command.match(/^[A-Z]/))
    ) {
      for (let server of this.servers) {
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
    this.updateParameters(command);
    reapplySettings(this.parameters);

    if (logResult) document.getElementById("result").value = results.join("\n");
    return results.join("\n");
  }

  updateParameters(command) {
    if (!command.match(/^[A-Z][A-Z0-9,]+$/)) return;
    // need to parse something like A12,34,BA1,2,3
    const parts = command.split(/,(?=[A-Z])/);
    for (const part of parts) {
      const label = part.replace(/[0-9,]/g, "");
      const values = part.replace(/[A-Z]/g, "").split(",");
      let firstIndex = label.charCodeAt(0) - 65;
      if (label.length === 2) {
        firstIndex = 26 + firstIndex * 26 + label.charCodeAt(1) - 65;
      }
      for (let i = 0; i < values.length; i++) {
        this.parameters[firstIndex + i].value = Number(values[i]);
      }
    }
  }

  async sendSlowlyCommand(command, value) {
    this.lastEvent = this.lastEvent || Date.now();
    this.throttle = this.throttle || 200;
    this.timerId && clearTimeout(this.timerId);
    if (Date.now() - this.lastEvent > this.throttle) {
      this.lastEvent = Date.now();
      this.sendCommand(command, value);
    } else {
      this.timerId = setTimeout(() => {
        this.lastEvent = Date.now();
        this.sendCommand(command, value);
      }, this.throttle - (Date.now() - this.lastEvent));
    }
  }
  getSchedule() {
    getSchedule(this);
  }
  addColorModelsButtons(models) {
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
        await this.sendCommand(
          "BP" +
            model.slice(0, 4).join(",") +
            ",BV" +
            model.slice(4, 8).join(",")
        );
      };
      modelsElement.append(button);
    }
  }

  async getCurrentLineColor() {
    let parameters = this.parameters.slice(52).map((p) => p.value);
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

  async getCurrentSettings() {
    let parameters = this.parameters.slice(0, 52).map((p) => p.value);
    const withIntensities =
      "BA" +
      parameters.slice(0, 8).join(",") +
      ",BK" +
      parameters.slice(10, 13).join(",");
    const withoutIntensities =
      "BB" +
      parameters.slice(1, 7).join(",") +
      ",BK" +
      parameters.slice(10, 13).join(",");
    document.getElementById(
      "result"
    ).value = `With intensities: ${withIntensities}\nWithout intensities: ${withoutIntensities}`;
    return withIntensities;
  }
}

function download(data) {
  const blob = new Blob([data], { type: "text/csv" });
  const elem = window.document.createElement("a");
  elem.href = window.URL.createObjectURL(blob);
  elem.download = Date.now() + ".csv";
  document.body.appendChild(elem);
  elem.click();
  document.body.removeChild(elem);
}

async function updateGIFList() {
  const results = await sendCommand("fd");
  const lines = results
    .split("\n")
    .filter((line) => line.includes(".gif") && !line.includes("weather"));
  const gifs = lines.map((line) => {
    const fields = line.trim().split(" ");
    return {
      size: fields[0],
      filename: fields[1],
      name: fields[1].replace(/.*\//, ""),
    };
  });
  const buttons = gifs
    .map((gif) => `<button value="${gif.filename}">${gif.name}</button>`)
    .join("");
  document.getElementById("gifButtons").innerHTML = buttons;
  setGIF(gifs[0].filename);
}

function setGIF(gif) {
  sendCommand("pg" + gif);
  console.log({ gif });
}

async function getSchedule(pixels) {
  console.table(pixels.parameters.slice(78));
  const parameters = pixels.parameters.slice(78);
  const lines = [];
  const dayNight = { 1: "Day", 2: "Night", 3: "Day and Night" };
  lines.push(
    "On during: " +
      (dayNight[parameters[1].value] ? dayNight[parameters[1].value] : "Never")
  );
  lines.push("Sunset offset: " + parameters[2].value + " minutes");
  lines.push("Sunrise offset: " + parameters[3].value + " minutes");
  for (let i = 4; i < 12; i++) {
    const value = parameters[i].value;
    const label = "C" + String.fromCharCode(i + 65);
    if (value < 0) {
      lines.push(label + ": No action");
    } else {
      const intensity = Math.floor(value / 2000);
      const minutes = value % 2000;
      const hours = Math.floor(minutes / 60);
      const minutesLeft = minutes % 60;
      lines.push(
        label + `: At ${hours}h${minutesLeft}m, intensity ${intensity}`
      );
    }
  }
  document.getElementById("result").value = lines.join("\n");
}

async function setServo1(status) {
  const settings = await getParameters();
  const off = settings[19];
  const on = settings[20];
  sendCommand("I" + (status ? on : off));
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
function colorHexTo12(value) {
  const rgb = value.match(/[A-Za-z0-9]{2}/g).map((v) => parseInt(v, 16));
  const color =
    ((rgb[0] >> 4) << 8) + ((rgb[1] >> 4) << 4) + ((rgb[2] >> 4) << 0);
  return color;
}

async function delay(millis) {
  return new Promise((resolve) => {
    setTimeout(resolve, millis);
  });
}
