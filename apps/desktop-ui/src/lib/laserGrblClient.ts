import { invoke } from "@tauri-apps/api/core";

export interface LaserGrblLaunchRequest {
  binaryPath: string;
  gcodeFilePath?: string;
}

export interface LaserGrblLaunchResult {
  platform: string;
  processId: number;
  status: string;
  message: string;
}

export function launchLaserGrbl(
  request: LaserGrblLaunchRequest,
): Promise<LaserGrblLaunchResult> {
  return invoke("launch_laser_grbl", { request });
}
