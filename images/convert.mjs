

import { writeFileSync, readFileSync } from 'fs';

const data = readFileSync(new URL('campfire.gif', import.meta.url));

console.log(data.byteLength)

const array = Array.from(new Uint8Array(data))


const hex = array.map((x) => '0x' + Number(x).toString(16)).join(',').replace(/'/g, '');


writeFileSync(new URL('campfire.txt', import.meta.url), hex, 'utf8');

console.log(hex)
