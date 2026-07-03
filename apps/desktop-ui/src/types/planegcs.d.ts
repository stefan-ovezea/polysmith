declare module "@salusoft89/planegcs" {
  export enum Algorithm {
    BFGS = 0,
    LevenbergMarquardt = 1,
    DogLeg = 2,
  }

  export interface GcsWrapper {
    clear_data(): void;
    push_primitive(primitive: Record<string, unknown>): void;
    set_max_iterations(maxIterations: number): void;
    set_convergence_threshold(threshold: number): void;
    solve(algorithm: Algorithm): number;
    apply_solution(): void;
    get_gcs_params(): number[];
    get_gcs_conflicting_constraints(): string[];
    get_gcs_redundant_constraints(): string[];
    destroy_gcs_module(): void;
  }

  export function make_gcs_wrapper(wasmPath?: string): Promise<GcsWrapper>;
}

declare module "@salusoft89/planegcs/dist/planegcs_dist/planegcs.wasm?url" {
  const url: string;
  export default url;
}
