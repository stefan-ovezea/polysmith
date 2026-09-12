//! Laser utility programs for the GRBL workspace.
//!
//! Small G-code snippets generated shell-side: a low-power framing
//! rectangle tracing the job bounds, and a focus pulse at the current
//! head position.  Both travel through the normal send pipeline
//! (`grbl_send_program`), so they preview, stream, and count progress
//! exactly like a CAM job.  Test fire is not a program — it is a raw
//! M3 S../M5 pair via `grbl_laser_power` in gcode_sender.rs.
//!
//! Pure generation functions — unit-tested against the shared parser.

use serde::{Deserialize, Serialize};

/// A request to generate a small laser utility program.
///
/// `rename_all` covers the VARIANT names; struct-variant FIELDS need
/// `rename_all_fields` — without it the UI's camelCase payload fails
/// to deserialize and every utility rejects ("missing field
/// `power_percent`").  Pinned by a regression test below.
#[derive(Debug, Clone, Deserialize)]
#[serde(tag = "kind", rename_all = "camelCase", rename_all_fields = "camelCase")]
pub enum GrblUtilityRequest {
    /// Framing: a low-power rectangle traced around the job bounds so
    /// the user can check placement before cutting.
    Framing {
        x: f64,
        y: f64,
        width: f64,
        height: f64,
        power_percent: f64,
        feed: f64,
    },
    /// Focus pulse: a short low-power pulse at the current position to
    /// aim the laser dot.
    FocusPulse {
        power_percent: f64,
        duration_seconds: f64,
    },
}

/// Scales a 0–100 % power into GRBL laser S units (0–255), clamped to
/// 1..255 so a 0 % request never means "laser off" by accident and
/// 100 % is full power.  Shared with the laser-power command.
pub fn scale_power(percent: f64) -> u32 {
    let scaled = (percent.clamp(0.0, 100.0) / 100.0 * 255.0).round() as i64;
    scaled.clamp(1, 255) as u32
}

/// Generates the utility program and its display label.
pub fn generate_utility_program(request: GrblUtilityRequest) -> (String, String) {
    match request {
        GrblUtilityRequest::Framing {
            x,
            y,
            width,
            height,
            power_percent,
            feed,
        } => {
            let s = scale_power(power_percent);
            let mut text = String::new();
            // Laser off first, absolute mode, then trace the rectangle
            // at low power and kill the beam on the closing corner.
            text.push_str("M5\nG90\n");
            text.push_str(&format!("G0 X{x:.3} Y{y:.3}\n"));
            text.push_str(&format!("M3 S{s}\n"));
            text.push_str(&format!("G1 X{x:.3} Y{y:.3} F{feed:.3}\n"));
            text.push_str(&format!("G1 X{:.3} Y{y:.3}\n", x + width));
            text.push_str(&format!("G1 X{:.3} Y{:.3}\n", x + width, y + height));
            text.push_str(&format!("G1 X{x:.3} Y{:.3}\n", y + height));
            text.push_str(&format!("G1 X{x:.3} Y{y:.3}\n"));
            text.push_str("M5\n");
            (text, "Framing box".to_string())
        }
        GrblUtilityRequest::FocusPulse {
            power_percent,
            duration_seconds,
        } => {
            let s = scale_power(power_percent);
            let mut text = String::new();
            text.push_str("M5\nG90\n");
            text.push_str(&format!("M3 S{s}\n"));
            text.push_str(&format!("G4 P{duration_seconds:.3}\n"));
            text.push_str("M5\n");
            (text, "Focus pulse".to_string())
        }
    }
}

/// Generated program returned to the UI.
#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GrblUtilityProgram {
    pub text: String,
    pub label: String,
}

#[tauri::command]
pub fn grbl_utility_program(request: GrblUtilityRequest) -> GrblUtilityProgram {
    let (text, label) = generate_utility_program(request);
    GrblUtilityProgram { text, label }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::gcode_parser::{parse_gcode, GcodeMoveKind};

    // The exact JSON shapes the TS client sends (grblClient.ts) — a
    // rename regression here breaks every utility in the workspace.
    #[test]
    fn request_deserializes_from_the_ui_payload() {
        let framing: GrblUtilityRequest = serde_json::from_str(
            r#"{"kind":"framing","x":10.0,"y":20.0,"width":100.0,"height":50.0,"powerPercent":2.0,"feed":1500.0}"#,
        )
        .expect("framing payload deserializes");
        assert!(matches!(
            framing,
            GrblUtilityRequest::Framing {
                power_percent: 2.0,
                ..
            }
        ));

        let pulse: GrblUtilityRequest = serde_json::from_str(
            r#"{"kind":"focusPulse","powerPercent":5.0,"durationSeconds":0.5}"#,
        )
        .expect("focus pulse payload deserializes");
        assert!(matches!(
            pulse,
            GrblUtilityRequest::FocusPulse {
                power_percent: 5.0,
                duration_seconds: 0.5,
            }
        ));
    }

    #[test]
    fn scale_power_clamps_and_maps() {
        assert_eq!(scale_power(0.0), 1, "0 % must not mean laser off");
        assert_eq!(scale_power(100.0), 255);
        assert_eq!(scale_power(50.0), 128);
        assert_eq!(scale_power(200.0), 255, "over-range clamps to full power");
        assert_eq!(scale_power(-5.0), 1);
    }

    #[test]
    fn framing_program_parses_to_four_feed_moves() {
        let (text, label) = generate_utility_program(GrblUtilityRequest::Framing {
            x: 10.0,
            y: 20.0,
            width: 100.0,
            height: 50.0,
            power_percent: 2.0,
            feed: 1500.0,
        });
        assert_eq!(label, "Framing box");
        let info = parse_gcode(&text);
        let feeds = info
            .moves
            .iter()
            .filter(|m| m.kind == GcodeMoveKind::Feed)
            .count();
        assert_eq!(feeds, 4, "four rectangle edges: {text}");
        let bounds = info.bounds.expect("framing has bounds");
        assert!((bounds.min_x - 10.0).abs() < 1e-6);
        assert!((bounds.max_x - 110.0).abs() < 1e-6);
        assert!((bounds.min_y - 20.0).abs() < 1e-6);
        assert!((bounds.max_y - 70.0).abs() < 1e-6);
        // The approach rapid travels beam-off; every cutting edge fires.
        assert!(info.moves.iter().all(|m| {
            m.kind != GcodeMoveKind::Feed || m.laser_on
        }), "every feed move fires the laser");
        assert!(info.moves.iter().any(|m| {
            m.kind == GcodeMoveKind::Rapid && !m.laser_on
        }), "the approach rapid travels beam-off");
    }

    #[test]
    fn focus_pulse_parses_to_dwell_with_laser_on() {
        let (text, label) = generate_utility_program(GrblUtilityRequest::FocusPulse {
            power_percent: 5.0,
            duration_seconds: 0.5,
        });
        assert_eq!(label, "Focus pulse");
        let info = parse_gcode(&text);
        let dwells: Vec<_> = info
            .moves
            .iter()
            .filter(|m| m.kind == GcodeMoveKind::Dwell)
            .collect();
        assert_eq!(dwells.len(), 1, "one G4 dwell: {text}");
        assert!(dwells[0].laser_on, "the dwell fires the laser");
        assert!(
            dwells[0]
                .dwell_seconds
                .is_some_and(|seconds| (seconds - 0.5).abs() < 1e-6),
            "dwell is 0.5 s"
        );
    }
}
