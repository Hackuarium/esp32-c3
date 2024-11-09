export async function reapplySettings(cachedParameters) {
  const elements = document.querySelectorAll("[data-label]");
  for (const element of elements) {
    const codes = element.getAttribute("data-label").split(",");
    const parameters = codes
      .map((code) => cachedParameters.find((p) => p.code === code))
      .filter((p) => p);
    if (parameters.length === 0) continue;
    const firstParameter = parameters[0];
    switch (element.getAttribute("type")) {
      case "radio":
        if (firstParameter.value) {
          element.setAttribute("checked", "checked");
        } else {
          element.removeAttribute("checked");
        }
        break;
      case "color":
        // check if data-rgb
        if (element.getAttribute("data-rgb")) {
          // full RGB value on 3 parameters, value from 0 to 255
          element.value =
            "#" +
            parameters
              .map((p) => p.value.toString(16).padStart(2, "0"))
              .join("");
        } else {
          element.value = color12ToHex(firstParameter.value);
        }
        break;
      default:
        element.value =
          firstParameter.value > 32767
            ? firstParameter.value - 65536
            : firstParameter.value;
    }
  }
}

export function color12ToHex(value) {
  const result =
    "#" +
    ("00" + Number((value & 0xf00) >> 4).toString(16)).slice(-2) +
    ("00" + Number((value & 0x0f0) >> 0).toString(16)).slice(-2) +
    ("00" + Number((value & 0x00f) << 4).toString(16)).slice(-2);
  return result;
}
