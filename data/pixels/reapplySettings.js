export async function reapplySettings(parameters) {
  const elements = document.querySelectorAll("[data-label]");
  for (const element of elements) {
    const code = element.getAttribute("data-label").split(",")[0];
    const parameter = parameters.find((p) => p.code === code);
    if (!parameter) continue;
    switch (element.getAttribute("type")) {
      case "radio":
        if (parameter.value) {
          element.setAttribute("checked", "checked");
        } else {
          element.removeAttribute("checked");
        }
        break;
      case "color":
        // check if data-rgb
        if (element.getAttribute("data-rgb")) {
          //element.value = parameter.value;
        } else {
          element.value = color12ToHex(parameter.value);
        }
        break;
      default:
        element.value =
          parameter.value > 32767 ? parameter.value - 65536 : parameter.value;
    }
  }

  // color button (in KLM)
  /*
  let elements = document.querySelectorAll(`[data-label="KLM"]`);
  for (let element of elements) {
    element.value =
      "#" + result.substr(42, 2) + result.substr(46, 2) + result.substr(50, 2);
  }
      */
}

export function color12ToHex(value) {
  const result =
    "#" +
    ("00" + Number((value & 0xf00) >> 4).toString(16)).slice(-2) +
    ("00" + Number((value & 0x0f0) >> 0).toString(16)).slice(-2) +
    ("00" + Number((value & 0x00f) << 4).toString(16)).slice(-2);
  return result;
}
