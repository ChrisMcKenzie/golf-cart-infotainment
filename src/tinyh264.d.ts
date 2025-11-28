declare module 'tinyh264' {
  export function init(): Promise<H264Decoder>;

  export interface H264Decoder {
    decode(data: Uint8Array): void;
    onPictureDecoded?: (buffer: Uint8Array, width: number, height: number) => void;
  }
}
