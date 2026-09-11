//! Pure G-code parser for the GRBL workspace preview.
//!
//! `parse_gcode` turns an exported `.nc` (or any GRBL-dialect file)
//! into structured moves for the preview renderer: rapids, feeds,
//! G2/G3 arcs with I/J centers, dwells (pierce markers), and the
//! effective laser state per move.  Coordinates are left in the
//! file's machine space so the preview overlays GRBL's `MPos` 1:1 —
//! PolySmith posts are already WCS-shifted.
//!
//! Line filtering must byte-match `gcode_sender.rs` (trim, drop blank
//! lines and `(`-prefixed comment lines) so `move.line` maps exactly
//! onto the sender's `linesSent` progress counter.  `parse_gcode` is
//! pure and unit-tested — the project's first Rust test module.

use serde::Serialize;

#[derive(Debug, Clone, Copy, PartialEq, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum GcodeMoveKind {
    Rapid,
    Feed,
    ArcCw,
    ArcCcw,
    Dwell,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GcodeMove {
    /// Sequential move index (0-based) across the filtered stream.
    pub index: usize,
    /// 1-based index in the FILTERED line stream — equals the sender's
    /// `linesSent` counter for the corresponding streamed line.
    pub line: usize,
    pub kind: GcodeMoveKind,
    pub start: [f64; 3],
    pub end: [f64; 3],
    /// Arc center (G2/G3, I/J offsets from start).
    pub center: Option<[f64; 3]>,
    pub radius: Option<f64>,
    /// mm/min (G20 inches are scaled to mm).
    pub feed: Option<f64>,
    /// Last S value seen (raw controller units).
    pub power: Option<f64>,
    /// Effective laser state during the move.
    pub laser_on: bool,
    /// G4 dwell in seconds (pierce marker).
    pub dwell_seconds: Option<f64>,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GcodeBounds {
    pub min_x: f64,
    pub min_y: f64,
    pub min_z: f64,
    pub max_x: f64,
    pub max_y: f64,
    pub max_z: f64,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GcodeFileInfo {
    pub file_name: String,
    pub moves: Vec<GcodeMove>,
    pub bounds: Option<GcodeBounds>,
    pub units_mm: bool,
    pub absolute: bool,
    pub warnings: Vec<String>,
}

/// Removes inline `(...)` comments and a trailing `;` comment.
/// Unbalanced `(` truncates the line (GRBL treats a leading `(` as a
/// comment line; mid-line unbalanced parens are vanishingly rare).
fn strip_inline_comments(line: &str) -> String {
    let mut result = String::with_capacity(line.len());
    let mut chars = line.chars().peekable();
    while let Some(c) = chars.next() {
        if c == '(' {
            let mut depth = 1usize;
            let mut closed = false;
            for inner in chars.by_ref() {
                if inner == '(' {
                    depth += 1;
                } else if inner == ')' {
                    depth -= 1;
                    if depth == 0 {
                        closed = true;
                        break;
                    }
                }
            }
            if !closed {
                break; // truncate at the unclosed comment
            }
        } else if c == ';' {
            break; // to end of line
        } else {
            result.push(c);
        }
    }
    result
}

/// The sender (gcode_sender.rs) streams `trim`med lines, skipping
/// empty ones and `(`-prefixed full-line comments.  The preview
/// parser applies the identical filter so `line` indices match
/// `linesSent` exactly.
pub fn filter_lines(text: &str) -> Vec<String> {
    text.lines()
        .map(str::trim)
        .filter(|line| !line.is_empty() && !line.starts_with('('))
        .map(str::to_string)
        .collect()
}

/// One word of a G-code line: letter + numeric value.
fn parse_words(line: &str) -> Vec<(char, f64)> {
    let mut words = Vec::new();
    let mut letter: Option<char> = None;
    let mut digits = String::new();
    let flush = |letter: &mut Option<char>, digits: &mut String,
                 words: &mut Vec<(char, f64)>| {
        if let Some(l) = letter.take() {
            if !digits.is_empty() {
                if let Ok(value) = digits.parse::<f64>() {
                    words.push((l, value));
                }
            }
            digits.clear();
        }
    };
    for c in line.chars() {
        if c.is_ascii_alphabetic() {
            flush(&mut letter, &mut digits, &mut words);
            letter = Some(c.to_ascii_uppercase());
        } else if c.is_ascii_digit() || c == '.' || c == '-' || c == '+' {
            if letter.is_some() {
                digits.push(c);
            }
        } else {
            flush(&mut letter, &mut digits, &mut words);
        }
    }
    flush(&mut letter, &mut digits, &mut words);
    words
}

/// Parses a full G-code program into structured moves.  Pure.
pub fn parse_gcode(text: &str) -> GcodeFileInfo {
    let mut moves: Vec<GcodeMove> = Vec::new();
    let mut warnings: Vec<String> = Vec::new();
    let warn_once = |warnings: &mut Vec<String>, message: &str| {
        if !warnings.iter().any(|w| w == message) {
            warnings.push(message.to_string());
        }
    };

    let mut position = [0.0f64; 3];
    let mut absolute = true;
    let mut units_mm = true;
    let mut current_feed: Option<f64> = None;
    let mut current_power: Option<f64> = None;
    let mut laser_on = false;

    // Bounds tracking: the initial position counts once any move
    // exists.
    let mut min = [f64::INFINITY; 3];
    let mut max = [f64::NEG_INFINITY; 3];
    let mut have_move = false;
    fn track(
        have_move: &mut bool,
        min: &mut [f64; 3],
        max: &mut [f64; 3],
        p: [f64; 3],
    ) {
        *have_move = true;
        for axis in 0..3 {
            min[axis] = min[axis].min(p[axis]);
            max[axis] = max[axis].max(p[axis]);
        }
    }

    let mut filtered_line: usize = 0;

    for raw in filter_lines(text) {
        filtered_line += 1;
        let line = strip_inline_comments(&raw);
        // Strip a leading N-line-number word ("N10 G0 X…").
        let mut words = parse_words(&line);
        if matches!(words.first(), Some(('N', _))) {
            words.remove(0);
        }
        if words.is_empty() {
            continue;
        }

        let word_value = |words: &[(char, f64)], letter: char| -> Option<f64> {
            words.iter().find(|(l, _)| *l == letter).map(|(_, v)| *v)
        };

        let mut kind: Option<GcodeMoveKind> = None;
        let mut dwell_seconds: Option<f64> = None;

        for (letter, value) in &words {
            match letter {
                'G' => {
                    let code = *value as i64;
                    match code {
                        0 => kind = Some(GcodeMoveKind::Rapid),
                        1 => kind = Some(GcodeMoveKind::Feed),
                        2 => kind = Some(GcodeMoveKind::ArcCw),
                        3 => kind = Some(GcodeMoveKind::ArcCcw),
                        4 => {
                            // Dwell — P seconds; no motion.
                            if let Some(p) = word_value(&words, 'P') {
                                dwell_seconds = Some(p);
                            }
                        }
                        20 => {
                            units_mm = false;
                            warn_once(
                                &mut warnings,
                                "The file switches to inches (G20) — coordinates are converted to mm for the preview.",
                            );
                        }
                        21 => units_mm = true,
                        90 => absolute = true,
                        91 => {
                            absolute = false;
                            warn_once(
                                &mut warnings,
                                "The file uses relative moves (G91) — applied as deltas.",
                            );
                        }
                        92 => {
                            warn_once(
                                &mut warnings,
                                "G92 coordinate offsets are ignored — the machine-position overlay may drift.",
                            );
                        }
                        _ => {}
                    }
                }
                'M' => {
                    let code = *value as i64;
                    match code {
                        3 | 4 => laser_on = true,
                        5 => laser_on = false,
                        2 | 30 => {
                            // Program end — stop parsing.
                            // Handled after move emission below.
                        }
                        _ => {}
                    }
                }
                'S' => current_power = Some(*value),
                'F' => {
                    // Feed in the file's units (G20 = inch/min).
                    current_feed =
                        Some(*value * if units_mm { 1.0 } else { 25.4 });
                }
                _ => {}
            }
        }

        let motion_kind = kind.or_else(|| {
            if dwell_seconds.is_some() {
                Some(GcodeMoveKind::Dwell)
            } else {
                None
            }
        });
        // Coordinate scale: G20 files work in inches — convert to mm
        // so the preview and MPos overlay stay metric.  Computed after
        // the word loop so a same-line G20/G91 applies.
        let scale = if units_mm { 1.0 } else { 25.4 };
        let end = {
            let mut end = position;
            for axis in 0..3 {
                let letter = ['X', 'Y', 'Z'][axis];
                if let Some(value) = word_value(&words, letter) {
                    end[axis] = if absolute {
                        value * scale
                    } else {
                        position[axis] + value * scale
                    };
                }
            }
            end
        };

        match motion_kind {
            Some(GcodeMoveKind::Dwell) => {
                let m = GcodeMove {
                    index: moves.len(),
                    line: filtered_line,
                    kind: GcodeMoveKind::Dwell,
                    start: position,
                    end: position,
                    center: None,
                    radius: None,
                    feed: None,
                    power: current_power,
                    laser_on,
                    dwell_seconds,
                };
                track(&mut have_move, &mut min, &mut max, position);
                moves.push(m);
            }
            Some(GcodeMoveKind::Rapid) | Some(GcodeMoveKind::Feed) => {
                if end != position {
                    let m = GcodeMove {
                        index: moves.len(),
                        line: filtered_line,
                        kind: motion_kind.unwrap(),
                        start: position,
                        end,
                        center: None,
                        radius: None,
                        feed: match motion_kind {
                            Some(GcodeMoveKind::Feed) => current_feed,
                            _ => None,
                        },
                        power: current_power,
                        laser_on,
                        dwell_seconds: None,
                    };
                    track(&mut have_move, &mut min, &mut max, end);
                    moves.push(m);
                    position = end;
                }
            }
            Some(kind @ (GcodeMoveKind::ArcCw | GcodeMoveKind::ArcCcw)) => {
                // I/J offsets from the start; R-style arcs unsupported
                // (warn + skip).
                if word_value(&words, 'R').is_some() {
                    warn_once(
                        &mut warnings,
                        "R-style arcs are not supported — those moves are skipped in the preview.",
                    );
                } else {
                    let i = word_value(&words, 'I').unwrap_or(0.0) * scale;
                    let j = word_value(&words, 'J').unwrap_or(0.0) * scale;
                    let arc_center = [position[0] + i, position[1] + j, position[2]];
                    let arc_radius = (i * i + j * j).sqrt();
                    // Full circles (start == end) are valid arcs; only
                    // reject when there is no center either.
                    if arc_radius > 0.0 || end != position {
                        let m = GcodeMove {
                            index: moves.len(),
                            line: filtered_line,
                            kind,
                            start: position,
                            end,
                            center: Some(arc_center),
                            radius: if arc_radius > 0.0 { Some(arc_radius) } else { None },
                            feed: current_feed,
                            power: current_power,
                            laser_on,
                            dwell_seconds: None,
                        };
                        track(&mut have_move, &mut min, &mut max, end);
                        moves.push(m);
                        position = end;
                    }
                }
            }
            None => {}
        }

        // Program end terminates the loop (M2/M30).
        if words.iter().any(|(l, v)| *l == 'M' && (*v == 2.0 || *v == 30.0)) {
            break;
        }
    }

    let bounds = if have_move {
        Some(GcodeBounds {
            min_x: min[0],
            min_y: min[1],
            min_z: min[2],
            max_x: max[0],
            max_y: max[1],
            max_z: max[2],
        })
    } else {
        None
    };

    GcodeFileInfo {
        file_name: String::new(),
        moves,
        bounds,
        units_mm,
        absolute,
        warnings,
    }
}

#[tauri::command]
pub fn grbl_parse_file(file_path: String) -> Result<GcodeFileInfo, String> {
    let text = std::fs::read_to_string(&file_path)
        .map_err(|error| format!("Could not read {file_path}: {error}"))?;
    let file_name = std::path::Path::new(&file_path)
        .file_name()
        .map(|name| name.to_string_lossy().to_string())
        .unwrap_or_default();
    let mut info = parse_gcode(&text);
    info.file_name = file_name;
    Ok(info)
}

#[cfg(test)]
mod tests {
    use super::*;

    // The real exported file the user verified (res/untitled-part.nc,
    // two regions: a circle + a rectangle, grbl post).
    const GOLDEN_NC: &str = "\n\
(operation: 2D Cut)\n\
(op: 2D Cut)\n\
G21\n\
G90\n\
G94\n\
G17\n\
M5\n\
G0 X11.736 Y0.000\n\
M4 S850.000\n\
G1 X11.736 Y0.000 F500.000\n\
G4 P0.100\n\
G1 X13.736 Y0.000\n\
G2 X13.736 Y0.000 I-13.736 J0.000\n\
G1 X11.736 Y0.000\n\
M5\n\
G0 X52.000 Y35.075\n\
M4 S850.000\n\
G1 X52.000 Y35.075\n\
G4 P0.100\n\
G1 X50.000 Y35.075\n\
G1 X-50.000 Y35.075\n\
G3 X-50.075 Y35.000 I0.000 J-0.075\n\
G1 X-50.075 Y-35.000\n\
G3 X-50.000 Y-35.075 I0.075 J0.000\n\
G1 X50.000 Y-35.075\n\
G3 X50.075 Y-35.000 I0.000 J0.075\n\
G1 X50.075 Y35.000\n\
G3 X50.000 Y35.075 I-0.075 J0.000\n\
G1 X48.000 Y35.075\n\
M5\n\
M2\n";

    fn assert_near(a: f64, b: f64, eps: f64, what: &str) {
        assert!(
            (a - b).abs() < eps,
            "{what}: expected {b}, got {a}"
        );
    }

    #[test]
    fn filter_parity_matches_sender() {
        let lines = filter_lines(GOLDEN_NC);
        // 29 filtered lines: blanks + 2 comment lines dropped,
        // everything else kept verbatim (trimmed).
        assert_eq!(lines.len(), 29, "filtered line count");
        assert_eq!(lines[0], "G21");
        assert_eq!(lines[2], "G94");
        assert_eq!(lines[4], "M5");
        assert!(lines.iter().all(|l| !l.is_empty() && !l.starts_with('(')));
    }

    #[test]
    fn golden_file_parses_to_expected_moves() {
        let info = parse_gcode(GOLDEN_NC);
        assert!(info.warnings.is_empty(), "no warnings: {:?}", info.warnings);
        assert_eq!(info.moves.len(), 17, "move count");

        let kinds: Vec<GcodeMoveKind> = info.moves.iter().map(|m| m.kind).collect();
        assert_eq!(
            kinds,
            vec![
                GcodeMoveKind::Rapid,
                GcodeMoveKind::Dwell,
                GcodeMoveKind::Feed,
                GcodeMoveKind::ArcCw,
                GcodeMoveKind::Feed,
                GcodeMoveKind::Rapid,
                GcodeMoveKind::Dwell,
                GcodeMoveKind::Feed,
                GcodeMoveKind::Feed,
                GcodeMoveKind::ArcCcw,
                GcodeMoveKind::Feed,
                GcodeMoveKind::ArcCcw,
                GcodeMoveKind::Feed,
                GcodeMoveKind::ArcCcw,
                GcodeMoveKind::Feed,
                GcodeMoveKind::ArcCcw,
                GcodeMoveKind::Feed,
            ],
            "kind sequence"
        );

        let first = &info.moves[0];
        assert_near(first.start[0], 0.0, 1e-9, "first start x");
        assert_near(first.end[0], 11.736, 1e-9, "first end x");

        // The full-circle arc: center (0,0), radius 13.736.
        let circle = &info.moves[3];
        assert_eq!(circle.kind, GcodeMoveKind::ArcCw);
        let c = circle.center.unwrap();
        assert_near(c[0], 0.0, 1e-9, "circle center x");
        assert_near(c[1], 0.0, 1e-9, "circle center y");
        assert_near(circle.radius.unwrap(), 13.736, 1e-6, "circle radius");
        assert_near(circle.end[0], circle.start[0], 1e-9, "full circle end x");

        // Laser state: feeds after M4 run laser_on; after M5 off.
        assert!(info.moves[2].laser_on, "feed after M4 is laser on");
        assert!(!info.moves[5].laser_on, "rapid after M5 is laser off");

        let b = info.bounds.unwrap();
        assert_near(b.min_x, -50.075, 1e-6, "bounds min x");
        assert_near(b.max_x, 52.0, 1e-6, "bounds max x");
        assert_near(b.min_y, -35.075, 1e-6, "bounds min y");
        assert_near(b.max_y, 35.075, 1e-6, "bounds max y");
        assert_near(b.min_z, 0.0, 1e-9, "bounds min z");
        assert_near(b.max_z, 0.0, 1e-9, "bounds max z");
    }

    #[test]
    fn arc_offsets_and_dwell() {
        let text = "G21 G90\nG1 X10 Y0 F600\nG3 X10 Y10 I0 J5\nG4 P0.2\nM5\nM2\n";
        let info = parse_gcode(text);
        assert!(info.warnings.is_empty());
        assert_eq!(info.moves.len(), 3, "feed + arc + dwell");
        let arc = &info.moves[1];
        assert_eq!(arc.kind, GcodeMoveKind::ArcCcw);
        let c = arc.center.unwrap();
        assert_near(c[0], 10.0, 1e-9, "arc center x");
        assert_near(c[1], 5.0, 1e-9, "arc center y");
        assert_near(arc.radius.unwrap(), 5.0, 1e-9, "arc radius");
        assert_near(arc.feed.unwrap(), 600.0, 1e-9, "arc feed");
        let dwell = &info.moves[2];
        assert_eq!(dwell.kind, GcodeMoveKind::Dwell);
        assert_near(dwell.dwell_seconds.unwrap(), 0.2, 1e-9, "dwell seconds");
    }

    #[test]
    fn relative_moves_apply_deltas_with_warning() {
        let text = "G21 G91\nG1 X10 Y5\nG1 X-2\n";
        let info = parse_gcode(text);
        assert_eq!(info.moves.len(), 2);
        assert_near(info.moves[0].end[0], 10.0, 1e-9, "delta x");
        assert_near(info.moves[0].end[1], 5.0, 1e-9, "delta y");
        assert_near(info.moves[1].end[0], 8.0, 1e-9, "second delta x");
        assert!(info.warnings.iter().any(|w| w.contains("G91")));
        assert!(!info.absolute);
    }

    #[test]
    fn inch_mode_scales_to_mm_with_warning() {
        let text = "G20 G90\nG1 X1 Y1 F10\n";
        let info = parse_gcode(text);
        assert_eq!(info.moves.len(), 1);
        assert_near(info.moves[0].end[0], 25.4, 1e-9, "inch x in mm");
        assert_near(info.moves[0].feed.unwrap(), 254.0, 1e-9, "inch feed in mm/min");
        assert!(info.warnings.iter().any(|w| w.contains("G20")));
        assert!(!info.units_mm);
    }

    #[test]
    fn r_arcs_warn_and_skip() {
        let text = "G21 G90\nG0 X2 Y0\nG1 X10 Y0\nG2 X10 Y10 R5\nM2\n";
        let info = parse_gcode(text);
        assert_eq!(info.moves.len(), 2, "R arc skipped, feeds kept");
        assert!(info.warnings.iter().any(|w| w.contains("R-style")));
    }

    #[test]
    fn line_numbers_and_inline_comments_are_stripped() {
        let text = "N10 G21\nN20 G90\nN30 G0 X1 (go) Y2 ; trailing\nN40 G1 X3 Y4\n";
        let info = parse_gcode(text);
        assert_eq!(info.moves.len(), 2);
        assert_near(info.moves[0].end[0], 1.0, 1e-9, "rapid x");
        assert_near(info.moves[0].end[1], 2.0, 1e-9, "rapid y");
        assert_near(info.moves[1].end[0], 3.0, 1e-9, "feed x");
    }
}
