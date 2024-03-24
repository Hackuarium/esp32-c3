
import { readdirSync } from "fs";

import { Image, copyTo, read, write } from "image-js";

let files = readdirSync(new URL('pngs', import.meta.url), { withFileTypes: true })


//files = files.filter(file => file.name === "78.png")

for (const file of files) {
  if (!file.isFile() || file.name.startsWith('.')) continue
  let image = (await read(file.path + '/' + file.name))

  if (image.colorModel !== 'RGB') {
    image = image.convertColor('RGB')
  }

  if (image.width !== 16 || image.height !== 16) {
    // need to resize the image to 16x16
    const newImage = new Image(16, 16, { origin: { row: ((image.height - 16) / 2) >> 0, column: ((image.width - 16) / 2) >> 0 } })
    image = copyTo(image, newImage)
  }

  for (let row = 0; row < image.height; row++) {
    for (let col = 0; col < image.width; col++) {
      const pixel = image.getPixel(col, row)
      const [r, g, b] = pixel
      if (Math.abs(r - g) < 12 && Math.abs(r - b) < 12 && Math.abs(g - b) < 12) {
        continue
      }
      if (b < r || b < g) {
        continue
      }
      image.setPixel(col, row, [0, g / 4, b])
    }
  }
  write(file.path + 'Out' + '/' + file.name, image)

}