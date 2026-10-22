// Decode the binary viewport geometry blobs (phase E4).  The core ships mesh
// positions/normals as base64 float32 and indices as base64 uint32; these decode
// straight into typed arrays so the renderer can hand them to BufferGeometry
// without a parse-and-widen pass.

function decodeBase64(base64: string): Uint8Array {
  if (!base64) {
    return new Uint8Array(0);
  }
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

export function decodeF32(base64: string): Float32Array {
  const bytes = decodeBase64(base64);
  return new Float32Array(bytes.buffer, 0, Math.floor(bytes.byteLength / 4));
}

export function decodeU32(base64: string): Uint32Array {
  const bytes = decodeBase64(base64);
  return new Uint32Array(bytes.buffer, 0, Math.floor(bytes.byteLength / 4));
}
