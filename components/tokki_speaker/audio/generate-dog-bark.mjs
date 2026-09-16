import { readFileSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { createHash } from "node:crypto";

const [sourcePath, outputPath] = process.argv.slice(2);
if (!sourcePath || !outputPath || resolve(sourcePath) === resolve(outputPath)) {
  throw new Error("Usage: node generate-dog-bark.mjs <source.wav> <different-output.wav>");
}
const source = readFileSync(sourcePath);
if (source.toString("ascii", 0, 4) !== "RIFF" ||
    source.toString("ascii", 8, 12) !== "WAVE" ||
    source.readUInt32LE(4) + 8 !== source.length) {
  throw new Error("Expected a complete RIFF/WAVE file");
}
let format;
let pcm;
for (let offset = 12; offset + 8 <= source.length;) {
  const id = source.toString("ascii", offset, offset + 4);
  const size = source.readUInt32LE(offset + 4);
  const start = offset + 8;
  const end = start + size;
  if (end + (size % 2) > source.length) throw new Error("Truncated WAV chunk");
  if (id === "fmt ") {
    if (size < 16 || format) throw new Error("Invalid or duplicate WAV format");
    format = {
      encoding: source.readUInt16LE(start), channels: source.readUInt16LE(start + 2),
      rate: source.readUInt32LE(start + 4), byteRate: source.readUInt32LE(start + 8),
      alignment: source.readUInt16LE(start + 12), bits: source.readUInt16LE(start + 14),
    };
  } else if (id === "data") {
    if (pcm) throw new Error("Multiple data chunks are unsupported");
    pcm = source.subarray(start, end);
  }
  offset = end + (size % 2);
}
if (!format || !pcm || format.encoding !== 1 || format.bits !== 16 ||
    ![1, 2].includes(format.channels) || format.rate < 8000 || format.rate > 192000 ||
    format.alignment !== format.channels * 2 || format.byteRate !== format.rate * format.alignment ||
    pcm.length === 0 || pcm.length % format.alignment !== 0) {
  throw new Error("Expected mono/stereo PCM16 with a valid 8-192 kHz sample rate");
}
const inputFrames = pcm.length / format.alignment;
if (inputFrames > format.rate * 5) throw new Error("Source bark must be at most five seconds");
const mono = new Float64Array(inputFrames);
for (let i = 0; i < inputFrames; i++) {
  for (let channel = 0; channel < format.channels; channel++) {
    mono[i] += pcm.readInt16LE(i * format.alignment + channel * 2) / format.channels;
  }
}

const rate = 16000;
const frames = Math.round(inputFrames * rate / format.rate);
const single = Buffer.alloc(frames * 2);
// Windowed-sinc low-pass filtering avoids aliasing when reducing the sample rate.
const cutoff = Math.min(1, rate / format.rate) * 0.94;
const radius = 48;
for (let i = 0; i < frames; i++) {
  const position = i * format.rate / rate;
  const center = Math.floor(position);
  let sum = 0;
  let weightSum = 0;
  for (let j = center - radius + 1; j <= center + radius; j++) {
    const distance = position - j;
    if (Math.abs(distance) >= radius) continue;
    const x = Math.PI * cutoff * distance;
    const sinc = Math.abs(x) < 1e-12 ? 1 : Math.sin(x) / x;
    const window = 0.42 + 0.5 * Math.cos(Math.PI * distance / radius) +
      0.08 * Math.cos(2 * Math.PI * distance / radius);
    const weight = cutoff * sinc * window;
    sum += mono[Math.max(0, Math.min(inputFrames - 1, j))] * weight;
    weightSum += weight;
  }
  const sample = Math.round(sum / weightSum);
  if (sample < -32768 || sample > 32767) {
    throw new Error("Resampling exceeded PCM16 range; reduce source amplitude before conversion");
  }
  single.writeInt16LE(sample, i * 2);
}
const repeated = Buffer.concat([single, single]);
if (!repeated.subarray(0, single.length).equals(repeated.subarray(single.length))) {
  throw new Error("Repeat verification failed");
}
const header = Buffer.alloc(44);
header.write("RIFF", 0);
header.writeUInt32LE(36 + repeated.length, 4);
header.write("WAVEfmt ", 8);
header.writeUInt32LE(16, 16);
header.writeUInt16LE(1, 20);
header.writeUInt16LE(1, 22);
header.writeUInt32LE(rate, 24);
header.writeUInt32LE(rate * 2, 28);
header.writeUInt16LE(2, 32);
header.writeUInt16LE(16, 34);
header.write("data", 36);
header.writeUInt32LE(repeated.length, 40);
writeFileSync(outputPath, Buffer.concat([header, repeated]));
console.log(JSON.stringify({
  sourceSha256: createHash("sha256").update(source).digest("hex"),
  sourceFrames: inputFrames, sourceRate: format.rate,
  outputFrames: frames * 2, sampleRate: rate, repetitions: 2,
  durationSeconds: frames * 2 / rate, outputPath: resolve(outputPath),
}, null, 2));
